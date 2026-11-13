/*
 * The Teletext WinHelp: port-specific helpers (see ttport.c).
 * Copyright 2026 The Teletext. GNU LGPL 2.1 or later (see COPYING.LIB).
 */
#ifndef TTWINHLP_SRC_TTPORT_H
#define TTWINHLP_SRC_TTPORT_H
#include <windows.h>

const char *TT_IniPath(void);
BOOL TT_LoadWindowPos(const char *key, RECT *rc, BOOL *maximized);
void TT_SaveWindowPos(const char *key, HWND hwnd);
void TT_WorkAreaFromWindow(HWND hwnd, RECT *wa);
void TT_InstallDialogPlacement(void);
void TT_RichEditEnablePictures(HWND re);

/* ttsearch.c: the WinHelp 3.1 Search dialog (phase 2a) */
struct tagHlpFileFile;
BOOL TT_SearchDialog(HWND owner, struct tagHlpFileFile *hlpfile, LONG *offset);

#endif
