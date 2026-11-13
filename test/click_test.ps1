# click_test.ps1 - open a file in TTWINHLP on monitor 2, activation click, click client point (cx,cy), screenshot.
param([string]$File, [int]$Cx, [int]$Cy, [string]$Name, [switch]$Riched20)
. "$PSScriptRoot\ttw_lib.ps1"
$out = "D:\winhelp_corpus\compare"
$ini = New-TtwIni -Path "$out\click.ini" -Width 679 -Height 596
if ($Riched20) { $env:TTWINHLP_RICHED20 = "1" } else { Remove-Item Env:TTWINHLP_RICHED20 -ErrorAction SilentlyContinue }
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$File`"" -Ini $ini -Log "$out\$Name.log"
Remove-Item Env:TTWINHLP_RICHED20 -ErrorAction SilentlyContinue
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
$wr = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null
function Click($x, $y) { if ($x -ge 0) { throw "refusing click off monitor 2" }; [TtW]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 150; [TtW]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 60; [TtW]::mouse_event(4,0,0,0,[IntPtr]::Zero) }
[TtW]::SetForegroundWindow($run.Main) | Out-Null
Click ($wr.L + 300) ($wr.T + 10); Start-Sleep -Milliseconds 400      # caption: activation, inert
Click ($wr.L + $Cx) ($wr.T + $Cy); Start-Sleep -Milliseconds 1500
$fg = [TtW]::GetForegroundWindow()
foreach ($h in [TtW]::Windows([uint32]$run.Proc.Id, $true)) { "win [$([TtW]::Cls($h))] [$([TtW]::Text($h))]" }
Save-TtwShot -Hwnd $run.Main -Path "$out\$Name.png" | Out-Null
"fg=$fg main=$($run.Main) shot=$out\$Name.png"
Stop-Ttw $run | Out-Null
