# nsr_probe.ps1 - phase 2a gap 2: the non-scrolling band, real input.
# 1. TTTEST.HLP: band present (title), PgDn/Down scroll the text, the band
#    does not move.  2. GOPHER.HLP -> Glossary button -> GLOSHLP "2nd" window:
#    its band holds the A..Z letter hotspots; a real click on "M" must
#    scroll the glossary to the M entries.
param([string]$Out = "D:\winhelp_corpus\p2a", [string]$Name = "nsr")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
if (-not ("TtNsr" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public class TtNsr {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, ref POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  // EM_GETSCROLLPOS is not marshalled cross-process; use the first visible line instead
  public static int FirstLine(IntPtr h) { return (int)SendMessage(h, 0x00CE, IntPtr.Zero, IntPtr.Zero); }
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
}
'@
}
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
function Rect($h) { $r = New-Object TtIn+RECT; [TtIn]::GetWindowRect($h, [ref]$r) | Out-Null; return $r }
function Band($main) {
    $n = [TtNsr]::GetDlgItem($main, 0x702); $t = [TtNsr]::GetDlgItem($main, 0x701); $rl = [TtNsr]::GetDlgItem($main, 0x703)
    $nr = Rect $n; $tr = Rect $t; $rr = Rect $rl
    return @{ N = $n; T = $t; Vis = [TtNsr]::IsWindowVisible($n); NTop = $nr.T; NH = $nr.B - $nr.T; RuleTop = $rr.T; RuleH = $rr.B - $rr.T; TextTop = $tr.T
              NText = [TtIn]::GetText($n); First = [TtNsr]::FirstLine($t) }
}

"--- TTTEST.HLP"
$ini = New-TtwIni -Path "$Out\nsr.ini" -Width 679 -Height 190   # short, so the text must scroll
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate tries: " + (Set-TtwActive $run.Main)
$b0 = Band $run.Main
"band0: vis={0} text=[{1}] h={2} rule={3}px at band+{4} text at rule+{5} first={6}" -f $b0.Vis, $b0.NText.Trim(), $b0.NH, $b0.RuleH, ($b0.RuleTop - $b0.NTop), ($b0.TextTop - $b0.RuleTop), $b0.First
Save-TtwShot -Hwnd $run.Main -Path "$Out\${Name}_tt0.png" | Out-Null
Send-TtwKeys "{DOWN}{DOWN}{DOWN}{DOWN}{WAIT:300}"
$b1 = Band $run.Main
"band1 (after 4x Down): text=[{0}] ntop={1} first={2}" -f $b1.NText.Trim(), $b1.NTop, $b1.First
Save-TtwShot -Hwnd $run.Main -Path "$Out\${Name}_tt1.png" | Out-Null
"scrolled=" + ($b1.First -gt $b0.First) + " band_fixed=" + ($b1.NTop -eq $b0.NTop -and $b1.NText -eq $b0.NText)
# browse to a topic, band follows the topic
Send-TtwKeys "{WAIT:200}"
$nb = [TtW]::Children($run.Main) | Where-Object { [TtW]::Cls($_) -eq "Button" -and [TtW]::Text($_) -eq "&>>" } | Select-Object -First 1
Invoke-TtwClickWindow $nb; Start-Sleep -Milliseconds 600
$b2 = Band $run.Main
"band2 (after >>): text=[{0}]" -f $b2.NText.Trim()
Stop-Ttw $run | Out-Null

"--- GOPHER.HLP -> Glossary"
$ini = New-TtwIni -Path "$Out\nsr.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_corpus\wfw311_tree\NETMANAG\GOPHER.HLP`"" -Ini $ini -Log "$Out\${Name}_g.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate tries: " + (Set-TtwActive $run.Main)
$g = Band $run.Main
"gopher band: [{0}]" -f $g.NText.Trim()
$gb = [TtW]::Children($run.Main) | Where-Object { [TtW]::Cls($_) -eq "Button" -and [TtW]::Text($_) -match "Glossary" } | Select-Object -First 1
Invoke-TtwClickWindow $gb; Start-Sleep -Milliseconds 1500
$glo = [TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "MS_WINHELP" -and [TtW]::Text($_) -eq "Glossary" } | Select-Object -First 1
if (-not $glo) { "NO GLOSSARY WINDOW"; foreach ($h in [TtW]::Windows([uint32]$run.Proc.Id, $true)) { "  [$([TtW]::Cls($h))] [$([TtW]::Text($h))]" }; Stop-Ttw $run | Out-Null; exit 1 }
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (glossary)"; exit 2 }
$gl0 = Band $glo
$gr = Rect $glo
"glossary: {0}x{1}; band vis={2} h={3} text=[{4}] first={5}" -f ($gr.R - $gr.L), ($gr.B - $gr.T), $gl0.Vis, $gl0.NH, ($gl0.NText -replace "\s+", " ").Trim(), $gl0.First
Save-TtwShot -Hwnd $glo -Path "$Out\${Name}_glo0.png" | Out-Null
# letter M: the 13th letter bitmap of the band strip. The letters are
# pictures (no text to find), so the target is measured from our render
# (nsr_glo0.png): first letter centre 9 px from the band's left edge, 20 px
# pitch, row centre 44 px below the band's top.
$n = $gl0.N
$nr = Rect $n
if (-not $gl0.Vis) { $nr = Rect $gl0.T; "(no band: TTWINHLP_NO_NSR; letters are in the text window)" }
$mx = $nr.L + 9 + 12 * 20; $my = $nr.T + 44
"M target: band+{0},+{1}" -f ($mx - $nr.L), ($my - $nr.T)
"activate glossary tries: " + (Set-TtwActive $glo)
Invoke-TtwClick $mx $my; Start-Sleep -Milliseconds 1200
$gl1 = Band $glo
$t1 = [TtIn]::GetText($gl1.T)
"after click M: first={0}; text starts [{1}]" -f $gl1.First, (($t1.Substring(0, [Math]::Min(60, $t1.Length))) -replace "\s+", " ")
Save-TtwShot -Hwnd $glo -Path "$Out\${Name}_glo1.png" | Out-Null
Save-TtwScreenShot $glo "$Out\${Name}_glo1_screen.png"
Stop-Ttw $run
