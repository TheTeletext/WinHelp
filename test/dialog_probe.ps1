# dialog_probe.ps1 - open Search (Alt+S) and About (Help > About) in our viewer
# with real keys and capture them from the SCREEN, active and inactive, for a
# pixel compare with the real WinHelp's (frames\f_search_active.png etc.).
param([string]$Out = "D:\winhelp_corpus\p2a", [string]$File = "D:\winhelp_build\TTTEST.HLP")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\dlg.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$File`"" -Ini $ini -Log "$Out\dlg.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
$wr = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null
[TtIn]::SetCursorPos($wr.L + 600, $wr.T + 500) | Out-Null
Send-TtwKeys "{ALT+s}{WAIT:900}"
$d = Get-TtwDialog $run "Search" 2000
if ($d -ne [IntPtr]::Zero) {
    if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
    $r = New-Object TtW+RECT; [TtW]::GetWindowRect($d, [ref]$r) | Out-Null
    "search at +{0},+{1} {2}x{3}" -f ($r.L - $wr.L), ($r.T - $wr.T), ($r.R - $r.L), ($r.B - $r.T)
    Save-TtwScreenShot $d "$Out\dlg_search_active.png"
    # inactive: another window of ours to the front (the 86Box VM is the
    # real one's Task List stand-in: any other active window will do)
    $vm = [Box]::FindEmu 2>$null
    Send-TtwKeys "{ESC}{WAIT:400}"
}
Send-TtwKeys "{ALT+h}{WAIT:300}a{WAIT:900}"
$a = Get-TtwDialog $run "About Help" 2000
if ($a -ne [IntPtr]::Zero) {
    $r = New-Object TtW+RECT; [TtW]::GetWindowRect($a, [ref]$r) | Out-Null
    "about at +{0},+{1} {2}x{3}" -f ($r.L - $wr.L), ($r.T - $wr.T), ($r.R - $r.L), ($r.B - $r.T)
    Save-TtwScreenShot $a "$Out\dlg_about_active.png"
    Send-TtwKeys "{ENTER}{WAIT:400}"
}
"monitor guard: " + ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run
