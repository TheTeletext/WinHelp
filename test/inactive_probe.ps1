# inactive_probe.ps1 - phase 2b: our Search dialog, active and then
# DEACTIVATED (another window activated by a real click), captured from the
# screen, for the pixel compare with the real one (real_dlg_inactive.png).
param([string]$Out = "D:\winhelp_corpus\p2b", [int]$OtherX = -1600, [int]$OtherY = 20)
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\inactive.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\inactive.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+s}{WAIT:900}"
$dlg = Get-TtwDialog $run "Search" 3000
if ($dlg -eq [IntPtr]::Zero) { "NO DIALOG"; Stop-Ttw $run | Out-Null; exit 1 }
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
Start-Sleep -Milliseconds 500
Save-TtwScreenShot $dlg "$Out\ours_dlg_active.png"
Invoke-TtwClick $OtherX $OtherY          # another window's caption: the dialog loses activation
Start-Sleep -Milliseconds 800
Save-TtwScreenShot $dlg "$Out\ours_dlg_inactive.png"
"foreground is the dialog: " + ([TtIn]::GetForegroundWindow() -eq $dlg)
Stop-Ttw $run
