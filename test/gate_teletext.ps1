# gate_teletext.ps1 - phase 2b: Teletext Navigator's TELETEXT.HLP in the viewer,
# with REAL input on monitor 2: walker structure (every jump and keyword
# resolving, titles in the band), Search with a deliberately overlapping
# keyword, a browse sequence end to end, a glossary popup.
param([string]$File = "C:\Users\Dimitri\Projects\mgt-unicorn-suite\help\TELETEXT.HLP",
      [string]$Out = "D:\winhelp_corpus\p2b\gate")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pass = 0; $fail = 0
function Check($name, $cond, $detail = "") { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name  $detail" } }
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null

"Structure (hlpwalk):"
$w = & "$PSScriptRoot\..\build\hlpwalk.exe" $File
$v = @{}; foreach ($m in [regex]::Matches($w, '(\w+)=(\d+)')) { $v[$m.Groups[1].Value] = [int]$m.Groups[2].Value }
"  $($w | Select-Object -Last 1)"
Check "WinHelp 3.1 file" ($v.ver -eq 21)
Check "every topic renders (browse_fail 0)" ($v.browse_fail -eq 0)
Check "every keyword resolves (kwunres 0)" ($v.kwunres -eq 0)
Check "keywords shared by several topics (deliberate overlaps)" ($v.kwmulti -ge 40) "kwmulti $($v.kwmulti)"
Check "every jump topic has its title in the band (nsr 80, nsr_bad 0)" ($v.nsr -eq 80 -and $v.nsr_bad -eq 0) "nsr $($v.nsr) bad $($v.nsr_bad)"
Check "the service icons and the logo are in (picts = objects >= 90)" ($v.picts -ge 90 -and $v.picts -eq $v.objects) "picts $($v.picts) objects $($v.objects)"
Check "jumps and popups present" ($v.jumps -ge 90 -and $v.popups -ge 60) "jumps $($v.jumps) popups $($v.popups)"

"Search, the overlapping keyword 'connecting':"
$s = (& "$PSScriptRoot\search_probe.ps1" -File $File -Word connecting -Out $Out -Name tele) -join "`n"
$want = @("Connecting from the Terminal", "Connecting to a News Server", "Connecting to a Reflector",
          "Connecting to a RIPscrip Board", "Connecting to a Videotex Service", "Connecting to a WAIS Server",
          "Connecting to an FTP Server", "Connecting to an FTP-Mail Server", "Connecting to an IRC Channel",
          "Opening a Gopher Menu", "Opening a Web Page", "Searching an Archie Server", "Starting the NABU")
$m = [regex]::Match($s, '3show: .*topics=\[([^\]]*)\]')
$got = if ($m.Success) { @($m.Groups[1].Value -split '\|') } else { @() }
Check "Show Topics lists the 13 connecting topics" ($got.Count -eq 13 -and -not (Compare-Object $got $want)) ($got -join '|')
$sel = if ($got.Count -gt 1) { $got[1] } else { "" }
Check "Down, Go To opens the second of them" ($s -match ("topic text: .{0,3}" + [regex]::Escape($sel))) $sel   # .{0,3}: the title icon's object mark
Check "monitor 2 guard (search)" ($s -notmatch 'MONITOR FAIL')

function BandTitle($main) {
    $best = $null
    foreach ($h in [TtW]::Children($main)) { if ([TtW]::Cls($h) -like "RichEdit*") { $t = ([TtIn]::GetText($h)).Trim(); if ($t -and $t -notmatch "`r|`n" -and ($null -eq $best -or $t.Length -lt $best.Length)) { $best = $t } } }
    return "$best"
}

"Browse sequence (FTP):"
$ini = New-TtwIni -Path "$Out\tele.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "-i ftp_overview `"$File`"" -Ini $ini -Log "$Out\tele_browse.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
Set-TtwActive $run.Main | Out-Null
# The browse buttons come from the file's [CONFIG] (BrowseButtons()), a
# moment after the window, and the whole button bar is rebuilt on every
# topic change (WINHELP_DeleteButtons): find >> afresh each time.
function NextButton {
    $sw = [Diagnostics.Stopwatch]::StartNew(); $b = $null
    while (-not $b -and $sw.ElapsedMilliseconds -lt 5000) {
        $b = [TtW]::Children($run.Main) | Where-Object { [TtW]::Cls($_) -eq "Button" -and [TtW]::Text($_) -eq "&>>" } | Select-Object -First 1
        if (-not $b) { Start-Sleep -Milliseconds 200 }
    }
    return $b
}
$seq = @(BandTitle $run.Main)
for ($i = 0; $i -lt 4; $i++) {
    $b = NextButton
    if (-not $b) { break }
    Invoke-TtwClickWindow $b; Start-Sleep -Milliseconds 700; $seq += BandTitle $run.Main
}
$next = NextButton
Check "<< >> walk FTP: Overview, Connecting, Settings, Browsing, Troubleshooting" (($seq -join '|') -eq "FTP|Connecting to an FTP Server|FTP Settings|Browsing and Downloading Files|FTP Troubleshooting") ($seq -join '|')
Check ">> dimmed at the end of the sequence" ($next -and -not [TtIn]::IsWindowEnabled($next))
Stop-Ttw $run | Out-Null

"Glossary popup (WAIS):"
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "-i wais_overview `"$File`"" -Ini $ini -Log "$Out\tele_popup.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
Set-TtwActive $run.Main | Out-Null
$text = [TtIn]::GetDlgItem($run.Main, 0x701); $tt = [TtIn]::GetText($text) -replace "`r`n", "`r"; $idx = $tt.IndexOf("WAIS, the Wide Area") - 2   # the glossary hotspot opening "About WAIS" (+3 below: its 2nd letter)
$packed = [int64][TtIn]::SendMessage($text, 0x00D6, [IntPtr]($idx + 3), [IntPtr]::Zero)
$tr = New-Object TtIn+RECT; [TtIn]::GetWindowRect($text, [ref]$tr) | Out-Null
Invoke-TtwClick ($tr.L + [int]($packed -band 0xFFFF) + 3) ($tr.T + [int](($packed -shr 16) -band 0xFFFF) + 7); Start-Sleep -Milliseconds 900
$pop = [TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "MS_WINHELP" -and $_ -ne $run.Main } | Select-Object -First 1
$ptext = ""; if ($pop) { foreach ($h in [TtW]::Children($pop)) { if ([TtW]::Cls($h) -like "RichEdit*") { $ptext += [TtIn]::GetText($h) } } }
Check "clicking WAIS shows its definition in a popup" ($ptext -match "Wide Area Information Servers") ($ptext.Substring(0, [Math]::Min(60, $ptext.Length)))
if ($pop) { Save-TtwShot -Hwnd $run.Main -Path "$Out\tele_popup.png" | Out-Null }
Check "monitor 2 guard" ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run | Out-Null
"GATE TELETEXT: $pass PASS, $fail FAIL"
