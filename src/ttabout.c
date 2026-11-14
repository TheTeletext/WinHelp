/*
 * The Teletext WinHelp: About Help.
 *
 * TELETEXT PORT (phase 2a, gap 3). Wine called ShellAbout, which shows the
 * Windows 11 about box under the name "Wine Help". WinHelp 3.1 had its own
 * "About Help" box (icon left, lines of text with rules between, OK top
 * right; real_about.png from the WfW 3.11 VM). This one keeps that shape
 * and carries what the LGPL asks of us (phase 1 report s8, item 4): the
 * Wine project's credit, the licence, and where the source is.
 * Look as in ttsearch.c: MS Sans Serif 8 bold, white, black rules.
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#include <windows.h>
#include "winhelp.h"
#include "winhelp_res.h"
#include "ttport.h"

static HFONT tta_font;

static BOOL CALLBACK tta_setfont(HWND c, LPARAM lp)
{
    SendMessageA(c, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static INT_PTR CALLBACK tta_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        RECT wr, dr, wa;
        HWND owner = GetWindow(dlg, GW_OWNER);
        tta_font = CreateFontA(-MulDiv(8, 96, 72), 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                               VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
        EnumChildWindows(dlg, tta_setfont, (LPARAM)tta_font);
        /* real: near the top left of the help window (+34,+48 in the VM),
         * kept on the owner's monitor */
        if (owner)
        {
            GetWindowRect(owner, &wr);
            GetWindowRect(dlg, &dr);
            TT_WorkAreaFromWindow(owner, &wa);
            {
                int w = dr.right - dr.left, h = dr.bottom - dr.top;
                int x = wr.left + 34, y = wr.top + 48;
                if (x + w > wa.right) x = wa.right - w;
                if (y + h > wa.bottom) y = wa.bottom - h;
                if (x < wa.left) x = wa.left;
                if (y < wa.top) y = wa.top;
                SetWindowPos(dlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return TRUE;
    }
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, RGB(0, 0, 0));
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lp;
        if (di->CtlID == IDC_TTA_RULE1 || di->CtlID == IDC_TTA_RULE2)
        {
            FillRect(di->hDC, &di->rcItem, GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
        {
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        if (tta_font) { DeleteObject(tta_font); tta_font = NULL; }
        break;
    }
    return FALSE;
}

void TT_AboutDialog(HWND owner)
{
    DialogBoxParamA(Globals.hInstance, MAKEINTRESOURCEA(IDD_TT_ABOUT), owner, tta_proc, 0);
}
