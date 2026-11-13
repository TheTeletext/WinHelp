# vm_key.ps1 - type into the 86Box guest with keybd_event scancodes.
# Like C:\Users\pa1067\box_key.ps1 but (a) keybd_event, since SendInput is
# inert on this host, and (b) it foregrounds 86Box WITHOUT maximizing it
# (box_lib's Fg() maximizes, which moves it to whatever monitor it is on).
# Tokens: {ENTER} {TAB} {ESC} {BS} {SPACE} {UP} {DOWN} {LEFT} {RIGHT} {HOME} {END}
#         {F1}..{F12} {ALT+x} {CTRL+x} {SHIFT+TAB} {WAIT:ms}
param([Parameter(Mandatory=$true)][string]$Text, [int]$CharDelay = 40)
if (-not ("VmKey" -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public class VmKey {
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr e);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int m);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  public static IntPtr FindBox() {
    IntPtr found = IntPtr.Zero;
    EnumWindows((h, l) => { if (!IsWindowVisible(h)) return true; var sb = new StringBuilder(256); GetWindowText(h, sb, 256);
      string t = sb.ToString(); if (t.Contains("86Box") && !t.Contains("Manager")) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  public static void Fg(IntPtr h) {
    IntPtr fg = GetForegroundWindow(); uint pid; uint ft = GetWindowThreadProcessId(fg, out pid);
    uint ct = GetCurrentThreadId(); AttachThreadInput(ct, ft, true);
    BringWindowToTop(h); SetForegroundWindow(h); AttachThreadInput(ct, ft, false);
  }
  public static void Key(byte sc, bool up, bool ext) {
    uint f = 0x0008; if (up) f |= 0x0002; if (ext) f |= 0x0001;
    keybd_event(0, sc, f, IntPtr.Zero);
  }
}
'@
}
$SCAN = @{ 'a'=0x1E;'b'=0x30;'c'=0x2E;'d'=0x20;'e'=0x12;'f'=0x21;'g'=0x22;'h'=0x23;'i'=0x17;'j'=0x24;'k'=0x25;'l'=0x26;'m'=0x32;
 'n'=0x31;'o'=0x18;'p'=0x19;'q'=0x10;'r'=0x13;'s'=0x1F;'t'=0x14;'u'=0x16;'v'=0x2F;'w'=0x11;'x'=0x2D;'y'=0x15;'z'=0x2C;
 '1'=0x02;'2'=0x03;'3'=0x04;'4'=0x05;'5'=0x06;'6'=0x07;'7'=0x08;'8'=0x09;'9'=0x0A;'0'=0x0B;
 '-'=0x0C;'='=0x0D;'['=0x1A;']'=0x1B;';'=0x27;"'"=0x28;'`'=0x29;'\'=0x2B;','=0x33;'.'=0x34;'/'=0x35;' '=0x39 }
$SH = @{ '!'=0x02;'@'=0x03;'#'=0x04;'$'=0x05;'%'=0x06;'^'=0x07;'&'=0x08;'*'=0x09;'('=0x0A;')'=0x0B;
 '_'=0x0C;'+'=0x0D;'{'=0x1A;'}'=0x1B;':'=0x27;'"'=0x28;'~'=0x29;'|'=0x2B;'<'=0x33;'>'=0x34;'?'=0x35 }
$EXT = @{ 'UP'=0x48;'DOWN'=0x50;'LEFT'=0x4B;'RIGHT'=0x4D;'HOME'=0x47;'END'=0x4F;'DEL'=0x53;'PGUP'=0x49;'PGDN'=0x51 }
$PLAIN = @{ 'ENTER'=0x1C;'TAB'=0x0F;'ESC'=0x01;'BS'=0x0E;'SPACE'=0x39 }
$FN = @(0,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x57,0x58)

$h = [VmKey]::FindBox(); if ($h -eq [IntPtr]::Zero) { Write-Output "NOWIN"; exit 1 }
[VmKey]::Fg($h); Start-Sleep -Milliseconds 250

function Tap([byte]$sc, [bool]$ext = $false, [byte[]]$mods = @()) {
    foreach ($m in $mods) { [VmKey]::Key($m, $false, $false); Start-Sleep -Milliseconds 15 }
    [VmKey]::Key($sc, $false, $ext); Start-Sleep -Milliseconds 25; [VmKey]::Key($sc, $true, $ext)
    [array]::Reverse($mods); foreach ($m in $mods) { Start-Sleep -Milliseconds 15; [VmKey]::Key($m, $true, $false) }
    Start-Sleep -Milliseconds $CharDelay
}
$i = 0
while ($i -lt $Text.Length) {
    $ch = $Text[$i]
    if ($ch -eq '{') {
        $end = $Text.IndexOf('}', $i); $tok = $Text.Substring($i + 1, $end - $i - 1); $i = $end + 1
        if ($PLAIN.ContainsKey($tok)) { Tap $PLAIN[$tok] }
        elseif ($EXT.ContainsKey($tok)) { Tap $EXT[$tok] $true }
        elseif ($tok -match '^F(\d+)$') { Tap $FN[[int]$Matches[1]] }
        elseif ($tok -match '^WAIT:(\d+)$') { Start-Sleep -Milliseconds ([int]$Matches[1]) }
        elseif ($tok -eq 'SHIFT+TAB') { Tap 0x0F $false @(0x2A) }
        elseif ($tok -match '^ALT\+F(\d+)$') { Tap $FN[[int]$Matches[1]] $false @(0x38) }
        elseif ($tok -match '^ALT\+(.)$') { $c = $Matches[1].ToLower(); $code = if ($SCAN.ContainsKey($c)) { $SCAN[$c] } else { $SH[$c] }; $mods = if ($SH.ContainsKey($c)) { @(0x38, 0x2A) } else { @(0x38) }; Tap $code $false $mods }
        elseif ($tok -match '^CTRL\+(.)$') { Tap $SCAN[$Matches[1].ToLower()] $false @(0x1D) }
        else { Write-Output "unknown token {$tok}" }
        continue
    }
    $s = [string]$ch
    if ($ch -cmatch '[A-Z]') { Tap $SCAN[$s.ToLower()] $false @(0x2A) }
    elseif ($SCAN.ContainsKey($s)) { Tap $SCAN[$s] }
    elseif ($SH.ContainsKey($s)) { Tap $SH[$s] $false @(0x2A) }
    else { Write-Output "skip '$ch'" }
    $i++
}
Write-Output "TYPED: $Text"
