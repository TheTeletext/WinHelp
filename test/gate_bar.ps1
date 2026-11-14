# gate_bar.ps1 - phase 2a gap 3 gate: WinHelp 3.1 button bar, menus and
# strings, with REAL input on monitor 2 at the real window size (679x596).
# Expected values: the real WINHELP.EXE 3.10 in the WfW 3.11 VM
# (real_menu_*.png, vm_tt.png pixel scan: buttons 59x21, edge to edge).
param([string]$Out = "D:\winhelp_corpus\p2a\gate")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
if (-not ("TtMenu" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Text; using System.Runtime.InteropServices;
public class TtMenu {
  [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr m);
  [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr m, int p);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr m, uint id, StringBuilder s, int max, uint flags);
  [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr m, uint id, uint flags);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr h, string name);
  // w31 owns the menu bar (19 px caption, 2026-11-13): SetMenu(NULL), the HMENU in a property
  public static IntPtr BarMenu(IntPtr h) { IntPtr m = GetMenu(h); return m != IntPtr.Zero ? m : GetProp(h, "NavW31.Menu"); }
  public static string Item(IntPtr m, int p) { var sb = new StringBuilder(256); GetMenuString(m, (uint)p, sb, 256, 0x400); return sb.ToString(); }
  public static string Tree(IntPtr menu) {
    var sb = new StringBuilder();
    int n = GetMenuItemCount(menu);
    for (int i = 0; i < n; i++) {
      sb.Append(Item(menu, i)); sb.Append(" {");
      IntPtr sub = GetSubMenu(menu, i);
      int k = GetMenuItemCount(sub);
      for (int j = 0; j < k; j++) { if ((GetMenuState(sub, (uint)j, 0x400) & 0x800) != 0) sb.Append("-"); else sb.Append(Item(sub, j)); if (j < k - 1) sb.Append("|"); }
      sb.Append("} ");
    }
    return sb.ToString().Trim();
  }
}
'@
}
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pass = 0; $fail = 0
function Check($name, $cond, $detail = "") { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name  $detail" } }
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null
$ini = New-TtwIni -Path "$Out\bar.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\bar.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }

"Menus:"
$tree = [TtMenu]::Tree([TtMenu]::BarMenu($run.Main))
"  $tree"
Check "menu bar = File Edit Bookmark Help, items as WinHelp 3.1" ($tree -ceq '&File {&Open...|-|&Print Topic|P&rint Setup...|-|E&xit} &Edit {&Copy...|-|&Annotate...} Book&mark {&Define...} &Help {&How to Use Help|Always on &Top|-|&About Help...}') $tree

"Button bar:"
$btns = @([TtW]::Children($run.Main) | Where-Object { [TtW]::Cls($_) -eq "Button" } | ForEach-Object {
    $r = New-Object TtW+RECT; [TtW]::GetWindowRect($_, [ref]$r) | Out-Null
    [pscustomobject]@{ H = $_; T = [TtW]::Text($_); X = $r.L; Y = $r.T; W = $r.R - $r.L; Ht = $r.B - $r.T; On = [TtMenu]::IsWindowEnabled($_) } } | Sort-Object X)
$labels = ($btns | ForEach-Object { $_.T }) -join " "
"  $labels"
Check "labels and order: Contents Search Back History << >>" ($labels -ceq '&Contents &Search &Back His&tory &<< &>>') $labels
$sizes = ($btns | ForEach-Object { "{0}x{1}" -f $_.W, $_.Ht } | Sort-Object -Unique) -join ","
Check "every button 59x21 (real pixel scan)" ($sizes -eq "59x21") $sizes
$adj = $true; for ($i = 1; $i -lt $btns.Count; $i++) { if ($btns[$i].X -ne $btns[$i-1].X + $btns[$i-1].W) { $adj = $false } }
Check "buttons edge to edge" $adj
$states = ($btns | ForEach-Object { if ($_.On) { "on" } else { "off" } }) -join " "
Check "first topic: Back and << grey (real: Back grey, << grey)" ($states -eq "on on off on off on") $states

"Mnemonics (real keys):"
"  activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+s}{WAIT:800}"
$dlg = Get-TtwDialog $run "Search" 2000
Check "Alt+S opens Search" ($dlg -ne [IntPtr]::Zero)
if ($dlg -ne [IntPtr]::Zero) { Send-TtwKeys "{ESC}{WAIT:400}" }
"  activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+t}{WAIT:800}"
$hist = [TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "WHHistory" -or [TtW]::Text($_) -match "History" } | Select-Object -First 1
Check "Alt+T opens History" ($null -ne $hist)
if ($hist) { [TtW]::PostMessage($hist, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 300 }
"  activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+h}{WAIT:300}a{WAIT:900}"
$ab = Get-TtwDialog $run "About Help" 2000
Check "Help > About Help opens 'About Help'" ($ab -ne [IntPtr]::Zero)
if ($ab -ne [IntPtr]::Zero) {
    if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (about)"; exit 2 }
    $txt = ([TtW]::Children($ab) | ForEach-Object { [TtIn]::GetText($_) }) -join " / "
    Save-TtwShot -Hwnd $ab -Path "$Out\about.png" | Out-Null
    Check "About names the product"                 ($txt -match 'The Teletext WinHelp')
    Check "About credits the Wine project (LGPL s8)" ($txt -match "Wine's WinHelp viewer" -and $txt -match 'Wine project authors')
    Check "About states the LGPL 2.1+"               ($txt -match 'Lesser General Public' -and $txt -match '2\.1 or later')
    Check "About gives the source URL"               ($txt -match 'github\.com/TheTeletext/WinHelp')
    Send-TtwKeys "{ENTER}{WAIT:400}"
}
"Strings:"
$all = @([TtW]::Text($run.Main), $tree, $labels) -join " "
Check "no visible 'Wine Help' left (caption, menus, buttons)" ($all -notmatch 'Wine Help')
Save-TtwShot -Hwnd $run.Main -Path "$Out\bar.png" | Out-Null
Check "monitor 2 guard" ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run | Out-Null
"GATE BAR: $pass PASS, $fail FAIL"
