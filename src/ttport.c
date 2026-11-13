/*
 * The Teletext WinHelp: port-specific helpers (not in Wine).
 *
 * 1. Saved window positions, as WinHelp 3.1 kept them in WIN.INI:
 *        [Windows Help]
 *        M_WindowPosition=[x,y,w,h,maximized]     main window
 *        H_WindowPosition=[x,y,w,h,maximized]     History window
 *    (format read from the real WIN.INI of the WfW 3.11 reference VM).
 *    MODERN ACCOMMODATION: Windows 11 does not let a user program write
 *    C:\Windows\WIN.INI (it would land in the UAC VirtualStore), so the same
 *    section and keys live in a private file,
 *    %APPDATA%\The Teletext\TTWINHLP.INI, or the path in TTWINHLP_INI.
 *
 * 2. Multi-monitor work areas. Wine's code clamps popups to
 *    0..SM_CXSCREEN, i.e. the primary monitor; on a desk with a monitor left
 *    of the primary (negative x) that throws every popup onto the wrong
 *    screen. MODERN ACCOMMODATION: Windows 3.1 had one screen; we clamp to
 *    the work area of the monitor the parent window is on.
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>
#include "ttport.h"

static char tt_ini[MAX_PATH];

const char *TT_IniPath(void)
{
    const char *env;
    if (tt_ini[0]) return tt_ini;
    env = getenv("TTWINHLP_INI");
    if (env && *env)
    {
        lstrcpynA(tt_ini, env, sizeof(tt_ini));
        return tt_ini;
    }
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, tt_ini)))
    {
        lstrcatA(tt_ini, "\\The Teletext");
        CreateDirectoryA(tt_ini, NULL);
        lstrcatA(tt_ini, "\\TTWINHLP.INI");
    }
    else lstrcpyA(tt_ini, "TTWINHLP.INI");
    return tt_ini;
}

/* Reads "[x,y,w,h,max]". Returns TRUE only for a sane rectangle that is at
 * least partly on some monitor (a position saved on a monitor that has since
 * gone away must not put the window off screen). */
BOOL TT_LoadWindowPos(const char *key, RECT *rc, BOOL *maximized)
{
    char buf[64];
    int x, y, w, h, m = 0;
    RECT test;
    if (!GetPrivateProfileStringA("Windows Help", key, "", buf, sizeof(buf), TT_IniPath()))
        return FALSE;
    if (sscanf(buf, " [ %d , %d , %d , %d , %d ]", &x, &y, &w, &h, &m) < 4) return FALSE;
    if (w < 50 || h < 50) return FALSE;
    SetRect(&test, x, y, x + w, y + h);
    if (!MonitorFromRect(&test, MONITOR_DEFAULTTONULL)) return FALSE;
    *rc = test;
    if (maximized) *maximized = m != 0;
    return TRUE;
}

void TT_SaveWindowPos(const char *key, HWND hwnd)
{
    WINDOWPLACEMENT wp;
    char buf[64];
    RECT r;
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp)) return;
    /* rcNormalPosition is in workspace coordinates; GetWindowRect is in screen
     * coordinates. Use the screen rect unless minimized or maximized. */
    if (wp.showCmd == SW_SHOWNORMAL && GetWindowRect(hwnd, &r))
        ;
    else
        r = wp.rcNormalPosition;
    snprintf(buf, sizeof(buf), "[%ld,%ld,%ld,%ld,%d]", r.left, r.top,
             r.right - r.left, r.bottom - r.top, wp.showCmd == SW_SHOWMAXIMIZED);
    WritePrivateProfileStringA("Windows Help", key, buf, TT_IniPath());
}

/* 3. Dialog placement. Wine's message boxes are unowned, and unowned dialogs
 * open centred on the PRIMARY monitor. MODERN ACCOMMODATION: a per-thread
 * CBT hook catches every top-level dialog (#32770: message boxes, common
 * dialogs, the index property sheet) at creation, before it is shown, and
 * centres it on the monitor of the help session (a visible MS_WINHELP
 * window of this thread, else the saved main position). */
static HHOOK tt_cbt;

static BOOL CALLBACK tt_find_main(HWND h, LPARAM lp)
{
    char cls[32];
    if (IsWindowVisible(h) && GetClassNameA(h, cls, sizeof(cls)) && !lstrcmpA(cls, "MS_WINHELP"))
    {
        *(HWND *)lp = h;
        return FALSE;
    }
    return TRUE;
}

static HMONITOR tt_session_monitor(void)
{
    HWND main_wnd = NULL;
    RECT saved;
    EnumThreadWindows(GetCurrentThreadId(), tt_find_main, (LPARAM)&main_wnd);
    if (main_wnd) return MonitorFromWindow(main_wnd, MONITOR_DEFAULTTOPRIMARY);
    if (TT_LoadWindowPos("M_WindowPosition", &saved, NULL))
        return MonitorFromRect(&saved, MONITOR_DEFAULTTOPRIMARY);
    return NULL;
}

static LRESULT CALLBACK tt_cbt_proc(int code, WPARAM wp, LPARAM lp)
{
    if (code == HCBT_ACTIVATE)
    {
        /* fallback for dialogs that reposition themselves after creation */
        HWND h = (HWND)wp;
        char cls[16];
        RECT r;
        MONITORINFO mi;
        HMONITOR mon;
        if (GetClassNameA(h, cls, sizeof(cls)) && !lstrcmpA(cls, "#32770") &&
            !(GetWindowLongA(h, GWL_STYLE) & WS_CHILD) && GetWindowRect(h, &r) &&
            (mon = tt_session_monitor()) && MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != mon)
        {
            mi.cbSize = sizeof(mi);
            GetMonitorInfoA(mon, &mi);
            SetWindowPos(h, NULL,
                         mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - (r.right - r.left)) / 2,
                         mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - (r.bottom - r.top)) / 2,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    if (code == HCBT_CREATEWND)
    {
        CBT_CREATEWNDA *cw = (CBT_CREATEWNDA *)lp;
        CREATESTRUCTA *cs = cw->lpcs;
        if (cs && !(cs->style & WS_CHILD) && cs->lpszClass == (LPCSTR)MAKEINTATOM(32770) &&
            cs->cx > 0 && cs->cy > 0)
        {
            RECT wa;
            MONITORINFO mi;
            HMONITOR mon = tt_session_monitor();
            mi.cbSize = sizeof(mi);
            if (mon && GetMonitorInfoA(mon, &mi))
            {
                wa = mi.rcWork;
                /* leave dialogs that already sit on that monitor alone */
                POINT c = { cs->x + cs->cx / 2, cs->y + cs->cy / 2 };
                if (cs->x == CW_USEDEFAULT || !PtInRect(&wa, c))
                {
                    cs->x = wa.left + (wa.right - wa.left - cs->cx) / 2;
                    cs->y = wa.top + (wa.bottom - wa.top - cs->cy) / 2;
                }
            }
        }
    }
    return CallNextHookEx(tt_cbt, code, wp, lp);
}

void TT_InstallDialogPlacement(void)
{
    if (!tt_cbt) tt_cbt = SetWindowsHookExA(WH_CBT, tt_cbt_proc, NULL, GetCurrentThreadId());
}

void TT_WorkAreaFromWindow(HWND hwnd, RECT *wa)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        *wa = mi.rcWork;
    else
        SystemParametersInfoA(SPI_GETWORKAREA, 0, wa, 0);
}
