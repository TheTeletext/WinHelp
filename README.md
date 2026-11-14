# The Teletext WinHelp (TTWINHLP.EXE)

A native Windows 11 build of the open-source WinHelp viewer from the Wine
project (`programs/winhlp32`), reading authentic WinHelp 3.0 and 3.1 `.HLP`
files. Microsoft removed WinHelp from Windows 10 and 11; this brings it
back as a standalone program, the way WinHelp always was: applications
launch it for their Help menu.

## Licence

GNU Lesser General Public License, version 2.1 or later (`COPYING.LIB`).
Every source file keeps its original copyright and licence header. The
original authors are the Wine contributors (Ulrich Schmid, Eric Pouech,
Sylvain Petreolle and others; see the file headers). Changes made for this
port are marked `TELETEXT PORT` in the source; the files that are new in
this port (`src/ttport.c`, `src/ttport.h`, `compat/*`, `test/*`, `tools/*`)
are under the same licence.

`upstream/` holds the unmodified Wine and ReactOS sources this fork started
from (`upstream/UPSTREAM.txt` names the commits), so every change is a
reviewable diff.

## Building

MSYS2 UCRT64 (GCC 16.1.0) and `flex` from MSYS2:

    build_x64.bat

produces `build\TTWINHLP.EXE` and the test-only `build\hlpwalk.exe`.

## Running

    TTWINHLP.EXE [-i context] file.hlp[>window]

Window positions are kept, as WinHelp 3.1 kept them in WIN.INI
(`[Windows Help] M_WindowPosition=[x,y,w,h,max]`), in
`%APPDATA%\The Teletext\TTWINHLP.INI`.

Diagnostics: `TTWINHLP_LOG=<file>` writes the upstream FIXME/ERR channels
(`TTWINHLP_LOGLEVEL` 0-3); `TTWINHLP_INI=<file>` uses another INI;
`TTWINHLP_RICHED20=1` uses the RichEdit 2.0 class instead of msftedit.
Phase 2a switches, each restoring Wine's or Windows' behaviour for
comparison: `TTWINHLP_W11=1` (Windows 11 look instead of the Win 3.1
chrome), `TTWINHLP_WINE_INDEX=1` (Wine's Index sheet instead of the 3.1
Search dialog), `TTWINHLP_NO_NSR=1` (no non-scrolling band),
`TTWINHLP_GREEN=1` (green hotspots), `TTWINHLP_NOSHADOW=1` (no popup
shadow), `TTWINHLP_TTFONTS=1` (TrueType instead of the Helv/Tms Rmn bitmap
faces), `TTWINHLP_SECBAR=1` (button bar in secondary windows).

The Windows 3.1 chrome (`src/w31.c`, `src/w31.h`) comes from Teletext
Navigator's `suite_w31.c`, relicensed for this copy under the LGPL 2.1+ by
its author, Dimitri Papadopoulos (2026-11-13); see the file headers.

## Tests

- `test/gate_corpus.ps1`: every topic of every file in a corpus through the
  parser and RichEdit (headless), then the viewer per file on the secondary
  monitor. The corpus itself (period help files) is not distributed here.
- `test/gate_roundtrip.ps1`: RTF + HPJ -> Microsoft Help Compiler 3.10.505
  (under DOSBox; not included) -> `.HLP` -> this viewer.
