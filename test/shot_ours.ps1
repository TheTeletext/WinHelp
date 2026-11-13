# shot_ours.ps1 - open a help file in TTWINHLP on monitor 2 at a given size, screenshot it,
# optionally type keys at it (keybd_event VKs via SendKeys-free path), and leave it open or close it.
param([Parameter(Mandatory=$true)][string]$File, [Parameter(Mandatory=$true)][string]$Name,
      [int]$W = 679, [int]$H = 596, [string]$Out = "D:\winhelp_corpus\compare", [switch]$KeepOpen,
      [string]$Topic = "")
. "$PSScriptRoot\ttw_lib.ps1"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$ini = New-TtwIni -Path "$Out\compare.ini" -Width $W -Height $H
$args2 = if ($Topic) { "-i $Topic `"$File`"" } else { "`"$File`"" }
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments $args2 -Ini $ini -Log "$Out\$Name.log"
$bad = Test-TtwOnMonitor2 $run
if ($bad.Count) { $run.Proc.Kill(); "MONITOR FAIL $bad"; exit 2 }
Start-Sleep -Milliseconds 500
Save-TtwShot -Hwnd $run.Main -Path "$Out\ours_$Name.png" | Out-Null
"$Out\ours_$Name.png pid=$($run.Proc.Id) hwnd=$($run.Main)"
if (-not $KeepOpen) { Stop-Ttw $run | Out-Null }
