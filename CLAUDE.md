# The Teletext WinHelp (TTWINHLP.EXE): working notes

Thin anchor. The machine anchor of record is `C:\Users\pa1067\CLAUDE.md`.
Reports live with the consumer, Navigator, in `..\mgt-unicorn-suite\docs\`.

## INDEX

- [WinHelp Revival phase 1 2026-11-13](..\mgt-unicorn-suite\docs\2026-11-13_WinHelp_Revival_Phase1.md) -
  port, corpus gate 110/110, fidelity audit vs the real WINHELP.EXE, HC
  round trip, LGPL obligations (s8), ranked gaps and phase 2 plan (s9).
- [WinHelp Revival phase 2a 2026-11-13](..\mgt-unicorn-suite\docs\2026-11-13_WinHelp_Revival_Phase2a.md) -
  viewer gaps closed (3.1 Search, \keepn band, 3.1 bar/menus/About, Win 3.1
  chrome from relicensed suite_w31 = src/w31.c, popups, fonts, History,
  frames, 19 px caption with w31 owning the menu bar, pixel-diff residual
  sweep s14); gates gate_search/nsr/bar/chrome/popup/menu + corpus 110/110;
  open items and where phase 2b resumes: s13.

## Rules for this repo

- **Fidelity fixes are autonomous** (operator, 2026-11-13, standing): a deviation
  from the real WinHelp 3.1 / Win 3.1 reference with a technically reasonable fix
  is fixed and reported done, without asking. Ask only for editorial,
  licence-related or hard-to-reverse decisions.
- LGPL-2.1+ (COPYING.LIB). Keep every Wine header; mark every change
  `TELETEXT PORT` with the reason. `upstream/` is never edited.
- Build: `build_x64.bat` (UCRT64 prepended; needs MSYS2 `flex`).
- Test windows go on monitor 2: the harness writes a test INI
  (`TTWINHLP_INI`) that places the viewer there BEFORE it is shown, offset
  860 px so it does not overlap the 86Box VM window; every run is guarded
  by `Test-TtwOnMonitor2`.
- The corpus (`D:\winhelp_corpus\wfw311_tree`) and HC505 (`D:\winhelp_dl`)
  are copyrighted third-party files: never commit them.
- Gates: `test\gate_corpus.ps1` (corpus), `test\gate_roundtrip.ps1` (HC),
  and the real-input viewer gates `gate_search`, `gate_nsr`, `gate_bar`,
  `gate_chrome`, `gate_popup`, `gate_menu`. Never build while a gate runs: a failed
  link deletes the exe.
- Before driving the viewer with the mouse, release 86Box's mouse grab
  (`vm_mouse.ps1 release`), or the clicks go into the VM.
