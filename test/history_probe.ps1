# history_probe.ps1 - open TTTEST, go Settings -> Contents (via Contents
# button), open History with Alt+T, capture it on screen (monitor 2), for a
# pixel compare with real_history.png (real at 266,247, 266x200).
param([string]$Out = "D:\winhelp_corpus\p2a")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\hist.ini" -Width 679 -Height 596
# no stored H_WindowPosition: exercise the default placement (main + 157,200, 266x200)
(Get-Content $ini) | Where-Object { $_ -notmatch "^H_WindowPosition" } | Set-Content -Encoding ascii $ini
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\hist.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+t}{WAIT:900}"
$hist = [TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Text($_) -match "History" } | Select-Object -First 1
if (-not $hist) { "NO HISTORY"; Stop-Ttw $run | Out-Null; exit 1 }
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (history)"; exit 2 }
$r = New-Object TtW+RECT; [TtW]::GetWindowRect($hist, [ref]$r) | Out-Null
"history rect {0},{1} {2}x{3}" -f $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T)
Start-Sleep -Milliseconds 400
Save-TtwScreenShot $hist "$Out\o_history.png"
Stop-Ttw $run | Out-Null
