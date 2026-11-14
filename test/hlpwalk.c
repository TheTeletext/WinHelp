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
#include <ole2.h>
#include <richole.h>
#include "hlpfile.h"
#include "ttport.h"

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

/* Phase 2a: the keyword walk also does what the Search dialog does
 * (ttsearch.c): every topic offset of every keyword through |KWDATA to a
 * page. HLPWALK_KW=1 prints the keywords that have several topics. */
struct kwstat { HLPFILE *hf; unsigned n, multi, refs, unres, macro; };

static void kw_cb(void *p, void **next, void *cookie)
{
    /* keyword B+ tree leaf entry: string, WORD count, DWORD offset */
    char *s = (char *)p;
    struct kwstat *k = cookie;
    short count;
    DWORD kwoff, off;
    int i;
    ULONG rel;
    memcpy(&count, s + strlen(s) + 1, 2);
    memcpy(&kwoff, s + strlen(s) + 3, 4);
    k->n++;
    if (count > 1)
    {
        k->multi++;
        if (getenv("HLPWALK_KW")) fprintf(stderr, "KW %d [%s]\n", count, s);
    }
    for (i = 0; i < count; i++)
    {
        memcpy(&off, k->hf->kwdata + 9 + kwoff + 4 * i, 4);
        k->refs++;
        if (off == 0xFFFFFFFF) { k->macro++; continue; }
        if (!HLPFILE_PageByOffset(k->hf, off, &rel)) k->unres++;
        else if (getenv("HLPWALK_KW") && count > 1)
            fprintf(stderr, "     -> %s\n", HLPFILE_PageByOffset(k->hf, off, &rel)->lpszTitle);
    }
    *next = s + strlen(s) + 1 + 2 + 4;
}

static int region_len(HWND re, HLPFILE_PAGE *page, int region)
{
    struct RtfData rd;
    struct sink s;
    EDITSTREAM es;
    memset(&rd, 0, sizeof(rd));
    if (!HLPFILE_BrowsePageRegion(page, &rd, 1, 0, region)) { free(rd.data); return -1; }
    s.p = rd.data; s.end = rd.ptr;
    es.dwCookie = (DWORD_PTR)&s; es.dwError = 0; es.pfnCallback = stream_cb;
    SendMessageA(re, WM_SETTEXT, 0, (LPARAM)"");
    SendMessageA(re, EM_STREAMIN, SF_RTF, (LPARAM)&es);
    free(rd.data);
    return GetWindowTextLengthA(re);
}

int main(int argc, char **argv)
{
    HLPFILE *hf;
    HLPFILE_PAGE *page;
    HWND re;
    unsigned pages = 0, browse_fail = 0, empty = 0, links = 0, popups = 0, macros = 0,
             picts = 0, keywords = 0, objects = 0, nsr = 0, nsr_bad = 0, nsr_links = 0;
    struct RtfData rd2;
    unsigned long chars = 0, rtf_bytes = 0;
    DWORD t0 = GetTickCount();

    if (argc < 2) { fprintf(stderr, "usage: hlpwalk FILE.HLP\n"); return 1; }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    OleInitialize(NULL);
    if (!LoadLibraryA("riched20.dll")) { printf("WALK %s ERROR no-riched20\n", argv[1]); return 3; }
    re = CreateWindowExA(0, LoadLibraryA("msftedit.dll") ? "RICHEDIT50W" : "RichEdit20A", NULL,  /* as the viewer */ ES_MULTILINE | ES_READONLY | WS_POPUP,
                         0, 0, 600, 400, NULL, NULL, GetModuleHandleA(NULL), NULL);

    if (re) TT_RichEditEnablePictures(re);   /* exactly as the viewer */
    hf = HLPFILE_ReadHlpFile(argv[1]);
    if (!hf) { printf("WALK %s LOADFAIL\n", argv[1]); return 2; }

    struct kwstat ks = { hf, 0, 0, 0, 0, 0 };
    if (hf->kwbtree) HLPFILE_BPTreeEnum(hf->kwbtree, kw_cb, &ks);
    keywords = ks.n;

    for (page = hf->first_page; page; page = page->next)
    {
        struct RtfData rd;
        HLPFILE_LINK *l;
        pages++;
        memset(&rd, 0, sizeof(rd));
        if (!HLPFILE_BrowsePage(page, &rd, 1, 0)) { browse_fail++; free(rd.data); continue; }
        if (argc > 2 && atoi(argv[2]) == (int)pages)   /* hlpwalk FILE N: dump topic N's RTF to stderr */
            fwrite(rd.data, 1, rd.ptr - rd.data, stderr);
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
            {
                IRichEditOle *ole = NULL;
                SendMessageA(re, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
                if (ole) { objects += ole->lpVtbl->GetObjectCount(ole); ole->lpVtbl->Release(ole); }
            }
            if (n <= 0) empty++;
            chars += n > 0 ? n : 0;
            /* Phase 2a gap 2: a topic with a non-scrolling region is shown
             * as two streams; their text must add up to the whole topic
             * (+-1 for RichEdit's final paragraph mark). */
            if (page->tt_has_nsr)
            {
                int a = region_len(re, page, TT_REGION_NSR), b;
                HLPFILE_LINK *nl;
                for (nl = page->tt_first_link_nsr; nl; nl = nl->next)
                {
                    nsr_links++;
                    if (getenv("HLPWALK_NSR")) fprintf(stderr, "NSRLINK [%s] cookie=%d\n", page->lpszTitle, nl->cookie);
                }
                b = region_len(re, page, TT_REGION_SCROLL);
                nsr++;
                /* the region's last paragraph mark ends the NSR stream, so
                 * it is not counted there: whole = nsr + scroll + 2 (CR LF
                 * in GetWindowTextLength), or + 0..1 at a stream end */
                if (a <= 0 || b < 0 || n - (a + b) < 0 || n - (a + b) > 2) nsr_bad++;
                if (getenv("HLPWALK_NSR"))
                    fprintf(stderr, "NSR [%s] whole=%d nsr=%d scroll=%d scroll_ref=%08lx ref=%08lx\n",
                            page->lpszTitle, n, a, b, page->tt_scroll_ref, page->reference);
                /* restore the whole-topic link list for the counts above */
                memset(&rd2, 0, sizeof(rd2));
                if (HLPFILE_BrowsePage(page, &rd2, 1, 0)) free(rd2.data);
            }
        }
        free(rd.data);
    }
    printf("WALK %s ver=%u topics=%u browse_fail=%u empty=%u jumps=%u popups=%u macrolinks=%u "
           "picts=%u objects=%u bmps=%u keywords=%u kwmulti=%u kwrefs=%u kwunres=%u kwmacro=%u nsr=%u nsr_bad=%u nsrlinks=%u windows=%u chars=%lu rtf=%lu ms=%lu\n",
           argv[1], hf->version, pages, browse_fail, empty, links, popups, macros, picts, objects,
           hf->numBmps, keywords, ks.multi, ks.refs, ks.unres, ks.macro, nsr, nsr_bad, nsr_links, hf->numWindows, chars, rtf_bytes,
           (unsigned long)(GetTickCount() - t0));
    if (nsr_bad) return 5;      /* the two regions do not add up to the topic */
    return ks.unres ? 4 : 0;   /* a keyword topic the Search dialog could not open */
}
