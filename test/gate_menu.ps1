# gate_menu.ps1 - the w31-owned menu bar (19 px caption, 2026-11-13) and the
# History window, REAL input on monitor 2. Expected values: the real
# WINHELP.EXE 3.10 in the WfW 3.11 VM (real_menu_f/h.png, real_history.png:
# drop-downs from the item's left edge on the menu rule, row 41; History
# 266x200, caption and body pixel-identical).
param([string]$Out = "D:\winhelp_corpus\p2a")
$pass = 0; $fail = 0
function Check($name, $cond, $detail = "") { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name  $detail" } }

"Menu bar (menu_probe.ps1):"
$m = (& "$PSScriptRoot\menu_probe.ps1" -Out $Out -Name gate_menu) -join "`n"
Check "click on File opens File at +4,+41 (115x92)"     ($m -match 'file_click: menus \[\+4,\+41 115x92\]')
Check "Esc closes it"                                   ($m -match 'after_esc: menus \[\]')
Check "Alt+H opens Help at +166,+41 (140x67)"          ($m -match 'alt_h: menus \[\+166,\+41 140x67\]')
Check "Right from Help wraps to File"                   ($m -match 'alt_h_right: menus \[\+4,\+41 115x92\]')
Check "F10 enters the bar without opening"              ($m -match 'f10: menus \[\]')
Check "F10, Right, Down opens Edit at +44,+41"          ($m -match 'f10_down: menus \[\+44,\+41 100x49\]')
Check "Help > About Help by mouse"                      ($m -match 'About via the menu: True')
Check "monitor 2 guard (menus)"                         ($m -match 'monitor guard: True')

"History (history_probe.ps1):"
$h = (& "$PSScriptRoot\history_probe.ps1" -Out $Out) -join "`n"
Check "Alt+T opens it, default 266x200 (real)"          ($h -match 'history rect -?\d+,-?\d+ 266x200')
if (Test-Path "$Out\o_history.png") {
    magick "$Out\real_history.png" -crop 266x200+266+247 +repage "$Out\gm_rh.png"
    $cap  = ((& magick compare -metric AE ( "$Out\gm_rh.png" + "[266x23+0+0]") ("$Out\o_history.png" + "[266x23+0+0]") null: 2>&1) -join '') -replace ' .*',''
    $body = ((& magick compare -metric AE ( "$Out\gm_rh.png" + "[266x150+0+50]") ("$Out\o_history.png" + "[266x150+0+50]") null: 2>&1) -join '') -replace ' .*',''
    Check "caption pixel-identical to the real (no min/max)" ($cap -eq "0") "diff $cap"
    Check "body + frame pixel-identical to the real"         ($body -eq "0") "diff $body"
}
Check "monitor 2 guard (history)"                       ($h -notmatch 'MONITOR FAIL')
"GATE MENU: $pass PASS, $fail FAIL"
