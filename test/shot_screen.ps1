# shot_screen.ps1 - open a help file in TTWINHLP on monitor 2 and capture it
# as it is ON SCREEN (after an activation click), plus PrintWindow.
param([Parameter(Mandatory=$true)][string]$File, [Parameter(Mandatory=$true)][string]$Name,
      [int]$W = 679, [int]$H = 596, [string]$Topic = "", [string]$Out = "D:\winhelp_corpus\p2a", [switch]$NoActivate)
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\screen.ini" -Width $W -Height $H
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments ($(if ($Topic) { "-i $Topic " } else { "" }) + "`"$File`"") -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
if (-not $NoActivate) { "activate: " + (Set-TtwActive $run.Main) }
Start-Sleep -Milliseconds 600
Save-TtwScreenShot $run.Main "$Out\${Name}_screen.png"
Save-TtwShot -Hwnd $run.Main -Path "$Out\${Name}_pw.png" | Out-Null
"$Out\${Name}_screen.png"
Stop-Ttw $run
