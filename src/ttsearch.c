/*
 * The Teletext WinHelp: the WinHelp 3.1 Search dialog.
 *
 * TELETEXT PORT (phase 2a, gap 1). Wine's winhlp32 offers a two-page
 * property sheet ("Index" keyword list + a "Not yet implemented" Search
 * page) and refuses any keyword that leads to more than one topic ("count
 * > 1 not supported yet"). WinHelp 3.1 has one modal "Search" dialog; this
 * file reproduces it, modelled on the real WINHELP.EXE 3.10.425 in the
 * WfW 3.11 reference VM (screens D:\winhelp_corpus\p2a\real_s*.png):
 *
 *   Type a word, or select one from the list.      [ Cancel      ]
 *   Then choose Show Topics.                       [ Show Topics ]
 *   [edit..............................]
 *      [keyword list......................................][^]
 *   ------------------------------------------------------------
 *   Select a topic, then choose Go To.             [ Go To       ]
 *   [topic list................................................]
 *
 * Behaviour observed in the real dialog and reproduced here:
 *   - opens with an empty edit, the first keyword selected, Show Topics
 *     and Go To disabled;
 *   - typing selects the first keyword with that prefix (case-insensitive)
 *     and enables Show Topics, which becomes the default button;
 *   - picking a keyword copies it into the edit;
 *   - Show Topics puts the keyword's own spelling in the edit, fills the
 *     topic list with the titles of every topic that has the keyword
 *     SORTED alphabetically (GOPHER.HLP "Item": 13 topics, real list
 *     starts "Add Gopher Server, Close Item, Copy To File"; LBS_SORT in the
 *     template), selects the first, enables Go To, moves the focus there
 *     and renames Cancel to "Close";
 *   - editing afterwards leaves the topic list as it was;
 *   - the default button follows the focus: Show Topics in the upper half,
 *     Go To in the topic list;
 *   - double-click on a keyword = Show Topics, on a topic = Go To.
 * Font: Win 3.1 dialogs draw their FONT bold; "Helv 8" is "MS Sans Serif 8"
 * (the same bitmap face, sserife.fon) on Windows 11.
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#include <string.h>
#include <windows.h>
#include "winhelp.h"
#include "winhelp_res.h"
#include "ttport.h"

struct tt_search
{
    HLPFILE *hlpfile;
    LONG     offset;     /* chosen topic offset */
    HFONT    font;
    BOOL     shown;      /* topics listed at least once */
    BOOL     quiet;      /* we are setting the edit text ourselves */
    int      item_h;     /* list item height = font height */
    WNDPROC  edit_proc;
};

/* Client-area geometry in pixels, measured from the real dialog at 800x600
 * (s0/s1 screens; 1 guest pixel = 1 pixel here). */
static const struct { int id, x, y, w, h; } tt_layout[] =
{
    { IDC_TTS_PROMPT1, 7,   11, 270, 28 },
    { IDCANCEL,        286, 11, 94,  23 },
    { IDC_TTS_SHOW,    286, 39, 94,  23 },
    { IDC_TTS_EDIT,    6,   46, 259, 23 },
    { IDC_TTS_KEYS,    24,  68, 358, 81 },
    { IDC_TTS_RULE,    6,   158, 376, 2 },
    { IDC_TTS_PROMPT2, 7,   172, 270, 15 },
    { IDC_TTS_GOTO,    286, 167, 94,  23 },
    { IDC_TTS_TOPICS,  5,   202, 378, 92 },
};
#define TT_CLIENT_W 389
#define TT_BAND_IN  2   /* see tts_layout */
#define TT_CLIENT_H 300

static void tts_kw_entry(const BYTE *p, int *count, DWORD *kwoff)
{
    size_t len = strlen((const char *)p);
    short c;
    memcpy(&c, p + len + 1, sizeof(c));
    memcpy(kwoff, p + len + 3, sizeof(*kwoff));
    *count = c;
}

static void cb_tts_keyword(void *p, void **next, void *cookie)
{
    HWND list = cookie;
    LRESULT i = SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)p);
    if (i >= 0) SendMessageA(list, LB_SETITEMDATA, i, (LPARAM)p);
    *next = (char *)p + strlen((char *)p) + 7;
}

static void tts_set_default(HWND dlg, int id)
{
    /* DM_SETDEFID alone does not repaint the old default's thick border */
    int old = LOWORD(SendMessageA(dlg, DM_GETDEFID, 0, 0));
    if (old == id) return;
    SendMessageA(dlg, DM_SETDEFID, id, 0);
    if (old && GetDlgItem(dlg, old))
        SendDlgItemMessageA(dlg, old, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
    SendDlgItemMessageA(dlg, id, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
}

static void tts_show_topics(HWND dlg, struct tt_search *s)
{
    HWND keys = GetDlgItem(dlg, IDC_TTS_KEYS);
    HWND topics = GetDlgItem(dlg, IDC_TTS_TOPICS);
    LRESULT sel = SendMessageA(keys, LB_GETCURSEL, 0, 0);
    char word[256];
    const BYTE *p;
    int count, i;
    DWORD kwoff;

    if (sel == LB_ERR || !GetWindowTextLengthA(GetDlgItem(dlg, IDC_TTS_EDIT)))
    {
        MessageBeep(0);
        return;
    }
    p = (const BYTE *)SendMessageA(keys, LB_GETITEMDATA, sel, 0);
    tts_kw_entry(p, &count, &kwoff);
    /* the real dialog scrolls the chosen keyword to the top row (GOPHER
     * "Item": index 5 of 12 becomes the first visible row) */
    SendMessageA(keys, LB_SETTOPINDEX, sel, 0);

    /* The edit takes the keyword's own spelling (real: "telnet" -> "Telnet"). */
    lstrcpynA(word, (const char *)p, sizeof(word));
    s->quiet = TRUE;
    SetDlgItemTextA(dlg, IDC_TTS_EDIT, word);
    s->quiet = FALSE;

    SendMessageA(topics, WM_SETREDRAW, FALSE, 0);
    SendMessageA(topics, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < count; i++)
    {
        DWORD off;
        ULONG rel;
        HLPFILE_PAGE *page;
        const char *title;
        LRESULT n;

        /* |KWDATA is kept with its 9-byte internal file header (Wine's own
         * Index page reads it at +9 too); entries are 4-byte topic offsets. */
        memcpy(&off, s->hlpfile->kwdata + 9 + kwoff + 4 * i, sizeof(off));
        if (off == 0xFFFFFFFF) continue;              /* macro keyword */
        page = HLPFILE_PageByOffset(s->hlpfile, off, &rel);
        if (!page) continue;
        title = (page->lpszTitle && page->lpszTitle[0]) ? page->lpszTitle
                                                        : ">>Untitled Topic<<";
        n = SendMessageA(topics, LB_ADDSTRING, 0, (LPARAM)title);
        if (n >= 0) SendMessageA(topics, LB_SETITEMDATA, n, (LPARAM)off);
    }
    SendMessageA(topics, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(topics, NULL, TRUE);

    if (SendMessageA(topics, LB_GETCOUNT, 0, 0) > 0)
    {
        char close[32];
        SendMessageA(topics, LB_SETCURSEL, 0, 0);
        EnableWindow(GetDlgItem(dlg, IDC_TTS_GOTO), TRUE);
        LoadStringA(Globals.hInstance, STID_TT_CLOSE, close, sizeof(close));
        SetDlgItemTextA(dlg, IDCANCEL, close);
        s->shown = TRUE;
        SendMessageA(dlg, WM_NEXTDLGCTL, (WPARAM)topics, TRUE);
        tts_set_default(dlg, IDC_TTS_GOTO);
    }
    else
        MessageBeep(0);
}

static void tts_goto(HWND dlg, struct tt_search *s)
{
    HWND topics = GetDlgItem(dlg, IDC_TTS_TOPICS);
    LRESULT sel = SendMessageA(topics, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) { MessageBeep(0); return; }
    s->offset = (LONG)SendMessageA(topics, LB_GETITEMDATA, sel, 0);
    EndDialog(dlg, IDOK);
}

/* WinHelp's edit drives the keyword list with the cursor keys, as a combo
 * box would. */
static LRESULT CALLBACK tts_edit_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    HWND dlg = GetParent(hwnd);
    struct tt_search *s = (struct tt_search *)GetWindowLongPtrA(dlg, DWLP_USER);
    if (msg == WM_KEYDOWN && (wp == VK_UP || wp == VK_DOWN || wp == VK_PRIOR || wp == VK_NEXT))
    {
        SendDlgItemMessageA(dlg, IDC_TTS_KEYS, WM_KEYDOWN, wp, lp);
        return 0;
    }
    return CallWindowProcA(s->edit_proc, hwnd, msg, wp, lp);
}

static void tts_layout(HWND dlg, struct tt_search *s)
{
    /* TELETEXT PORT: + the 2 px of the 3.1 dialog band that w31 paints
     * inside the client (real frame: 1 black + 4 navy); controls move by 2 */
    RECT rc = { 0, 0, TT_CLIENT_W + 2 * TT_BAND_IN, TT_CLIENT_H + 2 * TT_BAND_IN }, wr, wa;
    unsigned i;
    HWND owner = GetWindow(dlg, GW_OWNER);

    AdjustWindowRectEx(&rc, GetWindowLongA(dlg, GWL_STYLE), FALSE,
                       GetWindowLongA(dlg, GWL_EXSTYLE));
    /* Real WinHelp opens Search near the top left of the help window
     * (VM: dialog at +34,+42 from the main window); keep it on the owner's
     * monitor (TELETEXT PORT: multi-monitor). */
    GetWindowRect(owner ? owner : dlg, &wr);
    TT_WorkAreaFromWindow(owner ? owner : dlg, &wa);
    {
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        int x = wr.left + 34, y = wr.top + 42;
        if (x + w > wa.right) x = wa.right - w;
        if (y + h > wa.bottom) y = wa.bottom - h;
        if (x < wa.left) x = wa.left;
        if (y < wa.top) y = wa.top;
        SetWindowPos(dlg, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    for (i = 0; i < sizeof(tt_layout) / sizeof(tt_layout[0]); i++)
    {
        HWND c = GetDlgItem(dlg, tt_layout[i].id);
        if (!c) continue;
        /* MODERN ACCOMMODATION undone: Windows gives dialog fields and lists
         * a 2px 3D client edge; Windows 3.1 drew a 1px black border. */
        if (tt_layout[i].id == IDC_TTS_EDIT || tt_layout[i].id == IDC_TTS_KEYS ||
            tt_layout[i].id == IDC_TTS_TOPICS)
        {
            SetWindowLongA(c, GWL_EXSTYLE, GetWindowLongA(c, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
            SetWindowLongA(c, GWL_STYLE, GetWindowLongA(c, GWL_STYLE) | WS_BORDER);
        }
        SetWindowPos(c, NULL, tt_layout[i].x + TT_BAND_IN, tt_layout[i].y + TT_BAND_IN, tt_layout[i].w,
                     tt_layout[i].h, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        SendMessageA(c, WM_SETFONT, (WPARAM)s->font, FALSE);
    }
    /* list item height = the font's height, as in Win 3.1 (13 px) */
    {
        HDC dc = GetDC(dlg);
        HFONT old = SelectObject(dc, s->font);
        TEXTMETRICA tm;
        GetTextMetricsA(dc, &tm);
        SelectObject(dc, old);
        ReleaseDC(dlg, dc);
        s->item_h = tm.tmHeight;
        SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_SETITEMHEIGHT, 0, tm.tmHeight);
        SendDlgItemMessageA(dlg, IDC_TTS_TOPICS, LB_SETITEMHEIGHT, 0, tm.tmHeight);
    }
}

static INT_PTR CALLBACK tts_dlg_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    struct tt_search *s = (struct tt_search *)GetWindowLongPtrA(dlg, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
        s = (struct tt_search *)lp;
        SetWindowLongPtrA(dlg, DWLP_USER, (LONG_PTR)s);
        s->font = CreateFontA(-MulDiv(8, 96, 72), 0, 0, 0, FW_BOLD, 0, 0, 0,
                              ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, "MS Sans Serif");
        tts_layout(dlg, s);
        HLPFILE_BPTreeEnum(s->hlpfile->kwbtree, cb_tts_keyword, GetDlgItem(dlg, IDC_TTS_KEYS));
        SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_SETCURSEL, 0, 0);
        EnableWindow(GetDlgItem(dlg, IDC_TTS_SHOW), FALSE);
        EnableWindow(GetDlgItem(dlg, IDC_TTS_GOTO), FALSE);
        s->edit_proc = (WNDPROC)SetWindowLongPtrA(GetDlgItem(dlg, IDC_TTS_EDIT),
                                                  GWLP_WNDPROC, (LONG_PTR)tts_edit_proc);
        /* MODERN ACCOMMODATION undone: Windows 2000+ hides mnemonic
         * underlines and focus frames until the keyboard is used; Windows
         * 3.1 always drew them ("Type a word", "Select a topic"). */
        /* posted: the dialog manager initialises the UI state after
         * WM_INITDIALOG and would undo a sent change */
        PostMessageA(dlg, WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEACCEL | UISF_HIDEFOCUS), 0);
        SetFocus(GetDlgItem(dlg, IDC_TTS_EDIT));
        return FALSE;   /* we set the focus */

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:   /* Enter: the current default button */
            if (GetFocus() == GetDlgItem(dlg, IDC_TTS_TOPICS) && s->shown)
                tts_goto(dlg, s);
            else if (IsWindowEnabled(GetDlgItem(dlg, IDC_TTS_SHOW)))
                tts_show_topics(dlg, s);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_TTS_SHOW:
            tts_show_topics(dlg, s);
            return TRUE;
        case IDC_TTS_GOTO:
            tts_goto(dlg, s);
            return TRUE;
        case IDC_TTS_EDIT:
            if (HIWORD(wp) == EN_CHANGE && !s->quiet)
            {
                char text[256];
                GetDlgItemTextA(dlg, IDC_TTS_EDIT, text, sizeof(text));
                if (text[0])
                {
                    LRESULT i = SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_FINDSTRING,
                                                    (WPARAM)-1, (LPARAM)text);
                    if (i != LB_ERR)
                        SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_SETCURSEL, i, 0);
                }
                EnableWindow(GetDlgItem(dlg, IDC_TTS_SHOW), text[0] != 0);
                tts_set_default(dlg, IDC_TTS_SHOW);
            }
            else if (HIWORD(wp) == EN_SETFOCUS)
                tts_set_default(dlg, IDC_TTS_SHOW);
            return TRUE;
        case IDC_TTS_KEYS:
            if (HIWORD(wp) == LBN_SELCHANGE)
            {
                char text[256];
                LRESULT i = SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_GETCURSEL, 0, 0);
                if (i != LB_ERR &&
                    SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_GETTEXTLEN, i, 0) < (LRESULT)sizeof(text))
                {
                    SendDlgItemMessageA(dlg, IDC_TTS_KEYS, LB_GETTEXT, i, (LPARAM)text);
                    s->quiet = TRUE;
                    SetDlgItemTextA(dlg, IDC_TTS_EDIT, text);
                    s->quiet = FALSE;
                    EnableWindow(GetDlgItem(dlg, IDC_TTS_SHOW), TRUE);
                    tts_set_default(dlg, IDC_TTS_SHOW);
                }
            }
            else if (HIWORD(wp) == LBN_DBLCLK)
                tts_show_topics(dlg, s);
            else if (HIWORD(wp) == LBN_SETFOCUS)
                tts_set_default(dlg, IDC_TTS_SHOW);
            return TRUE;
        case IDC_TTS_TOPICS:
            if (HIWORD(wp) == LBN_DBLCLK)
                tts_goto(dlg, s);
            else if (HIWORD(wp) == LBN_SETFOCUS && s->shown)
                tts_set_default(dlg, IDC_TTS_GOTO);
            return TRUE;
        }
        break;

    /* Windows 3.1 colours: the Search dialog is white (COLOR_WINDOW), the
     * selection navy (VGA COLOR_HIGHLIGHT), the rule solid black. */
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetTextColor((HDC)wp, RGB(0, 0, 0));
        SetBkColor((HDC)wp, RGB(255, 255, 255));
        return (INT_PTR)GetStockObject(WHITE_BRUSH);

    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)lp)->itemHeight = s ? s->item_h : 13;
        return TRUE;

    case WM_DRAWITEM:
    {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)lp;
        if (di->CtlID == IDC_TTS_RULE)
        {
            FillRect(di->hDC, &di->rcItem, GetStockObject(BLACK_BRUSH));
            return TRUE;
        }
        if (di->CtlType == ODT_LISTBOX)
        {
            char text[512];
            BOOL sel = (di->itemState & ODS_SELECTED) != 0;
            RECT r = di->rcItem;
            HBRUSH br;
            if (di->itemID == (UINT)-1)
            {
                if (di->itemState & ODS_FOCUS) DrawFocusRect(di->hDC, &r);
                return TRUE;
            }
            br = CreateSolidBrush(sel ? RGB(0, 0, 128) : RGB(255, 255, 255));
            FillRect(di->hDC, &r, br);
            DeleteObject(br);
            text[0] = 0;
            if (SendMessageA(di->hwndItem, LB_GETTEXTLEN, di->itemID, 0) < (LRESULT)sizeof(text))
                SendMessageA(di->hwndItem, LB_GETTEXT, di->itemID, (LPARAM)text);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, sel ? RGB(255, 255, 255) : RGB(0, 0, 0));
            r.left += 2;
            DrawTextA(di->hDC, text, -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            r.left -= 2;
            /* Win 3.1 shows the dotted focus frame only after keyboard use
             * in the list (s4 vs s3 in the VM); ODS_NOFOCUSRECT is the same rule. */
            if ((di->itemState & ODS_FOCUS) && !(di->itemState & ODS_NOFOCUSRECT))
                DrawFocusRect(di->hDC, &r);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
        if (s && s->font) { DeleteObject(s->font); s->font = NULL; }
        break;
    }
    return FALSE;
}

/* Runs the modal Search dialog for the help file; returns TRUE and the topic
 * offset when the user chose Go To. */
BOOL TT_SearchDialog(HWND owner, HLPFILE *hlpfile, LONG *offset)
{
    struct tt_search s;

    memset(&s, 0, sizeof(s));
    s.hlpfile = hlpfile;
    if (DialogBoxParamA(Globals.hInstance, MAKEINTRESOURCEA(IDD_TT_SEARCH), owner,
                        tts_dlg_proc, (LPARAM)&s) != IDOK)
        return FALSE;
    *offset = s.offset;
    return TRUE;
}
