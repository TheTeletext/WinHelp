/*
 * hlpwalk - headless gate for the revived WinHelp viewer (test only, not shipped).
 *
 * Links the viewer's own parser (src/hlpfile.c) and, for one .HLP file:
 *   - loads it (HLPFILE_ReadHlpFile),
 *   - renders EVERY topic to RTF (HLPFILE_BrowsePage), exactly as the viewer
 *     does when a topic is shown,
 *   - streams each topic's RTF into a hidden RichEdit 2.0 control (the
 *     viewer's display path; the window is never shown, so no monitor rule
 *     applies),
 *   - counts topics, links by kind, pictures, keywords, plain characters.
 * Output: one line "WALK <file> key=value ..." on stdout. Exit 0 = walked;
 * 2 = load failed; a crash shows as the process exit code (run one file per
 * process).
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <richedit.h>
#include "hlpfile.h"

struct sink { const char *p; const char *end; };

static DWORD CALLBACK stream_cb(DWORD_PTR cookie, BYTE *buf, LONG cb, LONG *pcb)
{
    struct sink *s = (struct sink *)cookie;
    LONG n = (LONG)(s->end - s->p);
    if (n > cb) n = cb;
    memcpy(buf, s->p, n);
    s->p += n;
    *pcb = n;
    return 0;
}

static unsigned count_sub(const char *hay, size_t len, const char *needle)
{
    unsigned n = 0;
    size_t nl = strlen(needle);
    const char *p = hay, *end = hay + len;
    while (p + nl <= end && (p = memchr(p, needle[0], end - p)) && p + nl <= end)
    {
        if (!memcmp(p, needle, nl)) n++;
        p++;
    }
    return n;
}

static void kw_cb(void *p, void **next, void *cookie)
{
    /* keyword B+ tree leaf entry: string, WORD count, DWORD offset */
    char *s = (char *)p;
    (*(unsigned *)cookie)++;
    *next = s + strlen(s) + 1 + 2 + 4;
}

int main(int argc, char **argv)
{
    HLPFILE *hf;
    HLPFILE_PAGE *page;
    HWND re;
    unsigned pages = 0, browse_fail = 0, empty = 0, links = 0, popups = 0, macros = 0,
             picts = 0, keywords = 0;
    unsigned long chars = 0, rtf_bytes = 0;
    DWORD t0 = GetTickCount();

    if (argc < 2) { fprintf(stderr, "usage: hlpwalk FILE.HLP\n"); return 1; }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (!LoadLibraryA("riched20.dll")) { printf("WALK %s ERROR no-riched20\n", argv[1]); return 3; }
    re = CreateWindowExA(0, "RichEdit20A", NULL, ES_MULTILINE | ES_READONLY | WS_POPUP,
                         0, 0, 600, 400, NULL, NULL, GetModuleHandleA(NULL), NULL);

    hf = HLPFILE_ReadHlpFile(argv[1]);
    if (!hf) { printf("WALK %s LOADFAIL\n", argv[1]); return 2; }

    if (hf->kwbtree) HLPFILE_BPTreeEnum(hf->kwbtree, kw_cb, &keywords);

    for (page = hf->first_page; page; page = page->next)
    {
        struct RtfData rd;
        HLPFILE_LINK *l;
        pages++;
        memset(&rd, 0, sizeof(rd));
        if (!HLPFILE_BrowsePage(page, &rd, 1, 0)) { browse_fail++; free(rd.data); continue; }
        rtf_bytes += rd.ptr - rd.data;
        picts += count_sub(rd.data, rd.ptr - rd.data, "\\pict");
        for (l = page->first_link; l; l = l->next)
        {
            if (l->cookie == hlp_link_popup) popups++;
            else if (l->cookie == hlp_link_macro) macros++;
            else links++;
        }
        if (re)
        {
            struct sink s = { rd.data, rd.ptr };
            EDITSTREAM es = { (DWORD_PTR)&s, 0, stream_cb };
            int n;
            SendMessageA(re, WM_SETTEXT, 0, (LPARAM)"");
            SendMessageA(re, EM_STREAMIN, SF_RTF, (LPARAM)&es);
            n = GetWindowTextLengthA(re);
            if (n <= 0) empty++;
            chars += n > 0 ? n : 0;
        }
        free(rd.data);
    }
    printf("WALK %s ver=%u topics=%u browse_fail=%u empty=%u jumps=%u popups=%u macrolinks=%u "
           "picts=%u bmps=%u keywords=%u windows=%u chars=%lu rtf=%lu ms=%lu\n",
           argv[1], hf->version, pages, browse_fail, empty, links, popups, macros, picts,
           hf->numBmps, keywords, hf->numWindows, chars, rtf_bytes,
           (unsigned long)(GetTickCount() - t0));
    return 0;
}
