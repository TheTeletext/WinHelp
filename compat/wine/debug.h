/*
 * Minimal stand-in for Wine's <wine/debug.h> so the Wine winhlp32 sources
 * build as a native Windows program with MSYS2 UCRT64 GCC.
 *
 * MODERN ACCOMMODATION (port, not upstream): Wine's real debug channels are
 * driven by WINEDEBUG and libwine. Here every channel goes to one log file,
 * only when the environment variable TTWINHLP_LOG names it, so a release run
 * writes nothing. The fidelity audit uses this log: each FIXME is an
 * upstream-acknowledged unimplemented feature hit by a real .hlp file.
 *
 * Copyright 2026 The Teletext. Same licence as the code that includes it:
 * GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#ifndef TTWINHLP_COMPAT_WINE_DEBUG_H
#define TTWINHLP_COMPAT_WINE_DEBUG_H

#include <stdarg.h>
#include <windef.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* level: 0 ERR, 1 FIXME, 2 WARN, 3 TRACE */
void tt_dbg_printf(int level, const char *func, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
const char *tt_dbgstr_an(const char *s, int n);
int tt_dbg_level(void);

#define WINE_DEFAULT_DEBUG_CHANNEL(ch) static const char *const tt_dbch_unused_##ch __attribute__((unused)) = #ch

#define WINE_ERR(...)   tt_dbg_printf(0, __func__, __VA_ARGS__)
#define WINE_FIXME(...) do { if (tt_dbg_level() >= 1) tt_dbg_printf(1, __func__, __VA_ARGS__); } while (0)
#define WINE_WARN(...)  do { if (tt_dbg_level() >= 2) tt_dbg_printf(2, __func__, __VA_ARGS__); } while (0)
#define WINE_TRACE(...) do { if (tt_dbg_level() >= 3) tt_dbg_printf(3, __func__, __VA_ARGS__); } while (0)

#define debugstr_a(s)     tt_dbgstr_an((s), -1)
#define debugstr_an(s, n) tt_dbgstr_an((s), (n))

#endif
