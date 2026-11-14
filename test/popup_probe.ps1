# popup_probe.ps1 - phase 2a gap 5: click a popup hotspot with REAL input and
# measure the popup (size, border, shadow) on screen. Monitor 2 only.
param([string]$File = "D:\winhelp_build\TTTEST.HLP", [string]$Word = "modem", [int]$Nth = 1,
      [string]$Out = "D:\winhelp_corpus\p2a", [string]$Name = "pop", [int]$W = 679, [int]$H = 596)
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\pop.ini" -Width $W -Height $H
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$File`"" -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
$text = [TtIn]::GetDlgItem($run.Main, 0x701)
$t = [TtIn]::GetText($text) -replace "`r`n", "`r"     # RichEdit counts a paragraph mark as one character
$idx = -1; for ($k = 0; $k -lt $Nth; $k++) { $idx = $t.IndexOf($Word, $idx + 1) }
if ($idx -lt 0) { "WORD NOT FOUND"; Stop-Ttw $run | Out-Null; exit 1 }
$packed = [int64][TtIn]::SendMessage($text, 0x00D6, [IntPtr]($idx + 1), [IntPtr]::Zero)   # EM_POSFROMCHAR, edit form
$cx = [int]($packed -band 0xFFFF); $cy = [int](($packed -shr 16) -band 0xFFFF)
$tr = New-Object TtIn+RECT; [TtIn]::GetWindowRect($text, [ref]$tr) | Out-Null
$clickX = $tr.L + $cx + 3; $clickY = $tr.T + $cy + 7
"hotspot '$Word' at char $idx -> screen $clickX,$clickY"
Invoke-TtwClick $clickX $clickY; Start-Sleep -Milliseconds 900
$pops = @([TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "MS_WINHELP" -and $_ -ne $run.Main })
$shadow = @([TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "TTWinHelpShadow" })
"popups: $($pops.Count) shadows: $($shadow.Count)"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (popup)"; exit 2 }
if ($pops.Count) {
    $p = $pops[0]; $pr = New-Object TtW+RECT; [TtW]::GetWindowRect($p, [ref]$pr) | Out-Null
    "popup: {0},{1} {2}x{3}; relative to click: left {4}, top +{5}; centre offset {6}" -f $pr.L, $pr.T, ($pr.R - $pr.L), ($pr.B - $pr.T), ($pr.L - $clickX), ($pr.T - $clickY), ((($pr.L + $pr.R) / 2) - $clickX)
    if ($shadow.Count) { $sr = New-Object TtW+RECT; [TtW]::GetWindowRect($shadow[0], [ref]$sr) | Out-Null; "shadow: offset {0},{1} size {2}x{3}" -f ($sr.L - $pr.L), ($sr.T - $pr.T), ($sr.R - $sr.L), ($sr.B - $sr.T) }
    # screen capture of the area around the popup, including the shadow
    $m2 = Get-TtwMonitor2
    $x0 = [Math]::Max($m2.Left, $pr.L - 20); $y0 = [Math]::Max($m2.Top, $pr.T - 30)
    $bmp = New-Object System.Drawing.Bitmap(($pr.R - $pr.L + 50), ($pr.B - $pr.T + 60))
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($x0, $y0, 0, 0, $bmp.Size); $g.Dispose()
    $bmp.Save("$Out\${Name}_area.png", [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    "capture origin {0},{1} (popup at +{2},+{3})" -f $x0, $y0, ($pr.L - $x0), ($pr.T - $y0)
    "popup text: [" + (([TtIn]::GetText([TtIn]::GetDlgItem($p, 0x701))) -replace "\s+", " ").Trim() + "]"
}
Stop-Ttw $run
