# The Teletext WinHelp (TTWINHLP.EXE): working notes

Thin anchor. The machine anchor of record is `C:\Users\pa1067\CLAUDE.md`.
Reports live with the consumer, Navigator, in `..\mgt-unicorn-suite\docs\`.

## INDEX

- [WinHelp Revival phase 1 2026-11-13](..\mgt-unicorn-suite\docs\2026-11-13_WinHelp_Revival_Phase1.md) -
  port, corpus gate 110/110, fidelity audit vs the real WINHELP.EXE, HC
  round trip, LGPL obligations (s8), ranked gaps and phase 2 plan (s9).

## Rules for this repo

- LGPL-2.1+ (COPYING.LIB). Keep every Wine header; mark every change
  `TELETEXT PORT` with the reason. `upstream/` is never edited.
- Build: `build_x64.bat` (UCRT64 prepended; needs MSYS2 `flex`).
- Test windows go on monitor 2: the harness writes a test INI
  (`TTWINHLP_INI`) that places the viewer there BEFORE it is shown, offset
  860 px so it does not overlap the 86Box VM window; every run is guarded
  by `Test-TtwOnMonitor2`.
- The corpus (`D:\winhelp_corpus\wfw311_tree`) and HC505 (`D:\winhelp_dl`)
  are copyrighted third-party files: never commit them.
- Gates: `test\gate_corpus.ps1` (corpus), `test\gate_roundtrip.ps1` (HC).
