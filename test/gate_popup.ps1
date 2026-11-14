# gate_popup.ps1 - phase 2a gap 5 gate: popup fidelity and hotspot colours,
# REAL clicks on monitor 2. Reference: real WINHELP.EXE 3.10 (VM): TTTEST
# "modem" popup 448x50 fitted, Chameleon glossary "access" 436 px wrapped
# after "operating"; 2 px black border; stipple shadow offset 12,12; top ~9 px
# under the click, centred; hotspots drawn in their font colour (TTTEST and
# Microsoft's WINHELP.HLP black, Chameleon green).
param([string]$Out = "D:\winhelp_corpus\p2a\gate")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pass = 0; $fail = 0
function Check($name, $cond, $detail = "") { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name  $detail" } }
function Px($bmp, $x, $y) { $c = $bmp.GetPixel($x, $y); return "{0:X2}{1:X2}{2:X2}" -f $c.R, $c.G, $c.B }

"TTTEST 'modem':"
$t = (& "$PSScriptRoot\popup_probe.ps1" -Out $Out -Name pop 2>&1) -join "`n"
Check "one popup and one shadow"                ($t -match 'popups: 1 shadows: 1')
Check "top 9 px under the click"                ($t -match 'top \+9;')
Check "centred on the click (+-1 px)"           ($t -match 'centre offset (0|0\.5|-0\.5|1|-1)(\r|\n|$)')
$m = [regex]::Match($t, 'popup: [-\d]+,[-\d]+ (\d+)x(\d+)')
Check "fitted to the text (one line): narrower than the 480 max" ([int]$m.Groups[1].Value -lt 480 -and [int]$m.Groups[1].Value -gt 400) $m.Value
Check "two lines high (real 50; ours 52)"       ([int]$m.Groups[2].Value -ge 48 -and [int]$m.Groups[2].Value -le 56) $m.Value
Check "shadow offset 12,12, same size"          ($t -match 'shadow: offset 12,12 size (\d+)x(\d+)')
$b = [System.Drawing.Bitmap]::FromFile("$Out\pop_area.png")
$o = [regex]::Match($t, 'popup at \+(\d+),\+(\d+)'); $px = [int]$o.Groups[1].Value; $py = [int]$o.Groups[2].Value
$pw = [int]$m.Groups[1].Value; $ph = [int]$m.Groups[2].Value
Check "2 px black border (left edge)"           ((Px $b $px ($py + 20)) -eq "000000" -and (Px $b ($px + 1) ($py + 20)) -eq "000000" -and (Px $b ($px + 2) ($py + 20)) -ne "000000") ("{0} {1} {2}" -f (Px $b $px ($py+20)), (Px $b ($px+1) ($py+20)), (Px $b ($px+2) ($py+20)))
# shadow: below the popup, a 50% stipple: of 8 neighbouring pixels on a row, about half black
$row = $py + $ph + 5; $blk = 0; for ($i = 0; $i -lt 16; $i++) { if ((Px $b ($px + 100 + $i) $row) -eq "000000") { $blk++ } }
Check "shadow under the popup is a 50% stipple" ($blk -ge 6 -and $blk -le 10) "black $blk of 16"
$blk2 = 0; for ($i = 0; $i -lt 16; $i++) { if ((Px $b ($px + $pw + 5) ($py + 20 + $i)) -eq "000000") { $blk2++ } }
Check "shadow right of the popup is a 50% stipple" ($blk2 -ge 6 -and $blk2 -le 10) "black $blk2 of 16"
$b.Dispose()

"GLOSHLP 'access' (Chameleon, wraps):"
$g = (& "$PSScriptRoot\popup_probe.ps1" -File "D:\winhelp_corpus\wfw311_tree\NETMANAG\GLOSHLP.HLP" -Word access -Out $Out -Name popg 2>&1) -join "`n"
$mg = [regex]::Match($g, 'popup: [-\d]+,[-\d]+ (\d+)x(\d+)')
Check "wrapped popup shrinks to its widest line (real 436; ours < 480)" ([int]$mg.Groups[1].Value -lt 480 -and [int]$mg.Groups[1].Value -gt 400) $mg.Value
Check "three lines high (real 3 lines)"         ([int]$mg.Groups[2].Value -ge 72 -and [int]$mg.Groups[2].Value -le 86) $mg.Value

"Hotspot colours and closing:"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
function Greens($file, $name) {
    $ini = New-TtwIni -Path "$Out\col.ini" -Width 679 -Height 596
    $run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$file`"" -Ini $ini -Log "$Out\$name.log"
    if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); return -1 }
    Set-TtwActive $run.Main | Out-Null; Start-Sleep -Milliseconds 400
    Save-TtwScreenShot $run.Main "$Out\$name.png"
    $bb = [System.Drawing.Bitmap]::FromFile("$Out\$name.png"); $n = 0
    for ($y = 60; $y -lt $bb.Height; $y += 1) { for ($x = 0; $x -lt $bb.Width; $x += 2) { $c = $bb.GetPixel($x, $y); if ($c.R -lt 40 -and $c.G -gt 100 -and $c.B -lt 40) { $n++ } } }
    $bb.Dispose(); Stop-Ttw $run | Out-Null; return $n
}
$gt = Greens "D:\winhelp_build\TTTEST.HLP" "col_tttest"
$gg = Greens "D:\winhelp_corpus\wfw311_tree\NETMANAG\GOPHER.HLP" "col_gopher"
Check "TTTEST hotspots not green (font colour, as the real WinHelp)" ($gt -eq 0) "green px $gt"
Check "Chameleon hotspots green (their fonts are green)"            ($gg -gt 50) "green px $gg"
# a click elsewhere closes the popup and its shadow
$ini = New-TtwIni -Path "$Out\pop.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\close.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
Set-TtwActive $run.Main | Out-Null
$text = [TtIn]::GetDlgItem($run.Main, 0x701); $tt = [TtIn]::GetText($text) -replace "`r`n", "`r"; $idx = $tt.IndexOf("modem")
$packed = [int64][TtIn]::SendMessage($text, 0x00D6, [IntPtr]($idx + 1), [IntPtr]::Zero)
$tr = New-Object TtIn+RECT; [TtIn]::GetWindowRect($text, [ref]$tr) | Out-Null
Invoke-TtwClick ($tr.L + [int]($packed -band 0xFFFF) + 3) ($tr.T + [int](($packed -shr 16) -band 0xFFFF) + 7); Start-Sleep -Milliseconds 800
$before = @([TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "TTWinHelpShadow" }).Count
Invoke-TtwClick ($tr.L + 300) ($tr.B - 40); Start-Sleep -Milliseconds 600
$after = @([TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "TTWinHelpShadow" -or ([TtW]::Cls($_) -eq "MS_WINHELP" -and $_ -ne $run.Main) }).Count
Check "a click elsewhere closes popup and shadow" ($before -eq 1 -and $after -eq 0) "before $before after $after"
Check "monitor 2 guard" ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run | Out-Null
"GATE POPUP: $pass PASS, $fail FAIL"
