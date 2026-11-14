# gate_roundtrip.ps1 - authoring chain gate: RTF+HPJ -> HC 3.10.505 (DOSBox) -> .HLP -> TTWINHLP.
# The real-WINHELP half of the round trip is a VM step (see the phase 1 report, s7).
# Needs: D:\winhelp_build with HC.EXE/HC.ERR (HC505, not in the repo: Microsoft licence).
param([string]$Build = "D:\winhelp_build", [switch]$SkipCompile)
. "$PSScriptRoot\ttw_lib.ps1"
$src = "$PSScriptRoot\authoring"
$fail = 0
function Check($name, $ok) { if ($ok) { "PASS $name" } else { "FAIL $name"; $script:fail++ } }
if (-not $SkipCompile) {
    foreach ($f in "TTTEST.RTF", "TTTEST.HPJ") {   # DOS tools: CRLF
        [IO.File]::WriteAllText("$Build\$f", (([IO.File]::ReadAllText("$src\$f")) -replace "`r?`n", "`r`n"), [Text.Encoding]::ASCII)
    }
    Copy-Item "$src\TTLOGO.BMP" $Build -Force
    Remove-Item "$Build\TTTEST.HLP", "$Build\HC.LOG" -ErrorAction SilentlyContinue
    # headless: DOSBox on a private desktop, no window on any monitor (phase 2b)
    $hc = & "$PSScriptRoot\hc_headless.ps1" -Project TTTEST.HPJ -Build $Build -TimeoutS 120 2>$null
    Copy-Item "$Build\TTTEST.LOG" "$Build\HC.LOG" -Force -ErrorAction SilentlyContinue
    Check "HC ran headless (no window on the user's desktop)" (($hc -join "`n") -match "windows seen on the user's desktop: 0")
    $log = Get-Content "$Build\HC.LOG" -Raw
    Check "HC compiled TTTEST.HLP" (Test-Path "$Build\TTTEST.HLP")
    Check "HC reported no errors or warnings" ($log -notmatch '(?i)error|warning')
}
$w = & "$PSScriptRoot\..\build\hlpwalk.exe" "$Build\TTTEST.HLP"
$v = @{}; foreach ($m in [regex]::Matches($w, '(\w+)=(\d+)')) { $v[$m.Groups[1].Value] = [int]$m.Groups[2].Value }
Check "WinHelp 3.1 file (ver 21)" ($v.ver -eq 21)
Check "jumps = 4" ($v.jumps -eq 4)
Check "popups = 2" ($v.popups -eq 2)
Check "bitmap emitted and accepted (picts = objects = 1)" ($v.picts -eq 1 -and $v.objects -eq 1)
Check "keywords = 8" ($v.keywords -eq 8)
Check "no topic failed" ($v.browse_fail -eq 0)
$ini = New-TtwIni -Path "$Build\rt.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$Build\TTTEST.HLP`"" -Ini $ini -Log "$Build\rt.log"
Check "viewer window up on monitor 2" ($run.Main -ne [IntPtr]::Zero -and (Test-TtwOnMonitor2 $run).Count -eq 0)
Check "caption is the TITLE" ([TtW]::Text($run.Main) -eq "Teletext Test Help")
Save-TtwShot -Hwnd $run.Main -Path "$Build\rt_ours.png" | Out-Null
Check "clean exit" ((Stop-Ttw $run) -eq "EXIT 0")
"ROUNDTRIP: $(if ($fail) { "FAIL ($fail)" } else { 'PASS' })"
