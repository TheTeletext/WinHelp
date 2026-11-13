/*
 * Force-included (gcc -include) into every source of the port.
 * Names Wine's own headers define that the mingw-w64 SDK headers do not.
 * Each one is the value Windows itself uses; no behaviour changes here.
 *
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#ifndef TTWINHLP_COMPAT_H
#define TTWINHLP_COMPAT_H

#include <windows.h>
#include <richedit.h>

/* wingdi.h in Wine: EASTEUROPE_CHARSET under its Win 3.1 name. */
#ifndef EE_CHARSET
#define EE_CHARSET EASTEUROPE_CHARSET   /* 238 */
#endif

/* richedit.h in Wine; mingw-w64 only has the TEXT() form RICHEDIT_CLASS. */
#ifndef RICHEDIT_CLASS20A
#define RICHEDIT_CLASS20A "RichEdit20A"
#endif

#endif
