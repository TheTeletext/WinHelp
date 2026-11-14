# menu_probe.ps1 - the w31-drawn menu bar, REAL input on monitor 2:
# a click on File, Alt+H, F10 + Right + Down; each open drop-down captured
# from the screen (window origin) for comparison with the real WinHelp
# (real_menu_f.png, real_menu_h.png: window at 109,47 in those shots).
param([string]$Out = "D:\winhelp_corpus\p2a", [string]$Name = "menu")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
if (-not ("TtMw" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
public class TtMw {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int m);
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Rt, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  public static string Menus() {
    var sb = new StringBuilder();
    EnumWindows((h, l) => { var c = new StringBuilder(64); GetClassName(h, c, 64);
      if (c.ToString() == "#32768" && IsWindowVisible(h)) { R r; GetWindowRect(h, out r); sb.Append(r.L + "," + r.T + "," + (r.Rt - r.L) + "x" + (r.B - r.T) + " "); }
      return true; }, IntPtr.Zero);
    return sb.ToString().Trim();
  }
}
'@
}
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\menu.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\$Name.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
$wr = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null
function Snap($tag) {
    Start-Sleep -Milliseconds 500
    $bmp = New-Object System.Drawing.Bitmap(340, 230); $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($wr.L, $wr.T, 0, 0, $bmp.Size); $g.Dispose(); $bmp.Save("$Out\${Name}_$tag.png"); $bmp.Dispose()
    $m = [TtMw]::Menus()
    $rel = ($m -split ' ' | Where-Object { $_ } | ForEach-Object { $p = $_ -split ','; "+{0},+{1} {2}" -f ([int]$p[0] - $wr.L), ([int]$p[1] - $wr.T), $p[2] }) -join ' '
    "{0}: menus [{1}]" -f $tag, $rel
}
# 1. a click on File
Invoke-TtwClick ($wr.L + 22) ($wr.T + 31); Snap "file_click"
Send-TtwKeys "{ESC}{WAIT:300}"; Snap "after_esc"
# 2. Alt+H: Help, first item highlighted
Send-TtwKeys "{ALT+h}"; Snap "alt_h"
Send-TtwKeys "{RIGHT}"; Snap "alt_h_right"
Send-TtwKeys "{ESC}{WAIT:300}"
# 3. F10: File highlighted in the bar, Right -> Edit, Down -> Edit's drop-down
[TtIn]::keybd_event(0x79, 0x44, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 30; [TtIn]::keybd_event(0x79, 0x44, 2, [IntPtr]::Zero)
Snap "f10"
Send-TtwKeys "{RIGHT}"; Snap "f10_right"
Send-TtwKeys "{DOWN}"; Snap "f10_down"
Send-TtwKeys "{ESC}{WAIT:300}"
# 4. a command through the menu: Help > About Help with the mouse
Invoke-TtwClick ($wr.L + 190) ($wr.T + 31); Start-Sleep -Milliseconds 500
"menus after clicking Help: " + [TtMw]::Menus()
Send-TtwKeys "{UP}{ENTER}{WAIT:900}"
$ab = Get-TtwDialog $run "About Help" 2000
"About via the menu: " + ($ab -ne [IntPtr]::Zero)
if ($ab -ne [IntPtr]::Zero) { Send-TtwKeys "{ENTER}{WAIT:400}" }
"monitor guard: " + ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run
