/*
 * Implementation of the <wine/debug.h> stand-in (see compat/wine/debug.h).
 *
 * MODERN ACCOMMODATION (port, not upstream). Environment:
 *   TTWINHLP_LOG=<path>        append channel output to <path>
 *   TTWINHLP_LOGLEVEL=0..3     0 ERR only, 1 +FIXME (default), 2 +WARN, 3 +TRACE
 * Without TTWINHLP_LOG nothing is written anywhere.
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "wine/debug.h"

static FILE *tt_log;
static int tt_level = -1;

static void tt_dbg_init(void)
{
    const char *p, *l;
    if (tt_level >= 0) return;
    tt_level = 1;
    l = getenv("TTWINHLP_LOGLEVEL");
    if (l && *l) tt_level = atoi(l);
    p = getenv("TTWINHLP_LOG");
    if (p && *p) tt_log = fopen(p, "a");
    if (!tt_log) tt_level = -1000; /* nothing to write to: stay silent */
}

int tt_dbg_level(void)
{
    tt_dbg_init();
    return tt_level;
}

void tt_dbg_printf(int level, const char *func, const char *fmt, ...)
{
    static const char *const names[] = { "err", "fixme", "warn", "trace" };
    va_list ap;
    tt_dbg_init();
    if (!tt_log || level > tt_level) return;
    fprintf(tt_log, "%s:winhelp:%s ", names[level & 3], func);
    va_start(ap, fmt);
    vfprintf(tt_log, fmt, ap);
    va_end(ap);
    fflush(tt_log);
}

/* Quoted, escaped copy of a narrow string, from a small ring of buffers
 * (Wine's debugstr_a has the same "valid until a few calls later" contract). */
const char *tt_dbgstr_an(const char *s, int n)
{
    static char ring[8][256];
    static int slot;
    char *out = ring[slot++ & 7], *o = out;
    char *end = out + sizeof(ring[0]) - 6;
    if (!s) return "(null)";
    if (!HIWORD((ULONG_PTR)s)) { snprintf(out, 256, "#%04x", LOWORD((ULONG_PTR)s)); return out; }
    if (n < 0) n = (int)strlen(s);
    *o++ = '"';
    while (n-- > 0 && o < end)
    {
        unsigned char c = (unsigned char)*s++;
        if (c == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; }
        else if (c == '"' || c == '\\') { *o++ = '\\'; *o++ = c; }
        else if (c < 0x20 || c >= 0x7f) { o += sprintf(o, "\\x%02x", c); }
        else *o++ = c;
    }
    if (n >= 0) { *o++ = '.'; *o++ = '.'; *o++ = '.'; }
    *o++ = '"'; *o = 0;
    return out;
}
