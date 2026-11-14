# nsr_cursor.ps1 - diagnosis: which cursor does the band show over each glossary letter?
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
if (-not ("TtCur" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public class TtCur {
  [StructLayout(LayoutKind.Sequential)] public struct CI { public int cb, flags; public IntPtr h; public int x, y; }
  [DllImport("user32.dll")] public static extern bool GetCursorInfo(ref CI c);
  [DllImport("user32.dll")] public static extern IntPtr LoadCursor(IntPtr i, int id);
  public static IntPtr Now() { var c = new CI(); c.cb = Marshal.SizeOf(c); GetCursorInfo(ref c); return c.h; }
}
'@
}
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$Out = "D:\winhelp_corpus\p2a"
$ini = New-TtwIni -Path "$Out\nsr.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_corpus\wfw311_tree\NETMANAG\GLOSHLP.HLP`"" -Ini $ini -Log "$Out\nsr_cur.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
$n = [TtIn]::GetDlgItem($run.Main, 0x702)
$r = New-Object TtIn+RECT; [TtIn]::GetWindowRect($n, [ref]$r) | Out-Null
"band {0},{1} {2}x{3}" -f $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T)
Save-TtwShot -Hwnd $run.Main -Path "$Out\nsr_cur.png" | Out-Null
$arrow = [TtCur]::LoadCursor([IntPtr]::Zero, 32512)
foreach ($dy in 10, 30, 44, 50) { foreach ($dx in 9, 29, 249) {
    Assert-Monitor2 ($r.L + $dx) ($r.T + $dy)
    [TtIn]::SetCursorPos($r.L + $dx, $r.T + $dy) | Out-Null; Start-Sleep -Milliseconds 150
    $c = [TtCur]::Now(); "  +{0},+{1}: {2}" -f $dx, $dy, $(if ($c -eq $arrow) { "arrow" } else { "OTHER $c" })
} }
Stop-Ttw $run
