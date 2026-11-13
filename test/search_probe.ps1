# search_probe.ps1 - drive OUR Search dialog with real input and capture it.
param([string]$File = "D:\winhelp_build\TTTEST.HLP", [string]$Word = "telnet", [string]$Out = "D:\winhelp_corpus\p2a", [string]$Name = "ours_s", [switch]$KeepOpen)
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null     # 86Box must not hold the mouse
$ini = New-TtwIni -Path "$Out\probe.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$File`"" -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate tries: " + (Set-TtwActive $run.Main)
$btn = [TtW]::Children($run.Main) | Where-Object { [TtW]::Cls($_) -eq "Button" -and ([TtW]::Text($_) -match "Search|Index") } | Select-Object -First 1
"search button: [$([TtW]::Text($btn))]"
Invoke-TtwClickWindow $btn
$dlg = Get-TtwDialog $run "Search"
if ($dlg -eq [IntPtr]::Zero) { "NO DIALOG"; foreach ($h in [TtW]::Windows([uint32]$run.Proc.Id, $true)) { "  [$([TtW]::Cls($h))] [$([TtW]::Text($h))]" }; Stop-Ttw $run | Out-Null; exit 1 }
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (dialog)"; exit 2 }
function Snap($tag) {
    Start-Sleep -Milliseconds 300
    Save-TtwShot -Hwnd $dlg -Path "$Out\${Name}_$tag.png" | Out-Null
    $kw = [TtIn]::GetDlgItem($dlg, 0x163); $tp = [TtIn]::GetDlgItem($dlg, 0x164 + 4)
    $ed = [TtIn]::GetDlgItem($dlg, 0x162)
    $tops = @(); for ($i = 0; $i -lt [TtIn]::LbCount($tp); $i++) { $tops += [TtIn]::LbText($tp, $i) }
    "{0}: edit=[{1}] kwsel={2}[{3}] top={4} show={5} goto={6} def=0x{7:x} cancel=[{8}] topics=[{9}] topicsel={10}" -f $tag,
        [TtIn]::GetText($ed), [TtIn]::LbCurSel($kw), [TtIn]::LbText($kw, [TtIn]::LbCurSel($kw)), [TtIn]::LbTop($kw),
        [TtIn]::IsWindowEnabled([TtIn]::GetDlgItem($dlg, 0x164)), [TtIn]::IsWindowEnabled([TtIn]::GetDlgItem($dlg, 0x167)),
        [TtIn]::DefId($dlg), [TtIn]::GetText([TtIn]::GetDlgItem($dlg, 2)), ($tops -join '|'), [TtIn]::LbCurSel($tp)
}
Snap "0open"
Send-TtwKeys $Word.Substring(0, 2); Snap "1prefix"
Send-TtwKeys $Word.Substring(2); Snap "2word"
Send-TtwKeys "{ENTER}"; Snap "3show"
Send-TtwKeys "{DOWN}"; Snap "4down"
$r = New-Object TtW+RECT; [TtW]::GetWindowRect($dlg, [ref]$r) | Out-Null
$m = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$m) | Out-Null
"dialog at +{0},+{1} size {2}x{3}" -f ($r.L - $m.L), ($r.T - $m.T), ($r.R - $r.L), ($r.B - $r.T)
Send-TtwKeys "{ENTER}{WAIT:800}"
"after Go To: dialog gone=" + ([TtW]::Windows([uint32]$run.Proc.Id, $true) -notcontains $dlg)
Save-TtwShot -Hwnd $run.Main -Path "$Out\${Name}_5goto.png" | Out-Null
foreach ($h in [TtW]::Children($run.Main)) { if ([TtW]::Cls($h) -like "RichEdit*") { $t = [TtIn]::GetText($h); "topic text: " + ($t.Substring(0, [Math]::Min(80, $t.Length)) -replace "`r|`n", " / ") } }
if (-not $KeepOpen) { Stop-Ttw $run }
