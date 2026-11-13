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

void TT_WorkAreaFromWindow(HWND hwnd, RECT *wa)
{
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        *wa = mi.rcWork;
    else
        SystemParametersInfoA(SPI_GETWORKAREA, 0, wa, 0);
}
