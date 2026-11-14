/*
 * Help Viewer
 *
 * Copyright    1996 Ulrich Schmid <uschmid@mail.hh.provi.de>
 *              2002 Sylvain Petreolle <spetreolle@yahoo.fr>
 *              2002, 2008 Eric Pouech <eric.pouech@wanadoo.fr>
 *              2004 Ken Belleau <jamez@ivic.qc.ca>
 *              2008 Kirill K. Smirnov <lich@math.spbu.ru>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "commdlg.h"
#include "winhelp.h"
#include "winhelp_res.h"
#include "shellapi.h"
#include "richedit.h"
#include "commctrl.h"
#include <ole2.h>
#include "ttport.h"     /* TELETEXT PORT: saved positions, monitor work areas */
#include "w31.h"        /* TELETEXT PORT (gap 4): Windows 3.1 chrome (from Navigator) */

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winhelp);

WINHELP_GLOBALS Globals = {3, NULL, TRUE, NULL, NULL, NULL, NULL, NULL, {{{NULL,NULL}},0}, NULL};

#define CTL_ID_BUTTON   0x700
#define CTL_ID_TEXT     0x701


/***********************************************************************
 *
 *           WINHELP_InitFonts
 */
static void WINHELP_InitFonts(HWND hWnd)
{
    WINHELP_WINDOW *win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
    LOGFONTW logfontlist[] = {
        {-10, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        {-12, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        {-12, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        {-12, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        {-12, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        {-10, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"},
        { -8, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 32, L"Helv"}};

    static HFONT fonts[ARRAY_SIZE(logfontlist)];
    static BOOL init = FALSE;

    win->fonts_len = ARRAY_SIZE(logfontlist);
    win->fonts = fonts;

    if (!init)
    {
        UINT i;

        for (i = 0; i < ARRAY_SIZE(logfontlist); i++)
	{
            fonts[i] = CreateFontIndirectW(&logfontlist[i]);
	}

        init = TRUE;
    }
}

static DWORD CALLBACK WINHELP_RtfStreamIn(DWORD_PTR cookie, BYTE* buff,
                                          LONG cb, LONG* pcb)
{
    struct RtfData*     rd = (struct RtfData*)cookie;

    if (rd->where >= rd->ptr) return 1;
    if (rd->where + cb > rd->ptr)
        cb = rd->ptr - rd->where;
    memcpy(buff, rd->where, cb);
    rd->where += cb;
    *pcb = cb;
    return 0;
}

/* TELETEXT PORT (phase 2a, gap 2): the non-scrolling region. WinHelp 3.1
 * shows the paragraphs marked \keepn in a band under the button bar, with a
 * 1px black rule below it, and only the rest scrolls (TTTEST.HLP and every
 * Chameleon topic title in the WfW 3.11 VM). Wine streamed the whole topic
 * into one RichEdit. Here the band is a second, scrollbar-less RichEdit
 * (CTL_ID_NSR) sized to its content by EN_REQUESTRESIZE, with its own
 * hotspot list (page->tt_first_link_nsr); the rule is a 1px black static.
 * Popups keep the whole topic in one control. */
#define CTL_ID_NSR      0x702
#define CTL_ID_RULE     0x703
/* TELETEXT PORT (gap 2): the RichEdit a mouse message came from (text or band) */
static int tt_link_ctl = 0x701 /* CTL_ID_TEXT */;
/* TELETEXT PORT (phase 2a, item 8): the History list's highlighted entry.
 * WinHelp 3.1 highlights the PREVIOUS topic (entry 1; the current one is
 * entry 0), so Enter goes back (real_history.png). */
static unsigned tt_hist_sel;

static void TT_SetupNsr(WINHELP_WINDOW* win)
{
    static const WCHAR emptyW[1];
    HWND hNsr = GetDlgItem(win->hMainWnd, CTL_ID_NSR);
    struct RtfData rd;
    EDITSTREAM es;
    RECT rc;

    win->tt_nsr_on = FALSE;
    if (!hNsr) return;
    if (getenv("TTWINHLP_NO_NSR")) { ShowWindow(hNsr, SW_HIDE); return; }  /* diagnosis: Wine's one-control view */
    SendMessageW(hNsr, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hNsr, WM_SETTEXT, 0, (LPARAM)emptyW);
    if (win->page && win->page->tt_has_nsr)
    {
        /* wrap at the window width before asking for the height */
        GetClientRect(win->hMainWnd, &rc);
        SetWindowPos(hNsr, NULL, 0, 0, rc.right - rc.left, win->tt_nsr_h ? win->tt_nsr_h : 1,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(hNsr, EM_SETBKGNDCOLOR, 0, (LPARAM)win->info->nsr_color);
        SendMessageW(hNsr, EM_SETTARGETDEVICE, 0, 0);
        if (HLPFILE_BrowsePageRegion(win->page, &rd, win->font_scale, 0, TT_REGION_NSR))
        {
            rd.where = rd.data;
            es.dwCookie = (DWORD_PTR)&rd;
            es.dwError = 0;
            es.pfnCallback = WINHELP_RtfStreamIn;
            SendMessageW(hNsr, EM_STREAMIN, SF_RTF, (LPARAM)&es);
            win->tt_nsr_on = TRUE;
        }
        free(rd.data);
    }
    SendMessageW(hNsr, WM_SETREDRAW, TRUE, 0);
    ShowWindow(hNsr, win->tt_nsr_on ? SW_SHOWNA : SW_HIDE);
    ShowWindow(GetDlgItem(win->hMainWnd, CTL_ID_RULE), win->tt_nsr_on ? SW_SHOWNA : SW_HIDE);
    if (win->tt_nsr_on)
    {
        SendMessageW(hNsr, EM_REQUESTRESIZE, 0, 0);   /* -> EN_REQUESTRESIZE: tt_nsr_h */
        RedrawWindow(hNsr, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
    }
}

static void WINHELP_SetupText(HWND hTextWnd, WINHELP_WINDOW* win, ULONG relative)
{
    static const WCHAR emptyW[1];
    int region;
    /* At first clear area - needed by EM_POSFROMCHAR/EM_SETSCROLLPOS */
    SendMessageW(hTextWnd, WM_SETTEXT, 0, (LPARAM)emptyW);
    SendMessageW(hTextWnd, WM_SETREDRAW, FALSE, 0);
    SendMessageW(hTextWnd, EM_SETBKGNDCOLOR, 0, (LPARAM)win->info->sr_color);
    /* set word-wrap to window size (undocumented) */
    SendMessageW(hTextWnd, EM_SETTARGETDEVICE, 0, 0);
    /* TELETEXT PORT: band first, then the scrolling region only */
    TT_SetupNsr(win);
    region = win->tt_nsr_on ? TT_REGION_SCROLL : TT_REGION_ALL;
    if (win->page)
    {
        struct RtfData  rd;
        EDITSTREAM      es;
        unsigned        cp = 0;
        POINTL          ptl;
        POINT           pt;


        if (HLPFILE_BrowsePageRegion(win->page, &rd, win->font_scale, relative, region))
        {
            rd.where = rd.data;
            es.dwCookie = (DWORD_PTR)&rd;
            es.dwError = 0;
            es.pfnCallback = WINHELP_RtfStreamIn;

            SendMessageW(hTextWnd, EM_STREAMIN, SF_RTF, (LPARAM)&es);
            cp = rd.char_pos_rel;
        }
        /* FIXME: else leaking potentially the rd.first_link chain */
        free(rd.data);
        SendMessageW(hTextWnd, EM_POSFROMCHAR, (WPARAM)&ptl, cp ? cp - 1 : 0);
        pt.x = 0; pt.y = ptl.y;
        SendMessageW(hTextWnd, EM_SETSCROLLPOS, 0, (LPARAM)&pt);
        WINE_TRACE("TT SetupText relative=%lx cp=%u y=%ld\n", relative, cp, ptl.y);
    }
    SendMessageW(hTextWnd, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(hTextWnd, NULL, NULL, RDW_FRAME|RDW_INVALIDATE);
}

/* TELETEXT PORT: an in-topic jump (same topic, another position): scroll
 * the text already shown instead of rebuilding it (and the band), which a
 * click inside the band itself must not do. The RTF is regenerated only to
 * find the character at that position, exactly as WINHELP_SetupText does. */
static void TT_ScrollToRelative(WINHELP_WINDOW* win, ULONG relative)
{
    HWND hText = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);
    struct RtfData rd;
    POINTL ptl;
    POINT cur, pt;
    unsigned cp = 0;

    if (!hText || !win->page) return;
    if (HLPFILE_BrowsePageRegion(win->page, &rd, win->font_scale, relative,
                                 win->tt_nsr_on ? TT_REGION_SCROLL : TT_REGION_ALL))
        cp = rd.char_pos_rel;
    free(rd.data);
    SendMessageW(hText, EM_GETSCROLLPOS, 0, (LPARAM)&cur);
    SendMessageW(hText, EM_POSFROMCHAR, (WPARAM)&ptl, cp ? cp - 1 : 0);
    pt.x = 0; pt.y = cur.y + ptl.y;      /* POSFROMCHAR is relative to the view */
    SendMessageW(hText, EM_SETSCROLLPOS, 0, (LPARAM)&pt);
}

/***********************************************************************
 *
 *           WINHELP_GetOpenFileName
 */
BOOL WINHELP_GetOpenFileName(LPSTR lpszFile, int len)
{
    OPENFILENAMEA openfilename;
    CHAR szDir[MAX_PATH];
    CHAR szzFilter[2 * MAX_STRING_LEN + 100];
    LPSTR p = szzFilter;

    WINE_TRACE("()\n");

    LoadStringA(Globals.hInstance, STID_HELP_FILES_HLP, p, MAX_STRING_LEN);
    p += strlen(p) + 1;
    strcpy(p, "*.hlp");
    p += strlen(p) + 1;
    LoadStringA(Globals.hInstance, STID_ALL_FILES, p, MAX_STRING_LEN);
    p += strlen(p) + 1;
    strcpy(p, "*.*");
    p += strlen(p) + 1;
    *p = '\0';

    GetCurrentDirectoryA(sizeof(szDir), szDir);

    lpszFile[0]='\0';

    openfilename.lStructSize       = sizeof(openfilename);
    openfilename.hwndOwner         = (Globals.active_win ? Globals.active_win->hMainWnd : 0);
    openfilename.hInstance         = Globals.hInstance;
    openfilename.lpstrFilter       = szzFilter;
    openfilename.lpstrCustomFilter = 0;
    openfilename.nMaxCustFilter    = 0;
    openfilename.nFilterIndex      = 1;
    openfilename.lpstrFile         = lpszFile;
    openfilename.nMaxFile          = len;
    openfilename.lpstrFileTitle    = 0;
    openfilename.nMaxFileTitle     = 0;
    openfilename.lpstrInitialDir   = szDir;
    openfilename.lpstrTitle        = 0;
    openfilename.Flags             = OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_READONLY;
    openfilename.nFileOffset       = 0;
    openfilename.nFileExtension    = 0;
    openfilename.lpstrDefExt       = 0;
    openfilename.lCustData         = 0;
    openfilename.lpfnHook          = 0;
    openfilename.lpTemplateName    = 0;

    return W31_UNHOOKED(GetOpenFileNameA(&openfilename));   /* TELETEXT PORT: common dialog left to Windows */
}

/***********************************************************************
 *
 *           WINHELP_MessageBoxIDS_s
 */
static INT WINHELP_MessageBoxIDS_s(UINT ids_text, LPCSTR str, UINT ids_title, WORD type)
{
    CHAR text[MAX_STRING_LEN];
    CHAR newtext[MAX_STRING_LEN + MAX_PATH];

    CHAR title[MAX_STRING_LEN];

    LoadStringA(Globals.hInstance, ids_text, text, sizeof(text));
    wsprintfA(newtext, text, str);
    /* TELETEXT PORT: Windows' MessageBoxA takes no resource IDs (Wine's
     * does); load the caption string. */
    LoadStringA(Globals.hInstance, ids_title, title, sizeof(title));

    return MessageBoxA(0, newtext, title, type);
}

/***********************************************************************
 *
 *           WINHELP_LookupHelpFile
 */
HLPFILE* WINHELP_LookupHelpFile(LPCSTR lpszFile)
{
    HLPFILE*        hlpfile;
    char szFullName[MAX_PATH];
    char szAddPath[MAX_PATH];
    char *p;
    /* TELETEXT PORT: directory of the last help file opened. Wine left
     * szAddPath UNINITIALISED when no help window was active yet, which is
     * the case while a file's own [CONFIG] macros run (e.g. TUTORIAL.HLP
     * jumping into itself by name): SearchPath got stack garbage. */
    static char last_dir[MAX_PATH];

    lstrcpynA(szAddPath, last_dir, sizeof(szAddPath));
    /*
     * NOTE: This is needed by popup windows only.
     * In other cases it's not needed but does not hurt though.
     */
    if (Globals.active_win && Globals.active_win->page && Globals.active_win->page->file)
    {
        strcpy(szAddPath, Globals.active_win->page->file->lpszPath);
        p = strrchr(szAddPath, '\\');
        if (p) *p = 0;
    }

    /*
     * FIXME: Should we swap conditions?
     */
    if (!SearchPathA(NULL, lpszFile, ".hlp", MAX_PATH, szFullName, NULL) &&
        !SearchPathA(szAddPath, lpszFile, ".hlp", MAX_PATH, szFullName, NULL))
    {
        if (WINHELP_MessageBoxIDS_s(STID_FILE_NOT_FOUND_s, lpszFile, STID_WHERROR,
                                    MB_YESNO|MB_ICONQUESTION) != IDYES)
            return NULL;
        if (!WINHELP_GetOpenFileName(szFullName, MAX_PATH))
            return NULL;
    }
    hlpfile = HLPFILE_ReadHlpFile(szFullName);
    if (!hlpfile)
        WINHELP_MessageBoxIDS_s(STID_HLPFILE_ERROR_s, lpszFile,
                                STID_WHERROR, MB_OK|MB_ICONSTOP);
    else
    {
        /* TELETEXT PORT: remember where it was found (see last_dir) */
        if (GetFullPathNameA(szFullName, sizeof(last_dir), last_dir, NULL) &&
            (p = strrchr(last_dir, '\\')))
            *p = 0;
    }
    return hlpfile;
}

/******************************************************************
 *		WINHELP_GetWindowInfo
 *
 *
 */
/* TELETEXT PORT: moved from function scope to file scope so TT_PlaceWindow
 * can tell the built-in main window from a file-defined one. */
static      HLPFILE_WINDOWINFO      mwi;

HLPFILE_WINDOWINFO*     WINHELP_GetWindowInfo(HLPFILE* hlpfile, LPCSTR name)
{
    unsigned int     i;

    if (!name || !name[0])
        name = Globals.active_win->info->name;

    if (hlpfile)
        for (i = 0; i < hlpfile->numWindows; i++)
            if (!lstrcmpiA(hlpfile->windows[i].name, name))
                return &hlpfile->windows[i];

    if (strcmp(name, "main") != 0)
    {
        /* TELETEXT PORT: Wine asserted here, which aborts the viewer on any
         * help file that names a window it does not define. Fall back to
         * the main window instead. */
        WINE_FIXME("Couldn't find window info for %s, using main\n", debugstr_a(name));
        name = "main";
        if (hlpfile)
            for (i = 0; i < hlpfile->numWindows; i++)
                if (!lstrcmpiA(hlpfile->windows[i].name, name))
                    return &hlpfile->windows[i];
    }
    if (!mwi.name[0])
    {
        strcpy(mwi.type, "primary");
        strcpy(mwi.name, "main");
        if (hlpfile && hlpfile->lpszTitle[0])
            /* TELETEXT PORT: WinHelp 3.1 captions the main window with the
             * file's TITLE alone (verified in the WfW 3.11 VM); Wine added
             * "Wine Help - <path>". Same choice as ReactOS's __REACTOS__ branch. */
            snprintf(mwi.caption, sizeof(mwi.caption), "%s", hlpfile->lpszTitle);
        else
            LoadStringA(Globals.hInstance, STID_WINE_HELP, mwi.caption, sizeof(mwi.caption));
        mwi.origin.x = mwi.origin.y = mwi.size.cx = mwi.size.cy = CW_USEDEFAULT;
        /* TELETEXT PORT: placed by TT_PlaceWindow (M_WindowPosition). */
        mwi.style = SW_SHOW;
        mwi.win_style = WS_OVERLAPPEDWINDOW;
        mwi.sr_color = mwi.nsr_color = 0xFFFFFF;
    }
    return &mwi;
}

/******************************************************************
 *		TT_PlaceWindow                         (TELETEXT PORT)
 *
 * Screen rectangle for a main or secondary window.
 * - Positions from a help file's [WINDOWS] section are WinHelp's virtual
 *   0..1023 coordinates, which WinHelp scaled to the screen; Wine used them
 *   as pixels. MODERN ACCOMMODATION: Windows 3.1 had one screen; we scale
 *   to the work area of the monitor the help session is on (the active help
 *   window's monitor, else the saved main window's, else the primary).
 * - "main" without a file-defined position uses M_WindowPosition.
 * - Nothing is left to CW_USEDEFAULT, which would put the window wherever
 *   Windows likes (in practice the primary monitor).
 */
static void TT_PlaceWindow(const HLPFILE_WINDOWINFO *wi, BOOL primary,
                           POINT *pos, SIZE *size)
{
    RECT saved, wa;
    BOOL have_saved = TT_LoadWindowPos("M_WindowPosition", &saved, NULL);
    HMONITOR mon;
    MONITORINFO mi;
    LONG ww, wh;

    if (Globals.active_win && Globals.active_win->hMainWnd)
        mon = MonitorFromWindow(Globals.active_win->hMainWnd, MONITOR_DEFAULTTOPRIMARY);
    else if (have_saved)
        mon = MonitorFromRect(&saved, MONITOR_DEFAULTTOPRIMARY);
    else
    {
        POINT origin = {0, 0};
        mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    }
    mi.cbSize = sizeof(mi);
    GetMonitorInfoA(mon, &mi);
    wa = mi.rcWork;
    ww = wa.right - wa.left;
    wh = wa.bottom - wa.top;

    if (primary && have_saved &&
        (wi == &mwi || wi->origin.x == CW_USEDEFAULT || wi->size.cx == CW_USEDEFAULT))
    {
        pos->x = saved.left;
        pos->y = saved.top;
        size->cx = saved.right - saved.left;
        size->cy = saved.bottom - saved.top;
        return;
    }
    if (wi == &mwi)  /* built-in main window, nothing saved yet */
    {
        pos->x = wa.left + ww / 8;
        pos->y = wa.top + wh / 8;
        size->cx = ww * 3 / 4;
        size->cy = wh * 3 / 4;
        return;
    }
    pos->x = wa.left + (wi->origin.x != CW_USEDEFAULT ? MulDiv(wi->origin.x, ww, 1024) : ww / 8);
    pos->y = wa.top  + (wi->origin.y != CW_USEDEFAULT ? MulDiv(wi->origin.y, wh, 1024) : wh / 8);
    size->cx = wi->size.cx != CW_USEDEFAULT ? MulDiv(wi->size.cx, ww, 1024) : ww * 3 / 4;
    size->cy = wi->size.cy != CW_USEDEFAULT ? MulDiv(wi->size.cy, wh, 1024) : wh * 3 / 4;
    if (size->cx < 100) size->cx = 100;
    if (size->cy < 60) size->cy = 60;
    /* keep it on the monitor */
    if (pos->x + size->cx > wa.right)  pos->x = max(wa.left, wa.right - size->cx);
    if (pos->y + size->cy > wa.bottom) pos->y = max(wa.top, wa.bottom - size->cy);
}

/* TELETEXT PORT (phase 2a, gap 5): popup fidelity. Measured on the real
 * WINHELP.EXE 3.10 (VM, pixel scans of shot_real_tt_popup and the Chameleon
 * glossary popup): outer box sized to the text, at most 448..487 px wide
 * (two samples; 480 used), 2 px black border, text inset ~10 px left and
 * right and ~4 px top and bottom, centred on the click with its top ~9 px
 * below it, kept 2 px inside the screen; and a drop shadow: the popup's
 * rectangle offset 12,12, a 50% black stipple over whatever is behind,
 * outside the popup only. Wine: half the parent's width, a 1px Windows
 * border, no shadow. */
#define TT_POPUP_MAXW    480
#define TT_POP_BORDER    2
#define TT_POP_PADX      10
#define TT_POP_PADY      4
#define TT_POP_SHADOW    12
#define TT_POP_RESLACK   8   /* RichEdit needs ~8 px past the last glyph not to wrap; the real padding is 10 px of white past the ink */
static POINT tt_popup_click;
static const char TT_SHADOW_CLASS[] = "TTWinHelpShadow";

static BOOL tt_is_popup(const WINHELP_WINDOW* win)
{
    return win && win == Globals.active_popup && win->hMainWnd &&
           !(GetWindowLongA(win->hMainWnd, GWL_STYLE) & WS_CAPTION);
}

/* the 50% stipple: black dots on white, white keyed out (layered window) */
static LRESULT CALLBACK TT_ShadowWndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        static const WORD bits[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        HBITMAP pat = CreateBitmap(8, 8, 1, 1, bits);
        HBRUSH br = CreatePatternBrush(pat);
        RECT rc;
        GetClientRect(h, &rc);
        SetTextColor(dc, RGB(255, 255, 255));   /* 1 bits */
        SetBkColor(dc, RGB(0, 0, 0));           /* 0 bits */
        SetBrushOrgEx(dc, 0, 0, NULL);
        FillRect(dc, &rc, br);
        DeleteObject(br);
        DeleteObject(pat);
        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    return DefWindowProcA(h, msg, wp, lp);
}

static void TT_PlaceShadow(WINHELP_WINDOW* win)
{
    RECT r;
    HRGN all, hole;
    int w, h;
    if (!win->tt_shadow || !win->hMainWnd) return;
    GetWindowRect(win->hMainWnd, &r);
    w = r.right - r.left; h = r.bottom - r.top;
    SetWindowPos(win->tt_shadow, win->hMainWnd, r.left + TT_POP_SHADOW, r.top + TT_POP_SHADOW,
                 w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    /* only the part outside the popup: an L along the right and bottom */
    all = CreateRectRgn(0, 0, w, h);
    hole = CreateRectRgn(0, 0, w - TT_POP_SHADOW, h - TT_POP_SHADOW);
    CombineRgn(all, all, hole, RGN_DIFF);
    DeleteObject(hole);
    SetWindowRgn(win->tt_shadow, all, TRUE);   /* the system owns the region now */
}

static void TT_CreateShadow(WINHELP_WINDOW* win)
{
    if (getenv("TTWINHLP_NOSHADOW")) return;
    win->tt_shadow = CreateWindowExA(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     TT_SHADOW_CLASS, "", WS_POPUP,
                                     0, 0, 0, 0, win->hMainWnd, NULL, Globals.hInstance, NULL);
    if (win->tt_shadow)
    {
        SetLayeredWindowAttributes(win->tt_shadow, RGB(255, 255, 255), 0, LWA_COLORKEY);
        TT_PlaceShadow(win);
    }
}

/* Shrink the popup to its widest line when nothing wrapped, then centre it
 * on the click and keep it 2 px inside the work area. */
static void TT_FitPopup(WINHELP_WINDOW* win)
{
    HWND hText = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);
    RECT wr, wa, tr;
    int lines, i, widest = 0, w, x;
    BOOL wrapped = FALSE;

    if (!hText) return;
    /* the popup is born 10 px tall: an empty format rectangle makes
     * RichEdit wrap at every character. Format at full width and a working
     * height first; EN_REQUESTRESIZE sets the real height afterwards. */
    GetWindowRect(win->hMainWnd, &wr);
    SetWindowPos(win->hMainWnd, NULL, 0, 0, wr.right - wr.left, 400,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    WINHELP_LayoutMainWindow(win);
    GetClientRect(hText, &tr);
    lines = (int)SendMessageW(hText, EM_GETLINECOUNT, 0, 0);
    WINE_TRACE("TT fit: %d lines, text window %ldx%ld\n", lines, tr.right, tr.bottom);
    /* The narrowest text width that keeps exactly the same line breaks:
     * the widest line, whether it ends a paragraph or wraps (the real
     * WinHelp shrinks a wrapped popup to its widest line too: the Chameleon
     * glossary's "access" is 436 px, not its maximum). A binary search on
     * the line starts; EM_POSFROMCHAR at a line end overstated the width
     * by ~11 px in Microsoft's RichEdit. */
    if (lines > 0 && lines <= 64)
    {
        LONG starts[64];
        int lo = 16, hi = tr.right, mid;
        for (i = 0; i < lines; i++) starts[i] = (LONG)SendMessageW(hText, EM_LINEINDEX, i, 0);
        while (lo < hi)
        {
            BOOL same;
            mid = (lo + hi) / 2;
            SetWindowPos(hText, NULL, 0, 0, mid, tr.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            same = ((int)SendMessageW(hText, EM_GETLINECOUNT, 0, 0) == lines);
            for (i = 0; same && i < lines; i++)
                same = ((LONG)SendMessageW(hText, EM_LINEINDEX, i, 0) == starts[i]);
            if (same) hi = mid; else lo = mid + 1;
        }
        widest = lo;
        WINE_TRACE("TT fit: narrowest same-break width %d of %ld\n", widest, tr.right);
    }
    else
        wrapped = TRUE;
    GetWindowRect(win->hMainWnd, &wr);
    w = wr.right - wr.left;
    if (!wrapped && widest > 0)
    {
        int want = widest + 2 * (TT_POP_BORDER + TT_POP_PADX) - TT_POP_RESLACK;
        if (want < w) w = want;
    }
    TT_WorkAreaFromWindow(Globals.active_win ? Globals.active_win->hMainWnd : win->hMainWnd, &wa);
    x = tt_popup_click.x - w / 2;
    if (x + w > wa.right - 2) x = wa.right - 2 - w;
    if (x < wa.left + 2) x = wa.left + 2;
    SetWindowPos(win->hMainWnd, NULL, x, wr.top, w, wr.bottom - wr.top, SWP_NOZORDER | SWP_NOACTIVATE);
    WINHELP_LayoutMainWindow(win);
}

/******************************************************************
 *		HLPFILE_GetPopupWindowInfo
 *
 *
 */
HLPFILE_WINDOWINFO* WINHELP_GetPopupWindowInfo(HLPFILE* hlpfile, WINHELP_WINDOW* parent, LPARAM mouse)
{
    static      HLPFILE_WINDOWINFO      wi;

    RECT parent_rect;
    
    wi.type[0] = wi.name[0] = wi.caption[0] = '\0';

    /* Calculate horizontal size and position of a popup window */
    GetWindowRect(parent->hMainWnd, &parent_rect);
    wi.size.cx = (parent_rect.right  - parent_rect.left) / 2;
    wi.size.cy = 10; /* need a non null value, so that borders are taken into account while computing */

    wi.origin.x = (short)LOWORD(mouse);
    wi.origin.y = (short)HIWORD(mouse);
    /* TELETEXT PORT: the point is in the coordinates of the RichEdit that
     * reported the click (text or band), not of the main window (Wine put
     * the popup one button bar too high). */
    {
        HWND src = GetDlgItem(parent->hMainWnd, tt_link_ctl);
        ClientToScreen(src ? src : parent->hMainWnd, &wi.origin);
    }
    /* TELETEXT PORT (phase 2a, gap 5): WinHelp 3.10's popup (measured in
     * the VM, TTTEST "modem" and the Chameleon glossary): wrapped at most
     * TT_POPUP_MAXW wide, then shrunk to its widest line (TT_FitPopup),
     * centred on the click, its top ~9 px below it, 2 px black border, no
     * Windows frame. Wine: half the parent's width, top at the click. */
    wi.size.cx = TT_POPUP_MAXW;
    tt_popup_click = wi.origin;
    WINE_TRACE("TT popup: mouse %d,%d -> screen %ld,%ld\n", (short)LOWORD(mouse), (short)HIWORD(mouse), wi.origin.x, wi.origin.y);
    wi.origin.y += 9;
    wi.origin.x -= wi.size.cx / 2;
    /* TELETEXT PORT: clamp to the parent's monitor, not to 0..SM_CXSCREEN
     * (the primary monitor only); see ttport.c. */
    {
        RECT wa;
        TT_WorkAreaFromWindow(parent->hMainWnd, &wa);
        wi.origin.x  = min(wi.origin.x, wa.right - wi.size.cx);
        wi.origin.x  = max(wi.origin.x, wa.left);
    }

    wi.style = SW_SHOW;
    wi.win_style = WS_POPUP;   /* TELETEXT PORT: was | WS_BORDER; the 2 px border is drawn (WM_ERASEBKGND) */
    if (hlpfile->has_popup_color)
        wi.sr_color = hlpfile->popup_color;
    else if (parent->page && parent->page->file->has_popup_color)
        wi.sr_color = parent->page->file->popup_color;
    else
        wi.sr_color = parent->info->sr_color;
    wi.nsr_color = 0xFFFFFF;

    return &wi;
}

typedef struct
{
    WORD size;
    WORD command;
    LONG data;
    LONG reserved;
    WORD ofsFilename;
    WORD ofsData;
} WINHELP,*LPWINHELP;

static BOOL WINHELP_HasWorkingWindow(void)
{
    if (!Globals.active_win) return FALSE;
    if (Globals.active_win->next || Globals.win_list != Globals.active_win) return TRUE;
    return Globals.active_win->page != NULL && Globals.active_win->page->file != NULL;
}

/******************************************************************
 *		WINHELP_HandleCommand
 *
 *
 */
static LRESULT  WINHELP_HandleCommand(HWND hSrcWnd, LPARAM lParam)
{
    COPYDATASTRUCT*     cds = (COPYDATASTRUCT*)lParam;
    WINHELP*            wh;

    if (cds->dwData != 0xA1DE505)
    {
        WINE_FIXME("Wrong magic number (%08Ix)\n", cds->dwData);
        return 0;
    }

    wh = cds->lpData;

    if (wh)
    {
        char*   ptr = (wh->ofsFilename) ? (LPSTR)wh + wh->ofsFilename : NULL;

        WINE_TRACE("Got[%u]: cmd=%u data=%08lx fn=%s\n",
                   wh->size, wh->command, wh->data, debugstr_a(ptr));
        switch (wh->command)
        {
        case HELP_CONTEXT:
            if (ptr)
            {
                MACRO_JumpContext(ptr, "main", wh->data);
            }
            if (!WINHELP_HasWorkingWindow()) MACRO_Exit();
            break;
        case HELP_QUIT:
            MACRO_Exit();
            break;
        case HELP_CONTENTS:
            if (ptr)
            {
                MACRO_JumpContents(ptr, "main");
            }
            if (!WINHELP_HasWorkingWindow()) MACRO_Exit();
            break;
        case HELP_HELPONHELP:
            MACRO_HelpOn();
            if (!WINHELP_HasWorkingWindow()) MACRO_Exit();
            break;
        /* case HELP_SETINDEX: */
        case HELP_SETCONTENTS:
            if (ptr)
            {
                MACRO_SetContents(ptr, wh->data);
            }
            break;
        case HELP_CONTEXTPOPUP:
            if (ptr)
            {
                MACRO_PopupContext(ptr, wh->data);
            }
            break;
        /* case HELP_FORCEFILE:*/
        /* case HELP_CONTEXTMENU: */
        case HELP_FINDER:
            /* in fact, should be the topic dialog box */
            WINE_FIXME("HELP_FINDER: stub\n");
            if (ptr)
            {
                MACRO_JumpHash(ptr, "main", 0);
            }
            break;
        /* case HELP_WM_HELP: */
        /* case HELP_SETPOPUP_POS: */
        /* case HELP_KEY: */
        /* case HELP_COMMAND: */
        /* case HELP_PARTIALKEY: */
        /* case HELP_MULTIKEY: */
        /* case HELP_SETWINPOS: */
        default:
            WINE_FIXME("Unhandled command (%x) for remote winhelp control\n", wh->command);
            break;
        }
    }
    /* Always return success for now */
    return 1;
}

void            WINHELP_LayoutMainWindow(WINHELP_WINDOW* win)
{
    RECT        rect, button_box_rect;
    INT         text_top = 0;
    HWND        hButtonBoxWnd = GetDlgItem(win->hMainWnd, CTL_ID_BUTTON);
    HWND        hTextWnd = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);

    GetClientRect(win->hMainWnd, &rect);

    /* Update button box and text Window */
    SetWindowPos(hButtonBoxWnd, HWND_TOP,
                 rect.left, rect.top,
                 rect.right - rect.left,
                 rect.bottom - rect.top, 0);

    if (GetWindowRect(hButtonBoxWnd, &button_box_rect))
        text_top = rect.top + button_box_rect.bottom - button_box_rect.top;

    /* TELETEXT PORT (gap 2): the non-scrolling band and its rule */
    if (win->tt_nsr_on && GetDlgItem(win->hMainWnd, CTL_ID_NSR))
    {
        int h = win->tt_nsr_h > 0 ? win->tt_nsr_h : 0;
        int maxh = (rect.bottom - text_top) / 2;     /* never eat the whole window */
        if (h > maxh) h = maxh;
        SetWindowPos(GetDlgItem(win->hMainWnd, CTL_ID_NSR), HWND_TOP,
                     rect.left, text_top, rect.right - rect.left, h, SWP_NOACTIVATE);
        SetWindowPos(GetDlgItem(win->hMainWnd, CTL_ID_RULE), HWND_TOP,
                     rect.left, text_top + h, rect.right - rect.left, 1, SWP_NOACTIVATE);
        text_top += h + 1;
        /* a new width can rewrap the band: ask again (the handler relayouts
         * only when the height really changes) */
        SendMessageW(GetDlgItem(win->hMainWnd, CTL_ID_NSR), EM_REQUESTRESIZE, 0, 0);
    }

    /* TELETEXT PORT (gap 5): a popup's text sits inside its 2 px border
     * and the WinHelp 3.1 padding */
    if (tt_is_popup(win))
    {
        InflateRect(&rect, -(TT_POP_BORDER + TT_POP_PADX), -(TT_POP_BORDER + TT_POP_PADY));
        rect.right += TT_POP_RESLACK;   /* the text window reaches into the right padding */
        text_top = rect.top;
    }

    SetWindowPos(hTextWnd, HWND_TOP,
                 rect.left, text_top,
                 rect.right - rect.left,
                 rect.bottom - text_top, 0);

}

/******************************************************************
 *		WINHELP_DeleteButtons
 *
 */
static void WINHELP_DeleteButtons(WINHELP_WINDOW* win)
{
    WINHELP_BUTTON*     b;
    WINHELP_BUTTON*     bp;

    for (b = win->first_button; b; b = bp)
    {
        DestroyWindow(b->hWnd);
        bp = b->next;
        free(b);
    }
    win->first_button = NULL;
}

/******************************************************************
 *		WINHELP_DeleteBackSet
 *
 */
void WINHELP_DeleteBackSet(WINHELP_WINDOW* win)
{
    unsigned int i;

    for (i = 0; i < win->back.index; i++)
    {
        HLPFILE_FreeHlpFile(win->back.set[i].page->file);
        win->back.set[i].page = NULL;
    }
    win->back.index = 0;
}

/******************************************************************
 *             WINHELP_DeletePageLinks
 *
 */
static void WINHELP_DeletePageLinks(HLPFILE_PAGE* page)
{
    HLPFILE_LINK*       curr;
    HLPFILE_LINK*       next;

    for (curr = page->first_link; curr; curr = next)
    {
        next = curr->next;
        free(curr);
    }
    /* TELETEXT PORT (gap 2): the band's hotspots */
    for (curr = page->tt_first_link_nsr; curr; curr = next)
    {
        next = curr->next;
        free(curr);
    }
    page->tt_first_link_nsr = NULL;
    /* TELETEXT PORT: Wine left the freed list on the page. Two windows can
     * show the same page (TUTORIAL.HLP: main and "tutorial"), so closing
     * the second freed it again (double free, crash on exit). Known residue:
     * the surviving window's hotspots go inert until its topic is redrawn,
     * because links live on the page, not on the window. */
    page->first_link = NULL;
}

/***********************************************************************
 *
 *           WINHELP_GrabWindow
 */
WINHELP_WINDOW* WINHELP_GrabWindow(WINHELP_WINDOW* win)
{
    WINE_TRACE("Grab %p#%d++\n", win, win->ref_count);
    win->ref_count++;
    return win;
}

/***********************************************************************
 *
 *           WINHELP_ReleaseWindow
 */
BOOL WINHELP_ReleaseWindow(WINHELP_WINDOW* win)
{
    WINE_TRACE("Release %p#%d--\n", win, win->ref_count);

    if (!--win->ref_count)
    {
        DestroyWindow(win->hMainWnd);
        return FALSE;
    }
    return TRUE;
}

/***********************************************************************
 *
 *           WINHELP_DeleteWindow
 */
static void WINHELP_DeleteWindow(WINHELP_WINDOW* win)
{
    WINHELP_WINDOW**    w;
    BOOL bExit;
    HWND hTextWnd;

    for (w = &Globals.win_list; *w; w = &(*w)->next)
    {
        if (*w == win)
        {
            *w = win->next;
            break;
        }
    }
    bExit = (Globals.wVersion >= 4 && !lstrcmpiA(win->info->name, "main"));

    if (Globals.active_win == win)
    {
        Globals.active_win = Globals.win_list;
        if (Globals.win_list)
            SetActiveWindow(Globals.win_list->hMainWnd);
    }

    if (win == Globals.active_popup)
        Globals.active_popup = NULL;
    if (win->tt_shadow) { DestroyWindow(win->tt_shadow); win->tt_shadow = NULL; }   /* TELETEXT PORT (gap 5) */

    hTextWnd = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);
    SetWindowLongPtrA(hTextWnd, GWLP_WNDPROC, (LONG_PTR)win->origRicheditWndProc);
    /* TELETEXT PORT (gap 2): same for the band, before win is freed */
    if (GetDlgItem(win->hMainWnd, CTL_ID_NSR))
        SetWindowLongPtrA(GetDlgItem(win->hMainWnd, CTL_ID_NSR), GWLP_WNDPROC,
                          (LONG_PTR)win->origRicheditWndProc);

    WINHELP_DeleteButtons(win);

    if (win->page) WINHELP_DeletePageLinks(win->page);
    if (win->hHistoryWnd) DestroyWindow(win->hHistoryWnd);

    DeleteObject(win->hBrush);

    WINHELP_DeleteBackSet(win);

    if (win->page) HLPFILE_FreeHlpFile(win->page->file);
    free(win);

    if (bExit) MACRO_Exit();
    if (!Globals.win_list)
        PostQuitMessage(0);
}

/* TELETEXT PORT: is this window still in the window list (not yet freed)?
 * Compares pointers only; never dereferences win. */
static BOOL WINHELP_IsWindowAlive(const WINHELP_WINDOW* win)
{
    const WINHELP_WINDOW* w;
    for (w = Globals.win_list; w; w = w->next)
        if (w == win) return TRUE;
    return FALSE;
}

static char* WINHELP_GetCaption(WINHELP_WNDPAGE* wpage)
{
    if (wpage->wininfo->caption[0]) return wpage->wininfo->caption;
    return wpage->page->file->lpszTitle;
}

static void WINHELP_RememberPage(WINHELP_WINDOW* win, WINHELP_WNDPAGE* wpage)
{
    unsigned        num;

    if (!Globals.history.index || Globals.history.set[0].page != wpage->page)
    {
        num = ARRAY_SIZE(Globals.history.set);
        /* we're full, remove latest entry */
        if (Globals.history.index == num)
        {
            HLPFILE_FreeHlpFile(Globals.history.set[num - 1].page->file);
            Globals.history.index--;
        }
        memmove(&Globals.history.set[1], &Globals.history.set[0],
                Globals.history.index * sizeof(Globals.history.set[0]));
        Globals.history.set[0] = *wpage;
        Globals.history.index++;
        wpage->page->file->wRefCount++;
    }
    tt_hist_sel = (Globals.history.index > 1) ? 1 : 0;   /* TELETEXT PORT (item 8) */
    if (win->hHistoryWnd) InvalidateRect(win->hHistoryWnd, NULL, TRUE);

    num = ARRAY_SIZE(win->back.set);
    if (win->back.index == num)
    {
        /* we're full, remove latest entry */
        HLPFILE_FreeHlpFile(win->back.set[0].page->file);
        memmove(&win->back.set[0], &win->back.set[1],
                (num - 1) * sizeof(win->back.set[0]));
        win->back.index--;
    }
    win->back.set[win->back.index++] = *wpage;
    wpage->page->file->wRefCount++;
}

/***********************************************************************
 *
 *           WINHELP_FindLink
 */
/* TELETEXT PORT (gap 2): which RichEdit the mouse is over. Wine had one;
 * the band (CTL_ID_NSR) has its own control and link list. */
/* (tt_link_ctl is defined with CTL_ID_NSR, above) */

static HLPFILE_LINK* WINHELP_FindLink(WINHELP_WINDOW* win, LPARAM pos)
{
    HLPFILE_LINK*           link;
    POINTL                  mouse_ptl, char_ptl, char_next_ptl;
    DWORD                   cp;
    HWND                    hText;

    if (!win->page) return NULL;
    hText = GetDlgItem(win->hMainWnd, tt_link_ctl);
    if (!hText) return NULL;

    mouse_ptl.x = (short)LOWORD(pos);
    mouse_ptl.y = (short)HIWORD(pos);
    cp = SendMessageW(hText, EM_CHARFROMPOS,
                      0, (LPARAM)&mouse_ptl);
    for (link = (tt_link_ctl == CTL_ID_NSR) ? win->page->tt_first_link_nsr : win->page->first_link;
         link; link = link->next)
    {
        /* TELETEXT PORT (gap 2, found on the GLOSHLP A..Z strip): Microsoft's
         * EM_CHARFROMPOS returns the NEAREST character boundary, so over the
         * right half of a picture it gives the next cp and a bitmap hotspot
         * (one picture char, cpMin == cpMax) was missed; Wine's RichEdit
         * returns the picture itself. Test a bitmap hotspot against its own
         * picture's position, whatever cp the click mapped to. */
        if (link->bHotSpot && (cp == link->cpMin + 1) && cp > 0)
        {
            HLPFILE_HOTSPOTLINK*    hslink = (HLPFILE_HOTSPOTLINK*)link;
            SendMessageW(hText, EM_POSFROMCHAR, (LPARAM)&char_ptl, link->cpMin);
            if (mouse_ptl.x >= char_ptl.x + hslink->x &&
                mouse_ptl.x <  char_ptl.x + hslink->x + hslink->width &&
                mouse_ptl.y >= char_ptl.y + hslink->y &&
                mouse_ptl.y <  char_ptl.y + hslink->y + hslink->height)
                break;
            continue;
        }
        if (link->cpMin <= cp && cp <= link->cpMax)
        {
            /* check whether we're at end of line */
            SendMessageW(hText, EM_POSFROMCHAR,
                         (LPARAM)&char_ptl, cp);
            SendMessageW(hText, EM_POSFROMCHAR,
                         (LPARAM)&char_next_ptl, cp + 1);
            if (link->bHotSpot)
            {
                HLPFILE_HOTSPOTLINK*    hslink = (HLPFILE_HOTSPOTLINK*)link;
                if ((mouse_ptl.x < char_ptl.x + hslink->x) ||
                    (mouse_ptl.x >= char_ptl.x + hslink->x + hslink->width) ||
                    (mouse_ptl.y < char_ptl.y + hslink->y) ||
                    (mouse_ptl.y >= char_ptl.y + hslink->y + hslink->height))
                    continue;
                break;
            }
            if (char_next_ptl.y != char_ptl.y || mouse_ptl.x >= char_next_ptl.x)
                link = NULL;
            break;
        }
    }
    return link;
}

static LRESULT CALLBACK WINHELP_RicheditWndProc(HWND hWnd, UINT msg,
                                                WPARAM wParam, LPARAM lParam)
{
    WINHELP_WINDOW *win = (WINHELP_WINDOW*) GetWindowLongPtrW(GetParent(hWnd), 0);
    DWORD messagePos;
    POINT pt;
    switch(msg)
    {
        case WM_SETCURSOR:
            messagePos = GetMessagePos();
            pt.x = (short)LOWORD(messagePos);
            pt.y = (short)HIWORD(messagePos);
            ScreenToClient(hWnd, &pt);
            tt_link_ctl = GetDlgCtrlID(hWnd);   /* TELETEXT PORT: band or text */
            if (win->page && WINHELP_FindLink(win, MAKELPARAM(pt.x, pt.y)))
            {
                tt_link_ctl = CTL_ID_TEXT;
                SetCursor(win->hHandCur);
                return 0;
            }
            tt_link_ctl = CTL_ID_TEXT;
            /* fall through */
        default:
            return CallWindowProcA(win->origRicheditWndProc, hWnd, msg, wParam, lParam);
    }
}

/***********************************************************************
 *
 *           WINHELP_CreateHelpWindow
 */
BOOL WINHELP_CreateHelpWindow(WINHELP_WNDPAGE* wpage, int nCmdShow, BOOL remember)
{
    WINHELP_WINDOW*     win = NULL;
    BOOL                bPrimary, bPopup, bReUsed = FALSE;
    HICON               hIcon;
    HWND                hTextWnd = NULL;

    bPrimary = !lstrcmpiA(wpage->wininfo->name, "main");
    bPopup = !bPrimary && (wpage->wininfo->win_style & WS_POPUP);

    if (!bPopup)
    {
        for (win = Globals.win_list; win; win = win->next)
        {
            if (!lstrcmpiA(win->info->name, wpage->wininfo->name))
            {
                if (win->page == wpage->page && win->info == wpage->wininfo)
                {
                    /* see #22979, some hlp files have a macro (run at page opening), which
                     * jumps to the very same page
                     * Exit gracefully in that case
                     */
                    /* TELETEXT PORT (found in gap 2 on the Chameleon glossary):
                     * that early exit also swallowed every jump to a POSITION
                     * inside the topic on show (GLOSHLP's A..Z letters jump
                     * to "M" etc. inside the one glossary topic; the real
                     * WinHelp scrolls there). A jump with an in-topic offset
                     * now scrolls; offset 0 keeps Wine's early exit. */
                    if (wpage->relative)
                        TT_ScrollToRelative(win, wpage->relative);
                    return TRUE;
                }
                WINHELP_DeleteButtons(win);
                bReUsed = TRUE;
                SetWindowTextA(win->hMainWnd, WINHELP_GetCaption(wpage));
                if (win->info != wpage->wininfo)
                {
                    /* TELETEXT PORT: Wine moved by raw 0..1023 values (and
                     * had SWP_NOMOVE/SWP_NOSIZE crossed); TT_PlaceWindow
                     * scales them. Only a window whose definition carries a
                     * position is moved. */
                    if (wpage->wininfo->origin.x != CW_USEDEFAULT ||
                        wpage->wininfo->size.cx != CW_USEDEFAULT)
                    {
                        POINT   pt;
                        SIZE    sz;
                        TT_PlaceWindow(wpage->wininfo, bPrimary, &pt, &sz);
                        SetWindowPos(win->hMainWnd, HWND_TOP, pt.x, pt.y, sz.cx, sz.cy, 0);
                    }
                }

                if (wpage->page && win->page && wpage->page->file != win->page->file)
                    WINHELP_DeleteBackSet(win);
                WINHELP_InitFonts(win->hMainWnd);

                win->page = wpage->page;
                win->info = wpage->wininfo;
                hTextWnd = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);
                WINHELP_SetupText(hTextWnd, win, wpage->relative);

                InvalidateRect(win->hMainWnd, NULL, TRUE);
                if (win->hHistoryWnd) InvalidateRect(win->hHistoryWnd, NULL, TRUE);

                break;
            }
        }
    }

    if (!win)
    {
        /* Initialize WINHELP_WINDOW struct */
        win = calloc(1, sizeof(WINHELP_WINDOW));
        if (!win) return FALSE;
        win->next = Globals.win_list;
        Globals.win_list = win;

        win->hHandCur = LoadCursorW(0, (LPWSTR)IDC_HAND);
        win->back.index = 0;
        win->font_scale = 1;
        WINHELP_GrabWindow(win);
    }
    win->page = wpage->page;
    win->info = wpage->wininfo;
    WINHELP_GrabWindow(win);

    if (!bPopup && wpage->page && remember)
    {
        WINHELP_RememberPage(win, wpage);
    }

    if (bPopup)
        Globals.active_popup = win;
    else
        Globals.active_win = win;

    /* Initialize default pushbuttons */
    if (bPrimary && wpage->page)
    {
        CHAR    buffer[MAX_STRING_LEN];

        /* TELETEXT PORT (phase 2a, gap 3): the WinHelp 3.1 button bar,
         * "Contents Search Back History" (then << >> from BrowseButtons()),
         * as WINHELP.EXE 3.10 shows it, with the button IDs the Help
         * Compiler documents (btn_contents, btn_search, btn_back,
         * btn_history, btn_previous, btn_next): help files address them in
         * macros (MPLAYER.HLP: ...Button(`btn_s...)). Wine had "Summary",
         * "Index" (BTN_INDEX), no History button. */
        LoadStringA(Globals.hInstance, STID_CONTENTS, buffer, sizeof(buffer));
        MACRO_CreateButton("BTN_CONTENTS", buffer, "Contents()");
        LoadStringA(Globals.hInstance, STID_INDEX, buffer, sizeof(buffer));
        MACRO_CreateButton("BTN_SEARCH", buffer, "Search()");
        LoadStringA(Globals.hInstance, STID_BACK, buffer, sizeof(buffer));
        MACRO_CreateButton("BTN_BACK", buffer, "Back()");
        if (win->back.index <= 1) MACRO_DisableButton("BTN_BACK");
        LoadStringA(Globals.hInstance, STID_TT_HISTORY, buffer, sizeof(buffer));
        MACRO_CreateButton("BTN_HISTORY", buffer, "History()");
    }

    if (!bReUsed)
    {
        /* TELETEXT PORT: popups arrive in screen pixels already; main and
         * secondary windows are placed by TT_PlaceWindow. */
        POINT   create_pt = wpage->wininfo->origin;
        SIZE    create_sz = wpage->wininfo->size;
        if (!bPopup) TT_PlaceWindow(wpage->wininfo, bPrimary, &create_pt, &create_sz);
        win->hMainWnd = CreateWindowExA((bPopup) ? WS_EX_TOOLWINDOW : 0, MAIN_WIN_CLASS_NAME,
                                       WINHELP_GetCaption(wpage),
                                       bPrimary ? WS_OVERLAPPEDWINDOW : wpage->wininfo->win_style,
                                       create_pt.x, create_pt.y,
                                       create_sz.cx, create_sz.cy,
                                       bPopup ? Globals.active_win->hMainWnd : NULL,
                                       bPrimary ? LoadMenuW(Globals.hInstance, MAKEINTRESOURCEW(MAIN_MENU)) : 0,
                                       Globals.hInstance, win);
        /* TELETEXT PORT (phase 2a, gap 4): the Windows 3.1 title bar, frame
         * and menu bar (w31.c, from Navigator), before the window is shown.
         * Popups have no caption and keep their own frame (gap 5). */
        if (!bPopup && win->hMainWnd)
            w31_caption_attach(win->hMainWnd);
        /* TELETEXT PORT (phase 2a, item 9): only the main window has a
         * button bar in WinHelp 3.1; secondary windows (the Chameleon
         * glossary's "2nd") show none (real_glossary screens). Wine gave
         * every non-popup window one. TTWINHLP_SECBAR=1: Wine's behaviour. */
        if (!bPopup && (bPrimary || getenv("TTWINHLP_SECBAR")))
            /* Create button box and text Window */
            CreateWindowA(BUTTON_BOX_WIN_CLASS_NAME, "", WS_CHILD | WS_VISIBLE,
                         0, 0, 0, 0, win->hMainWnd, (HMENU)CTL_ID_BUTTON, Globals.hInstance, NULL);

        /* TELETEXT PORT: msftedit's RichEdit (4.1+) instead of riched20
         * (Microsoft's RichEdit 3.0): it can hide the gridlines RichEdit
         * draws on borderless tables (WinHelp draws none). Falls back to
         * RichEdit20A when msftedit.dll is absent. */
        hTextWnd = CreateWindowA(Globals.tt_msftedit ? "RICHEDIT50W" : RICHEDIT_CLASS20A, NULL,
                                ES_MULTILINE | ES_READONLY | WS_CHILD | WS_HSCROLL | WS_VSCROLL | WS_VISIBLE,
                                0, 0, 0, 0, win->hMainWnd, (HMENU)CTL_ID_TEXT, Globals.hInstance, NULL);
        TT_RichEditEnablePictures(hTextWnd);   /* TELETEXT PORT: see ttport.c, 4 */
        SendMessageW(hTextWnd, EM_SETEDITSTYLE, SES_HIDEGRIDLINES, SES_HIDEGRIDLINES); /* TELETEXT PORT */
        /* TELETEXT PORT (item 7): WinHelp 3.1 sets topic text 12 px inside
         * the window (pixel scan, TTTEST); RichEdit's own inset is ~5 px.
         * Popups have their measured padding instead (gap 5). */
        if (!bPopup)
            SendMessageW(hTextWnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(7, 0));
        SendMessageW(hTextWnd, EM_SETEVENTMASK, 0,
                    SendMessageW(hTextWnd, EM_GETEVENTMASK, 0, 0) | ENM_MOUSEEVENTS);
        win->origRicheditWndProc = (WNDPROC)SetWindowLongPtrA(hTextWnd, GWLP_WNDPROC,
                                                             (LONG_PTR)WINHELP_RicheditWndProc);
        /* TELETEXT PORT (gap 2): the non-scrolling band (hidden until a
         * topic has one) and the 1px rule under it; not in popups. */
        if (!bPopup)
        {
            HWND hNsr = CreateWindowA(Globals.tt_msftedit ? "RICHEDIT50W" : RICHEDIT_CLASS20A, NULL,
                                      ES_MULTILINE | ES_READONLY | WS_CHILD,
                                      0, 0, 0, 0, win->hMainWnd, (HMENU)CTL_ID_NSR, Globals.hInstance, NULL);
            TT_RichEditEnablePictures(hNsr);
            SendMessageW(hNsr, EM_SETEDITSTYLE, SES_HIDEGRIDLINES, SES_HIDEGRIDLINES);
            SendMessageW(hNsr, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(7, 0));
            SendMessageW(hNsr, EM_SETEVENTMASK, 0,
                         SendMessageW(hNsr, EM_GETEVENTMASK, 0, 0) | ENM_MOUSEEVENTS | ENM_REQUESTRESIZE);
            SetWindowLongPtrA(hNsr, GWLP_WNDPROC, (LONG_PTR)WINHELP_RicheditWndProc);
            CreateWindowA("STATIC", "", WS_CHILD | SS_OWNERDRAW,   /* painted black in WM_DRAWITEM: SS_BLACKRECT is #696969 on Windows 11 */
                          0, 0, 0, 0, win->hMainWnd, (HMENU)CTL_ID_RULE, Globals.hInstance, NULL);
        }
    }

    hIcon = (wpage->page) ? wpage->page->file->hIcon : NULL;
    if (!hIcon) hIcon = LoadImageW(Globals.hInstance, MAKEINTRESOURCEW(IDI_WINHELP), IMAGE_ICON,
                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED);
    SendMessageW(win->hMainWnd, WM_SETICON, ICON_SMALL, (DWORD_PTR)hIcon);

    /* Initialize file specific pushbuttons */
    if (!(wpage->wininfo->win_style & WS_POPUP) && wpage->page)
    {
        HLPFILE_MACRO  *macro;
        BOOL            alive = TRUE;
        /* TELETEXT PORT: a macro may delete this window, and with it the
         * last reference to its help file (e.g. NetManage XoftWare
         * TUTORIAL.HLP: JumpId into its own secondary window, whose [CONFIG]
         * run then performs CloseWindow("main") once more). Wine went on
         * executing the remaining macros against the freed window and
         * walking the freed file's macro list: a use-after-free that
         * crashed the viewer. Stop as soon as the window is gone. */
        for (macro = wpage->page->file->first_macro; macro && alive; macro = macro->next)
        {
            MACRO_ExecuteMacro(win, macro->lpszMacro);
            alive = WINHELP_IsWindowAlive(win);
        }

        for (macro = alive ? wpage->page->first_macro : NULL; macro && alive; macro = macro->next)
        {
            MACRO_ExecuteMacro(win, macro->lpszMacro);
            alive = WINHELP_IsWindowAlive(win);
        }
    }
    /* See #17681, in some cases, the newly created window is closed by the macros it contains
     * (braindead), so deal with this case
     */
    for (win = Globals.win_list; win; win = win->next)
    {
        if (!lstrcmpiA(win->info->name, wpage->wininfo->name)) break;
    }
    if (!win || !WINHELP_ReleaseWindow(win)) return TRUE;

    if (bPopup)
    {
        DWORD   mask = SendMessageW(hTextWnd, EM_GETEVENTMASK, 0, 0);

        win->font_scale = Globals.active_win->font_scale;
        WINHELP_SetupText(hTextWnd, win, wpage->relative);

        TT_FitPopup(win);   /* TELETEXT PORT (gap 5): width to the text, centred (before it shows) */
        /* we need the window to be shown for richedit to compute the size */
        ShowWindow(win->hMainWnd, nCmdShow);
        SendMessageW(hTextWnd, EM_SETEVENTMASK, 0, mask | ENM_REQUESTRESIZE);
        SendMessageW(hTextWnd, EM_REQUESTRESIZE, 0, 0);
        SendMessageW(hTextWnd, EM_SETEVENTMASK, 0, mask);
        TT_CreateShadow(win);   /* TELETEXT PORT (gap 5): the 3.1 drop shadow */
    }
    else
    {
        WINHELP_SetupText(hTextWnd, win, wpage->relative);
        WINHELP_LayoutMainWindow(win);
        ShowWindow(win->hMainWnd, nCmdShow);
    }

    return TRUE;
}

/******************************************************************
 *             WINHELP_OpenHelpWindow
 * Main function to search for a page and display it in a window
 */
BOOL WINHELP_OpenHelpWindow(HLPFILE_PAGE* (*lookup)(HLPFILE*, LONG, ULONG*),
                            HLPFILE* hlpfile, LONG val, HLPFILE_WINDOWINFO* wi,
                            int nCmdShow)
{
    WINHELP_WNDPAGE     wpage;

    wpage.page = lookup(hlpfile, val, &wpage.relative);
    if (wpage.page) wpage.page->file->wRefCount++;
    wpage.wininfo = wi;
    return WINHELP_CreateHelpWindow(&wpage, nCmdShow, TRUE);
}

/******************************************************************
 *             WINHELP_HandleTextMouse
 *
 */
static BOOL WINHELP_HandleTextMouse(WINHELP_WINDOW* win, UINT msg, LPARAM lParam)
{
    HLPFILE*                hlpfile;
    HLPFILE_LINK*           link;
    BOOL                    ret = FALSE;

    switch (msg)
    {
    case WM_LBUTTONDOWN:
        if ((link = WINHELP_FindLink(win, lParam)))
        {
            HLPFILE_WINDOWINFO*     wi;

            switch (link->cookie)
            {
            case hlp_link_link:
                if ((hlpfile = WINHELP_LookupHelpFile(link->string)))
                {
                    if (link->window == -1)
                    {
                        wi = win->info;
                        if (wi->win_style & WS_POPUP) wi = Globals.active_win->info;
                    }
                    else if (link->window < hlpfile->numWindows)
                        wi = &hlpfile->windows[link->window];
                    else
                    {
                        WINE_WARN("link to window %d/%d\n", link->window, hlpfile->numWindows);
                        break;
                    }
                    WINHELP_OpenHelpWindow(HLPFILE_PageByHash, hlpfile, link->hash, wi, SW_NORMAL);
                }
                break;
            case hlp_link_popup:
                if ((hlpfile = WINHELP_LookupHelpFile(link->string)))
                    WINHELP_OpenHelpWindow(HLPFILE_PageByHash, hlpfile, link->hash,
                                           WINHELP_GetPopupWindowInfo(hlpfile, win, lParam),
                                           SW_NORMAL);
                break;
            case hlp_link_macro:
                MACRO_ExecuteMacro(win, link->string);
                break;
            default:
                WINE_FIXME("Unknown link cookie %d\n", link->cookie);
            }
            ret = TRUE;
        }
        break;
    }
    return ret;
}

/***********************************************************************
 *
 *           WINHELP_CheckPopup
 */
static BOOL WINHELP_CheckPopup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT* lret)
{
    WINHELP_WINDOW*     popup;

    if (!Globals.active_popup) return FALSE;

    switch (msg)
    {
    case WM_NOTIFY:
        {
            MSGFILTER*  msgf = (MSGFILTER*)lParam;
            if (msgf->nmhdr.code == EN_MSGFILTER)
            {
                if (!WINHELP_CheckPopup(hWnd, msgf->msg, msgf->wParam, msgf->lParam, NULL))
                    return FALSE;
                if (lret) *lret = 1;
                return TRUE;
            }
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE || (HWND)lParam == Globals.active_win->hMainWnd ||
            (HWND)lParam == Globals.active_popup->hMainWnd ||
            GetWindow((HWND)lParam, GW_OWNER) == Globals.active_win->hMainWnd)
            break;
        /* fall through */
    case WM_LBUTTONDOWN:
        if (msg == WM_LBUTTONDOWN)
            WINHELP_HandleTextMouse(Globals.active_popup, msg, lParam);
        /* fall through */
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
        popup = Globals.active_popup;
        Globals.active_popup = NULL;
        WINHELP_ReleaseWindow(popup);
        if (lret) *lret = 1;
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *
 *           WINHELP_ButtonWndProc
 */
static LRESULT CALLBACK WINHELP_ButtonWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (WINHELP_CheckPopup(hWnd, msg, wParam, lParam, NULL)) return 0;

    if (msg == WM_KEYDOWN)
    {
        switch (wParam)
        {
        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_ESCAPE:
            return SendMessageA(GetParent(hWnd), msg, wParam, lParam);
        }
    }

    return CallWindowProcA(Globals.button_proc, hWnd, msg, wParam, lParam);
}

/***********************************************************************
 *
 *           WINHELP_ButtonBoxWndProc
 */
static LRESULT CALLBACK WINHELP_ButtonBoxWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WINDOWPOS      *winpos;
    WINHELP_WINDOW *win;
    WINHELP_BUTTON *button;
    SIZE button_size;
    INT  x, y;

    if (WINHELP_CheckPopup(hWnd, msg, wParam, lParam, NULL)) return 0L;

    switch (msg)
    {
    case WM_WINDOWPOSCHANGING:
        winpos = (WINDOWPOS*) lParam;
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(GetParent(hWnd), 0);

        /* Update buttons */
        button_size.cx = 0;
        button_size.cy = 0;
        for (button = win->first_button; button; button = button->next)
	{
            HDC  hDc;
            SIZE textsize;
            if (!button->hWnd)
            {
                button->hWnd = CreateWindowA(STRING_BUTTON, button->lpszName,
                                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                            0, 0, 0, 0,
                                            hWnd, (HMENU) button->wParam,
                                            Globals.hInstance, 0);
                if (button->hWnd)
                {
                    if (Globals.button_proc == NULL)
                    {
                        NONCLIENTMETRICSW ncm;
                        Globals.button_proc = (WNDPROC) GetWindowLongPtrA(button->hWnd, GWLP_WNDPROC);

                        ncm.cbSize = sizeof(NONCLIENTMETRICSW);
                        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                              sizeof(NONCLIENTMETRICSW), &ncm, 0);
                        /* TELETEXT PORT (gap 3): WinHelp 3.1's buttons are
                         * Helv 8 bold = MS Sans Serif 8 bold (the bitmap
                         * face), not the Windows 11 menu font. */
                        (void)ncm;
                        Globals.hButtonFont = CreateFontA(-MulDiv(8, 96, 72), 0, 0, 0, FW_BOLD, 0, 0, 0,
                                                          ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                                          DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
                    }
                    SetWindowLongPtrA(button->hWnd, GWLP_WNDPROC, (LONG_PTR) WINHELP_ButtonWndProc);
                    if (button->tt_disabled) EnableWindow(button->hWnd, FALSE);   /* TELETEXT PORT */
                    if (Globals.hButtonFont)
                        SendMessageW(button->hWnd, WM_SETFONT, (WPARAM)Globals.hButtonFont, TRUE);
                }
            }
            hDc = GetDC(button->hWnd);
            /* TELETEXT PORT: measure with the button's own font (Wine
             * measured with the DC's default font) */
            {
                HFONT old = Globals.hButtonFont ? SelectObject(hDc, Globals.hButtonFont) : NULL;
                char vis[256];
                const char *q = button->lpszName;
                int n = 0;
                /* the '&' marks the mnemonic and is not drawn (Wine counted it) */
                for (; *q && n < (int)sizeof(vis) - 1; q++)
                    if (*q != '&' || q[1] == '&') vis[n++] = *q;
                vis[n] = 0;
                GetTextExtentPointA(hDc, vis, n, &textsize);
                if (old) SelectObject(hDc, old);
            }
            ReleaseDC(button->hWnd, hDc);

            button_size.cx = max(button_size.cx, textsize.cx + BUTTON_CX);
            button_size.cy = max(button_size.cy, textsize.cy + BUTTON_CY);
	}

        x = 0;
        y = 0;
        for (button = win->first_button; button; button = button->next)
	{
            SetWindowPos(button->hWnd, HWND_TOP, x, y, button_size.cx, button_size.cy, 0);

            if (x + 2 * button_size.cx <= winpos->cx)
                x += button_size.cx;
            else
                x = 0, y += button_size.cy;
	}
        winpos->cy = y + (x ? button_size.cy : 0);
        break;

    case WM_COMMAND:
        SendMessageW(GetParent(hWnd), msg, wParam, lParam);
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_UP:
        case VK_DOWN:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_ESCAPE:
            return SendMessageA(GetParent(hWnd), msg, wParam, lParam);
        }
        break;
    }

    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/******************************************************************
 *		WINHELP_HistoryWndProc
 *
 *
 */
static LRESULT CALLBACK WINHELP_HistoryWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WINHELP_WINDOW*     win;
    PAINTSTRUCT         ps;
    HDC                 hDc;
    TEXTMETRICW         tm;
    unsigned int        i;
    RECT                r;

    switch (msg)
    {
    case WM_NCCREATE:
        win = (WINHELP_WINDOW*)((LPCREATESTRUCTA)lParam)->lpCreateParams;
        SetWindowLongPtrW(hWnd, 0, (ULONG_PTR)win);
        win->hHistoryWnd = hWnd;
        break;
    case WM_CREATE:
        hDc = GetDC(hWnd);
        GetTextMetricsW(hDc, &tm);
        /* TELETEXT PORT: WinHelp 3.1 reopened History at H_WindowPosition.
         * Otherwise size it to 30 x history-depth characters beside the main
         * window, on the main window's monitor. Wine created it at (0,0) of
         * the primary monitor, clamped negative coordinates to 0, and passed
         * right/bottom to MoveWindow as width/height. */
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        if (TT_LoadWindowPos("H_WindowPosition", &r, NULL))
        {
            MoveWindow(hWnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
            ReleaseDC(hWnd, hDc);
            break;
        }
        {
            RECT wa, main_rc;
            HWND main_wnd = win ? win->hMainWnd : NULL;
            TT_WorkAreaFromWindow(main_wnd, &wa);
            if (!main_wnd || !GetWindowRect(main_wnd, &main_rc)) main_rc = wa;
            SetRect(&r, 0, 0, 30 * tm.tmAveCharWidth,
                    ARRAY_SIZE(Globals.history.set) * tm.tmHeight);
            AdjustWindowRect(&r, GetWindowLongW(hWnd, GWL_STYLE), FALSE);
            OffsetRect(&r, -r.left, -r.top);
            OffsetRect(&r, main_rc.right, main_rc.top);
            if (r.right > wa.right) OffsetRect(&r, wa.right - r.right, 0);
            if (r.left < wa.left) OffsetRect(&r, wa.left - r.left, 0);
            if (r.bottom > wa.bottom) OffsetRect(&r, 0, wa.bottom - r.bottom);
            if (r.top < wa.top) OffsetRect(&r, 0, wa.top - r.top);
        }
        MoveWindow(hWnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
        ReleaseDC(hWnd, hDc);
        break;
    /* TELETEXT PORT (phase 2a, item 8): the History list behaves as WinHelp
     * 3.1's list box: a click selects, a double-click or Enter opens, Up and
     * Down move the navy highlight. Wine opened the topic on a single click
     * and highlighted nothing. */
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        hDc = GetDC(hWnd);
        GetTextMetricsW(hDc, &tm);
        i = HIWORD(lParam) / tm.tmHeight;
        ReleaseDC(hWnd, hDc);
        if (i < Globals.history.index)
        {
            tt_hist_sel = i;
            InvalidateRect(hWnd, NULL, TRUE);
            if (msg == WM_LBUTTONDBLCLK)
                WINHELP_CreateHelpWindow(&Globals.history.set[i], SW_SHOW, TRUE);
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_UP && tt_hist_sel > 0) { tt_hist_sel--; InvalidateRect(hWnd, NULL, TRUE); }
        else if (wParam == VK_DOWN && tt_hist_sel + 1 < Globals.history.index) { tt_hist_sel++; InvalidateRect(hWnd, NULL, TRUE); }
        else if (wParam == VK_RETURN && tt_hist_sel < Globals.history.index)
            WINHELP_CreateHelpWindow(&Globals.history.set[tt_hist_sel], SW_SHOW, TRUE);
        break;
    case WM_PAINT:
        hDc = BeginPaint(hWnd, &ps);
        GetTextMetricsW(hDc, &tm);

        for (i = 0; i < Globals.history.index; i++)
        {
            /* TELETEXT PORT (item 8): navy highlight, 2 px left margin */
            SetBkMode(hDc, TRANSPARENT);
            if (i == tt_hist_sel)
            {
                RECT row;
                HBRUSH navy = CreateSolidBrush(RGB(0, 0, 128));
                GetClientRect(hWnd, &row);
                row.top = i * tm.tmHeight; row.bottom = row.top + tm.tmHeight;
                FillRect(hDc, &row, navy);
                DeleteObject(navy);
                SetTextColor(hDc, RGB(255, 255, 255));
            }
            else
                SetTextColor(hDc, RGB(0, 0, 0));
            if (Globals.history.set[i].page->file == Globals.active_win->page->file)
            {
                TextOutA(hDc, 2, i * tm.tmHeight,
                        Globals.history.set[i].page->lpszTitle,
                        strlen(Globals.history.set[i].page->lpszTitle));
            }
            else
            {
                char        buffer[1024];
                const char* ptr1;
                const char* ptr2;
                unsigned    len;

                ptr1 = strrchr(Globals.history.set[i].page->file->lpszPath, '\\');
                if (!ptr1) ptr1 = Globals.history.set[i].page->file->lpszPath;
                else ptr1++;
                ptr2 = strrchr(ptr1, '.');
                len = ptr2 ? ptr2 - ptr1 : strlen(ptr1);
                if (len > sizeof(buffer)) len = sizeof(buffer);
                memcpy(buffer, ptr1, len);
                if (len < sizeof(buffer)) buffer[len++] = ':';
                lstrcpynA(&buffer[len], Globals.history.set[i].page->lpszTitle, sizeof(buffer) - len);
                TextOutA(hDc, 2, i * tm.tmHeight, buffer, strlen(buffer));
            }
        }
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        TT_SaveWindowPos("H_WindowPosition", hWnd);   /* TELETEXT PORT */
        if (hWnd == win->hHistoryWnd)
            win->hHistoryWnd = 0;
        break;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/**************************************************************************
 * cb_KWBTree
 *
 * HLPFILE_BPTreeCallback enumeration function for '|KWBTREE' internal file.
 *
 */
static void cb_KWBTree(void *p, void **next, void *cookie)
{
    HWND hListWnd = cookie;
    int count;

    WINE_TRACE("Adding %s to search list\n", debugstr_a((char *)p));
    SendMessageA(hListWnd, LB_INSERTSTRING, -1, (LPARAM)p);
    count = SendMessageW(hListWnd, LB_GETCOUNT, 0, 0);
    SendMessageW(hListWnd, LB_SETITEMDATA, count-1, (LPARAM)p);
    *next = (char*)p + strlen((char*)p) + 7;
}

struct index_data
{
    HLPFILE*    hlpfile;
    BOOL        jump;
    ULONG       offset;
};

/**************************************************************************
 * WINHELP_IndexDlgProc
 *
 */
static INT_PTR CALLBACK WINHELP_IndexDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static struct index_data* id;
    int sel;

    switch (msg)
    {
    case WM_INITDIALOG:
        id = (struct index_data*)((PROPSHEETPAGEA*)lParam)->lParam;
        HLPFILE_BPTreeEnum(id->hlpfile->kwbtree, cb_KWBTree,
                           GetDlgItem(hWnd, IDC_INDEXLIST));
        id->jump = FALSE;
        id->offset = 1;
        return TRUE;
    case WM_COMMAND:
        switch (HIWORD(wParam))
        {
        case LBN_DBLCLK:
            if (LOWORD(wParam) == IDC_INDEXLIST)
                SendMessageW(GetParent(hWnd), PSM_PRESSBUTTON, PSBTN_OK, 0);
            break;
        }
        break;
    case WM_NOTIFY:
	switch (((NMHDR*)lParam)->code)
	{
	case PSN_APPLY:
            sel = SendDlgItemMessageW(hWnd, IDC_INDEXLIST, LB_GETCURSEL, 0, 0);
            if (sel != LB_ERR)
            {
                BYTE *p;
                int count;

                p = (BYTE*)SendDlgItemMessageW(hWnd, IDC_INDEXLIST, LB_GETITEMDATA, sel, 0);
                count = *(short*)((char *)p + strlen((char *)p) + 1);
                if (count > 1)
                {
                    MessageBoxA(hWnd, "count > 1 not supported yet", "Error", MB_OK | MB_ICONSTOP);
                    SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }
                id->offset = *(ULONG*)((char *)p + strlen((char *)p) + 3);
                id->offset = *(long*)(id->hlpfile->kwdata + id->offset + 9);
                if (id->offset == 0xFFFFFFFF)
                {
                    MessageBoxA(hWnd, "macro keywords not supported yet", "Error", MB_OK | MB_ICONSTOP);
                    SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, PSNRET_INVALID);
                    return TRUE;
                }
                id->jump = TRUE;
                SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            }
            return TRUE;
        default:
            return FALSE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/**************************************************************************
 * WINHELP_SearchDlgProc
 *
 */
static INT_PTR CALLBACK WINHELP_SearchDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        return TRUE;
    case WM_NOTIFY:
	switch (((NMHDR*)lParam)->code)
	{
	case PSN_APPLY:
            SetWindowLongPtrW(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);
            return TRUE;
        default:
            return FALSE;
        }
        break;
    default:
        break;
    }
    return FALSE;
}

/***********************************************************************
 *
 *           WINHELP_MainWndProc
 */
/* TELETEXT PORT: one character to lower case, the Windows way (ANSI) */
static char tt_lower(BYTE c)
{
    return (char)(BYTE)(ULONG_PTR)CharLowerA((LPSTR)(ULONG_PTR)c);
}

static LRESULT CALLBACK WINHELP_MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WINHELP_WINDOW *win;
    WINHELP_BUTTON *button;
    HWND hTextWnd;
    LRESULT ret;

    if (WINHELP_CheckPopup(hWnd, msg, wParam, lParam, &ret)) return ret;

    switch (msg)
    {
    case WM_NCCREATE:
        win = (WINHELP_WINDOW*) ((LPCREATESTRUCTA) lParam)->lpCreateParams;
        SetWindowLongPtrW(hWnd, 0, (ULONG_PTR) win);
        if (!win->page && Globals.isBook)
            PostMessageW(hWnd, WM_COMMAND, MNID_FILE_OPEN, 0);
        win->hMainWnd = hWnd;
        break;

    case WM_WINDOWPOSCHANGED:
        WINHELP_LayoutMainWindow((WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0));
        break;

    /* TELETEXT PORT (gap 5): a popup is its background colour inside a 2 px
     * black border (the real WinHelp's), not a Windows frame */
    case WM_ERASEBKGND:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        if (tt_is_popup(win))
        {
            RECT rc;
            HBRUSH bg = CreateSolidBrush(win->info->sr_color);
            int i;
            GetClientRect(hWnd, &rc);
            FillRect((HDC)wParam, &rc, bg);
            DeleteObject(bg);
            for (i = 0; i < TT_POP_BORDER; i++)
            {
                FrameRect((HDC)wParam, &rc, GetStockObject(BLACK_BRUSH));
                InflateRect(&rc, -1, -1);
            }
            return 1;
        }
        break;

    /* TELETEXT PORT (gap 2): the rule under the non-scrolling band is solid
     * black, as in WinHelp 3.1 (pixel-checked against the VM) */
    case WM_DRAWITEM:
        if (wParam == CTL_ID_RULE)
        {
            const DRAWITEMSTRUCT *di = (const DRAWITEMSTRUCT *)lParam;
            FillRect(di->hDC, &di->rcItem, GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        break;

    /* TELETEXT PORT (phase 2a, gap 3): Alt+letter presses a button-bar
     * button, as in WinHelp 3.1 (Alt+C Contents, Alt+S Search, Alt+B Back,
     * Alt+T History, Alt+< Alt+>, and a file's own buttons, e.g. Alt+G
     * Glossary). The menu bar's mnemonics (F E M H) are tried first and
     * left to Windows; Wine had no button mnemonics at all. */
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU && lParam > ' ' &&
            (win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0)))
        {
            char ch = tt_lower((BYTE)lParam);
            HMENU menu = GetMenu(hWnd);
            int i, n = menu ? GetMenuItemCount(menu) : 0;
            BOOL in_menu = FALSE;
            for (i = 0; i < n && !in_menu; i++)
            {
                char item[64], *amp;
                if (GetMenuStringA(menu, i, item, sizeof(item), MF_BYPOSITION) &&
                    (amp = strchr(item, '&')) && amp[1] &&
                    tt_lower((BYTE)amp[1]) == ch)
                    in_menu = TRUE;
            }
            if (!in_menu)
            {
                for (button = win->first_button; button; button = button->next)
                {
                    const char *amp = strchr(button->lpszName, '&');
                    if (amp && amp[1] && tt_lower((BYTE)amp[1]) == ch)
                    {
                        if (button->hWnd && IsWindowEnabled(button->hWnd))
                            PostMessageA(hWnd, WM_COMMAND, button->wParam, 0);
                        else
                            MessageBeep(0);
                        return 0;
                    }
                }
            }
        }
        break;

    case WM_COMMAND:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        switch (LOWORD(wParam))
	{
            /* Menu FILE */
	case MNID_FILE_OPEN:    MACRO_FileOpen();       break;
	case MNID_FILE_PRINT:	MACRO_Print();          break;
	case MNID_FILE_SETUP:	MACRO_PrinterSetup();   break;
	case MNID_FILE_EXIT:	MACRO_Exit();           break;

            /* Menu EDIT */
	case MNID_EDIT_COPYDLG:
            SendDlgItemMessageW(hWnd, CTL_ID_TEXT, WM_COPY, 0, 0);
            break;
	case MNID_EDIT_ANNOTATE:MACRO_Annotate();       break;

            /* Menu Bookmark */
	case MNID_BKMK_DEFINE:  MACRO_BookmarkDefine(); break;

            /* Menu Help */
	case MNID_HELP_HELPON:	MACRO_HelpOn();         break;
	case MNID_HELP_HELPTOP: MACRO_HelpOnTop();      break;
	case MNID_HELP_ABOUT:	MACRO_About();          break;

            /* Context help */
        case MNID_CTXT_ANNOTATE:MACRO_Annotate();       break;
        case MNID_CTXT_COPY:    MACRO_CopyDialog();     break;
        case MNID_CTXT_PRINT:   MACRO_Print();          break;
        case MNID_OPTS_HISTORY: MACRO_History();        break;
        case MNID_OPTS_FONTS_SMALL:
        case MNID_CTXT_FONTS_SMALL:
            win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
            if (win->font_scale != 0)
            {
                win->font_scale = 0;
                WINHELP_SetupText(GetDlgItem(hWnd, CTL_ID_TEXT), win, 0 /* FIXME */);
            }
            break;
        case MNID_OPTS_FONTS_NORMAL:
        case MNID_CTXT_FONTS_NORMAL:
            win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
            if (win->font_scale != 1)
            {
                win->font_scale = 1;
                WINHELP_SetupText(GetDlgItem(hWnd, CTL_ID_TEXT), win, 0 /* FIXME */);
            }
            break;
        case MNID_OPTS_FONTS_LARGE:
        case MNID_CTXT_FONTS_LARGE:
            win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
            if (win->font_scale != 2)
            {
                win->font_scale = 2;
                WINHELP_SetupText(GetDlgItem(hWnd, CTL_ID_TEXT), win, 0 /* FIXME */);
            }
            break;

	default:
            /* Buttons */
            for (button = win->first_button; button; button = button->next)
                if (wParam == button->wParam) break;
            if (button)
                MACRO_ExecuteMacro(win, button->lpszMacro);
            else if (!HIWORD(wParam))
            {
                /* TELETEXT PORT: load the strings; Windows' MessageBoxW takes
                 * no resource IDs (Wine's does). */
                WCHAR text[128], caption[64];
                LoadStringW(Globals.hInstance, STID_NOT_IMPLEMENTED, text, ARRAY_SIZE(text));
                LoadStringW(Globals.hInstance, STID_WHERROR, caption, ARRAY_SIZE(caption));
                MessageBoxW(0, text, caption, MB_OK);
            }
            break;
	}
        break;
/* EPP     case WM_DESTROY: */
/* EPP         if (Globals.hPopupWnd) DestroyWindow(Globals.hPopupWnd); */
/* EPP         break; */
    case WM_COPYDATA:
        return WINHELP_HandleCommand((HWND)wParam, lParam);

    case WM_CHAR:
        if (wParam == 3)
        {
            SendDlgItemMessageW(hWnd, CTL_ID_TEXT, WM_COPY, 0, 0);
            return 0;
        }
        break;

    case WM_KEYDOWN:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        hTextWnd = GetDlgItem(win->hMainWnd, CTL_ID_TEXT);

        switch (wParam)
        {
        case VK_UP:
            SendMessageW(hTextWnd, EM_SCROLL, SB_LINEUP, 0);
            return 0;
        case VK_DOWN:
            SendMessageW(hTextWnd, EM_SCROLL, SB_LINEDOWN, 0);
            return 0;
        case VK_PRIOR:
            SendMessageW(hTextWnd, EM_SCROLL, SB_PAGEUP, 0);
            return 0;
        case VK_NEXT:
            SendMessageW(hTextWnd, EM_SCROLL, SB_PAGEDOWN, 0);
            return 0;
        case VK_ESCAPE:
            MACRO_Exit();
            return 0;
        }
        break;

    case WM_NOTIFY:
        /* TELETEXT PORT (gap 2): the band reports its content height and
         * forwards mouse messages for its own hotspots. */
        if (wParam == CTL_ID_NSR)
        {
            win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
            if (((NMHDR*)lParam)->code == EN_REQUESTRESIZE && win)
            {
                const RECT *rq = &((REQRESIZE*)lParam)->rc;
                int h = rq->bottom - rq->top;
                if (h != win->tt_nsr_h)
                {
                    win->tt_nsr_h = h;
                    if (win->tt_nsr_on) WINHELP_LayoutMainWindow(win);
                }
            }
            else if (((NMHDR*)lParam)->code == EN_MSGFILTER && win)
            {
                const MSGFILTER* msgf = (const MSGFILTER*)lParam;
                WINE_TRACE("TT NSR msgfilter %x\n", msgf->msg);
                if (msgf->msg == WM_LBUTTONDOWN)
                {
                    BOOL r;
                    POINT zero = { 0, 0 };
                    tt_link_ctl = CTL_ID_NSR;
                    r = WINHELP_HandleTextMouse(win, msgf->msg, msgf->lParam);
                    tt_link_ctl = CTL_ID_TEXT;
                    /* the band never scrolls, whatever the click did */
                    if (WINHELP_IsWindowAlive(win) && GetDlgItem(hWnd, CTL_ID_NSR))
                        SendMessageW(GetDlgItem(hWnd, CTL_ID_NSR), EM_SETSCROLLPOS, 0, (LPARAM)&zero);
                    return r;
                }
            }
            break;
        }
        if (wParam == CTL_ID_TEXT)
        {
            RECT        rc;

            switch (((NMHDR*)lParam)->code)
            {
            case EN_MSGFILTER:
                {
                    const MSGFILTER*    msgf = (const MSGFILTER*)lParam;
                    switch (msgf->msg)
                    {
                    case WM_KEYUP:
                        if (msgf->wParam == VK_ESCAPE)
                            WINHELP_ReleaseWindow((WINHELP_WINDOW*)GetWindowLongPtrW(hWnd, 0));
                        break;
                    case WM_RBUTTONDOWN:
                    {
                        HMENU       hMenu;
                        POINT       pt;

                        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
                        hMenu = LoadMenuW(Globals.hInstance, MAKEINTRESOURCEW(CONTEXT_MENU));
                        switch (win->font_scale)
                        {
                        case 0:
                            CheckMenuItem(hMenu, MNID_CTXT_FONTS_SMALL,
                                          MF_BYCOMMAND|MF_CHECKED);
                            break;
                        default:
                            WINE_FIXME("Unsupported %d\n", win->font_scale);
                            /* fall through */
                        case 1:
                            CheckMenuItem(hMenu, MNID_CTXT_FONTS_NORMAL,
                                          MF_BYCOMMAND|MF_CHECKED);
                            break;
                        case 2:
                            CheckMenuItem(hMenu, MNID_CTXT_FONTS_LARGE,
                                          MF_BYCOMMAND|MF_CHECKED);
                            break;
                        }
                        pt.x = (int)(short)LOWORD(msgf->lParam);
                        pt.y = (int)(short)HIWORD(msgf->lParam);
                        ClientToScreen(msgf->nmhdr.hwndFrom, &pt);
                        TrackPopupMenu(GetSubMenu(hMenu, 0), TPM_LEFTALIGN|TPM_TOPALIGN,
                                       pt.x, pt.y, 0, hWnd, NULL);
                        DestroyMenu(hMenu);
                    }
                    break;
                    default:
                        return WINHELP_HandleTextMouse((WINHELP_WINDOW*)GetWindowLongPtrW(hWnd, 0),
                                                       msgf->msg, msgf->lParam);
                    }
                }
                break;

            case EN_REQUESTRESIZE:
                rc = ((REQRESIZE*)lParam)->rc;
                win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
                if (tt_is_popup(win))
                {
                    /* TELETEXT PORT (gap 5): keep the fitted width; the
                     * height is the text's plus border and padding, and
                     * the popup stays on the work area */
                    RECT wr, wa;
                    int h = rc.bottom - rc.top + 2 * (TT_POP_BORDER + TT_POP_PADY);
                    GetWindowRect(win->hMainWnd, &wr);
                    TT_WorkAreaFromWindow(win->hMainWnd, &wa);
                    wr.top = tt_popup_click.y + 9;           /* always from the click */
                    if (wr.top + h > wa.bottom - 2) wr.top = max(wa.top + 2, wa.bottom - 2 - h);
                    SetWindowPos(win->hMainWnd, HWND_TOP, wr.left, wr.top,
                                 wr.right - wr.left, h, SWP_NOZORDER);
                    WINHELP_LayoutMainWindow(win);
                    TT_PlaceShadow(win);
                    break;
                }
                AdjustWindowRect(&rc, GetWindowLongW(win->hMainWnd, GWL_STYLE),
                                 FALSE);
                SetWindowPos(win->hMainWnd, HWND_TOP, 0, 0,
                             rc.right - rc.left, rc.bottom - rc.top,
                             SWP_NOMOVE | SWP_NOZORDER);
                WINHELP_LayoutMainWindow(win);
                break;
            }
        }
        break;

    case WM_INITMENUPOPUP:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        CheckMenuItem((HMENU)wParam, MNID_OPTS_FONTS_SMALL,
                      (win->font_scale == 0) ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem((HMENU)wParam, MNID_OPTS_FONTS_NORMAL,
                      (win->font_scale == 1) ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem((HMENU)wParam, MNID_OPTS_FONTS_LARGE,
                      (win->font_scale == 2) ? MF_CHECKED : MF_UNCHECKED);
        break;
    case WM_DESTROY:
        win = (WINHELP_WINDOW*) GetWindowLongPtrW(hWnd, 0);
        /* TELETEXT PORT: remember the main window for next time, as WinHelp
         * 3.1 did in WIN.INI (see ttport.c). */
        if (win && win->info && !lstrcmpiA(win->info->name, "main"))
            TT_SaveWindowPos("M_WindowPosition", hWnd);
        WINHELP_DeleteWindow(win);
        break;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

/**************************************************************************
 * WINHELP_CreateIndexWindow
 *
 * Displays a dialog with keywords of current help file.
 *
 */
BOOL WINHELP_CreateIndexWindow(BOOL is_search)
{
    HPROPSHEETPAGE      psPage[3];
    PROPSHEETPAGEA      psp;
    PROPSHEETHEADERA    psHead;
    struct index_data   id;
    char                buf[256];

    if (Globals.active_win && Globals.active_win->page && Globals.active_win->page->file)
        id.hlpfile = Globals.active_win->page->file;
    else
        return FALSE;

    if (id.hlpfile->kwbtree == NULL)
    {
        WINE_TRACE("No index provided\n");
        return FALSE;
    }

    /* TELETEXT PORT (phase 2a, gap 1): WinHelp 3.1 has one modal Search
     * dialog (Search button, Search() and Finder() macros), and it handles
     * keywords with several topics. ttsearch.c reproduces it; Wine's
     * Index/Search property sheet below is kept but no longer reached. */
    if (!getenv("TTWINHLP_WINE_INDEX"))
    {
        LONG offset;
        if (TT_SearchDialog(Globals.active_win->hMainWnd, id.hlpfile, &offset))
            WINHELP_OpenHelpWindow(HLPFILE_PageByOffset, id.hlpfile, offset,
                                   Globals.active_win->info, SW_NORMAL);
        return TRUE;
    }

    InitCommonControls();

    id.jump = FALSE;
    memset(&psp, 0, sizeof(psp));
    psp.dwSize = sizeof(psp);
    psp.dwFlags = 0;
    psp.hInstance = Globals.hInstance;

    psp.pszTemplate = MAKEINTRESOURCEA(IDD_INDEX);
    psp.lParam = (LPARAM)&id;
    psp.pfnDlgProc = WINHELP_IndexDlgProc;
    psPage[0] = CreatePropertySheetPageA(&psp);

    psp.pszTemplate = MAKEINTRESOURCEA(IDD_SEARCH);
    psp.lParam = (LPARAM)&id;
    psp.pfnDlgProc = WINHELP_SearchDlgProc;
    psPage[1] = CreatePropertySheetPageA(&psp);

    memset(&psHead, 0, sizeof(psHead));
    psHead.dwSize = sizeof(psHead);

    LoadStringA(Globals.hInstance, STID_PSH_INDEX, buf, sizeof(buf));
    strcat(buf, Globals.active_win->info->caption);

    psHead.pszCaption = buf;
    psHead.nPages = 2;
    psHead.nStartPage = is_search ? 1 : 0;
    psHead.hwndParent = Globals.active_win->hMainWnd;
    psHead.phpage = psPage;
    psHead.dwFlags = PSH_NOAPPLYNOW;

    PropertySheetA(&psHead);
    if (id.jump)
    {
        WINE_TRACE("got %ld as an offset\n", id.offset);
        WINHELP_OpenHelpWindow(HLPFILE_PageByOffset, id.hlpfile, id.offset,
                               Globals.active_win->info, SW_NORMAL);
    }
    return TRUE;
}

/***********************************************************************
 *
 *           RegisterWinClasses
 */
static BOOL WINHELP_RegisterWinClasses(void)
{
    WNDCLASSEXA class_main, class_button_box, class_history;

    class_main.cbSize              = sizeof(class_main);
    class_main.style               = CS_HREDRAW | CS_VREDRAW;
    class_main.lpfnWndProc         = WINHELP_MainWndProc;
    class_main.cbClsExtra          = 0;
    class_main.cbWndExtra          = sizeof(WINHELP_WINDOW *);
    class_main.hInstance           = Globals.hInstance;
    class_main.hIcon               = LoadIconW(Globals.hInstance, MAKEINTRESOURCEW(IDI_WINHELP));
    class_main.hCursor             = LoadCursorW(0, (LPWSTR)IDC_ARROW);
    class_main.hbrBackground       = (HBRUSH)(COLOR_WINDOW+1);
    class_main.lpszMenuName        = 0;
    class_main.lpszClassName       = MAIN_WIN_CLASS_NAME;
    class_main.hIconSm             = LoadImageW(Globals.hInstance, MAKEINTRESOURCEW(IDI_WINHELP), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                               LR_SHARED);

    class_button_box               = class_main;
    class_button_box.lpfnWndProc   = WINHELP_ButtonBoxWndProc;
    class_button_box.cbWndExtra    = 0;
    class_button_box.hbrBackground = GetStockObject(GRAY_BRUSH); /* TELETEXT PORT: 3.1 bar is #808080 */
    class_button_box.lpszClassName = BUTTON_BOX_WIN_CLASS_NAME;

    class_history                  = class_main;
    class_history.lpfnWndProc      = WINHELP_HistoryWndProc;
    class_history.lpszClassName    = HISTORY_WIN_CLASS_NAME;
    class_history.style           |= CS_DBLCLKS;   /* TELETEXT PORT (item 8): double-click opens */

    /* TELETEXT PORT (gap 5): the popup drop shadow */
    {
        WNDCLASSEXA cs;
        memset(&cs, 0, sizeof(cs));
        cs.cbSize        = sizeof(cs);
        cs.lpfnWndProc   = TT_ShadowWndProc;
        cs.hInstance     = Globals.hInstance;
        cs.hCursor       = LoadCursorW(0, (LPWSTR)IDC_ARROW);
        cs.lpszClassName = TT_SHADOW_CLASS;
        RegisterClassExA(&cs);
    }
    return (RegisterClassExA(&class_main) &&
            RegisterClassExA(&class_button_box) &&
            RegisterClassExA(&class_history));
}

/***********************************************************************
 *
 *           WinMain
 */
int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE prev, LPSTR cmdline, int show)
{
    MSG                 msg;
    LONG                lHash = 0;
    HLPFILE*            hlpfile;
    static CHAR         default_wndname[] = "main";
    LPSTR               wndname = default_wndname;
    WINHELP_DLL*        dll;
    HACCEL              hAccel;

    Globals.hInstance = hInstance;
    TT_InstallDialogPlacement();   /* TELETEXT PORT: dialogs on the session monitor */
    /* TELETEXT PORT (phase 2a, gap 4): the Windows 3.1 look from Navigator
     * (w31.c): its CTL3D-style thread hook restyles buttons, dialogs,
     * menus and scroll bars at birth; "plain" = Windows 3.1 as WinHelp drew
     * it (white dialogs, flat fields). TTWINHLP_W11=1 leaves Windows 11's
     * look, for diagnosis. */
    if (!getenv("TTWINHLP_W11"))
    {
        w31_set_plain(TRUE);
        w31_init();
        w31_hook_install();
    }
    /* TELETEXT PORT: Microsoft's RichEdit inserts RTF pictures as static
     * OLE objects and silently drops them when OLE is not initialised in
     * the process (Wine's RichEdit draws them itself, so Wine never called
     * this). Without it no bitmap in any help file appears. */
    OleInitialize(NULL);

    Globals.tt_msftedit = !getenv("TTWINHLP_RICHED20") && LoadLibraryA("msftedit.dll") != NULL;   /* TELETEXT PORT; env = diagnostic */
    if (LoadLibraryA("riched20.dll") == NULL)
    {
        /* TELETEXT PORT: Wine's MessageBoxW accepts resource IDs as the text
         * and caption; Windows' does not (it would read address 0x12F).
         * Load the strings first. */
        WCHAR text[256], caption[64];
        LoadStringW(hInstance, STID_NO_RICHEDIT, text, ARRAY_SIZE(text));
        LoadStringW(hInstance, STID_WHERROR, caption, ARRAY_SIZE(caption));
        return MessageBoxW(0, text, caption, MB_OK);
    }

    /* Get options */
    while (*cmdline && (*cmdline == ' ' || *cmdline == '-'))
    {
        CHAR   option;
        LPCSTR topic_id;
        if (*cmdline++ == ' ') continue;

        option = *cmdline;
        if (option) cmdline++;
        while (*cmdline == ' ') cmdline++;
        switch (option)
	{
	case 'i':
	case 'I':
            topic_id = cmdline;
            while (*cmdline && *cmdline != ' ') cmdline++;
            if (*cmdline) *cmdline++ = '\0';
            lHash = HLPFILE_Hash(topic_id);
            break;

	case '3':
	case '4':
            Globals.wVersion = option - '0';
            break;

        case 'x':
            show = SW_HIDE;
            Globals.isBook = FALSE;
            break;

        default:
            WINE_FIXME("Unsupported cmd line: %s\n", debugstr_a(cmdline));
            break;
	}
    }

    /* Create primary window */
    if (!WINHELP_RegisterWinClasses())
    {
        WINE_FIXME("Couldn't register classes\n");
        return 0;
    }

    if (*cmdline)
    {
        char*   ptr;
        if ((*cmdline == '"') && (ptr = strchr(cmdline+1, '"')))
        {
            cmdline++;
            *ptr = '\0';
        }
        if ((ptr = strchr(cmdline, '>')))
        {
            *ptr = '\0';
            wndname = ptr + 1;
        }
        hlpfile = WINHELP_LookupHelpFile(cmdline);
        if (!hlpfile) return 0;
    }
    else hlpfile = NULL;
    WINHELP_OpenHelpWindow(HLPFILE_PageByHash, hlpfile, lHash,
                           WINHELP_GetWindowInfo(hlpfile, wndname), show);

    /* Message loop */
    hAccel = LoadAcceleratorsW(hInstance, MAKEINTRESOURCEW(MAIN_ACCEL));
    while ((Globals.win_list || Globals.active_popup) && GetMessageW(&msg, 0, 0, 0))
    {
        HWND hWnd = Globals.active_win ? Globals.active_win->hMainWnd : NULL;
        if (!TranslateAcceleratorW(hWnd, hAccel, &msg))
	{
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    for (dll = Globals.dlls; dll; dll = dll->next)
    {
        if (dll->class & DC_INITTERM) dll->handler(DW_TERM, 0, 0);
    }
    return 0;
}
