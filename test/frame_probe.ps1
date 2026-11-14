# frame_probe.ps1 - the slim (4 px) sizable frame: geometry Windows reports,
# a zoomed screen capture of the top-left corner, and hit tests.
param([string]$File = "D:\winhelp_build\TTTEST.HLP", [string]$Out = "D:\winhelp_corpus\p2a", [string]$Name = "slim")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
if (-not ("TtNc" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public class TtNc {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct TBI { public int cb; public RECT rc; [MarshalAs(UnmanagedType.ByValArray, SizeConst=6)] public int[] st; }
  [StructLayout(LayoutKind.Sequential)] public struct MBI { public int cb; public RECT rc; public IntPtr menu, focus; public int flags; }
  [DllImport("user32.dll")] public static extern bool GetTitleBarInfo(IntPtr h, ref TBI t);
  [DllImport("user32.dll")] public static extern bool GetMenuBarInfo(IntPtr h, int obj, int item, ref MBI m);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static string Title(IntPtr h) { var t = new TBI(); t.cb = Marshal.SizeOf(t); t.st = new int[6]; GetTitleBarInfo(h, ref t); return t.rc.L+" "+t.rc.T+" "+t.rc.R+" "+t.rc.B; }
  public static string Menu(IntPtr h) { var m = new MBI(); m.cb = Marshal.SizeOf(m); GetMenuBarInfo(h, -3, 0, ref m); return m.rc.L+" "+m.rc.T+" "+m.rc.R+" "+m.rc.B; }
  public static int Hit(IntPtr h, int x, int y) { return (int)SendMessage(h, 0x84, IntPtr.Zero, (IntPtr)((y << 16) | (x & 0xFFFF))); }
}
'@
}
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\slim.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"$File`"" -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main); Start-Sleep -Milliseconds 400
$wr = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null
$cr = New-Object TtNc+RECT; [TtNc]::GetClientRect($run.Main, [ref]$cr) | Out-Null
$p = New-Object TtNc+PT; [TtNc]::ClientToScreen($run.Main, [ref]$p) | Out-Null
"window {0},{1} {2}x{3}" -f $wr.L, $wr.T, ($wr.R - $wr.L), ($wr.B - $wr.T)
"client at +{0},+{1} {2}x{3}; right gap {4}, bottom gap {5}" -f ($p.X - $wr.L), ($p.Y - $wr.T), $cr.R, $cr.B, ($wr.R - $p.X - $cr.R), ($wr.B - $p.Y - $cr.B)
$tb = ([TtNc]::Title($run.Main) -split ' ') | ForEach-Object { [int]$_ }
$mb = ([TtNc]::Menu($run.Main) -split ' ') | ForEach-Object { [int]$_ }
"titlebar +{0},+{1} to +{2},+{3}" -f ($tb[0]-$wr.L), ($tb[1]-$wr.T), ($tb[2]-$wr.L), ($tb[3]-$wr.T)
"menubar  +{0},+{1} to +{2},+{3}" -f ($mb[0]-$wr.L), ($mb[1]-$wr.T), ($mb[2]-$wr.L), ($mb[3]-$wr.T)
foreach ($pt in @(@(1,300,"left edge"), @(3,300,"left frame inner"), @(5,300,"client side"), @(300,1,"top edge"), @(678,300,"right edge"), @(300,595,"bottom edge"), @(1,1,"top-left corner"), @(678,595,"bottom-right corner"), @(300,12,"caption"), @(20,36,"menu strip y36"), @(20,44,"menu strip y44"), @(20,50,"y50"))) {
    "hit +{0},+{1} ({2}): {3}" -f $pt[0], $pt[1], $pt[2], [TtNc]::Hit($run.Main, $wr.L + $pt[0], $wr.T + $pt[1]) }
$bmp = New-Object System.Drawing.Bitmap(200, 90); $g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($wr.L - 10, $wr.T - 10, 0, 0, $bmp.Size); $g.Dispose(); $bmp.Save("$Out\${Name}_corner.png"); $bmp.Dispose()
$bmp = New-Object System.Drawing.Bitmap(60, 60); $g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($wr.R - 50, $wr.B - 50, 0, 0, $bmp.Size); $g.Dispose(); $bmp.Save("$Out\${Name}_br.png"); $bmp.Dispose()
Stop-Ttw $run
