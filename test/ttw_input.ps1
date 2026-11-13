# ttw_input.ps1 - REAL input (mouse_event / keybd_event) for TTWINHLP windows.
# Phase 2a. Dot-source after ttw_lib.ps1.
#
# Rules carried from the machine anchor and phase 1 open item 3:
#  - SendInput is inert on this host: keybd_event / mouse_event only.
#  - The first click on a background window is eaten by activation, and
#    SetForegroundWindow from a background process is often ignored: click an
#    INERT spot (the caption), CHECK GetForegroundWindow, retry.
#  - 86Box grabs the host mouse when the VM is "captured": release it
#    (Ctrl+End) before driving our viewer, or our clicks land in the guest.
#  - Clicks are refused anywhere off monitor 2.
#
# Copyright 2026 The Teletext. GNU LGPL 2.1 or later.

if (-not ("TtIn" -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public class TtIn {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr e);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint c);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  // WM_GETTEXT through SendMessage (marshalled cross-process; GetWindowText is not for edits)
  public static string GetText(IntPtr h) { var sb = new StringBuilder(4096); SendMessage(h, 0x000D, (IntPtr)4096, sb); return sb.ToString(); }
  public static int LbCount(IntPtr h) { return (int)SendMessage(h, 0x018B, IntPtr.Zero, IntPtr.Zero); }
  public static int LbCurSel(IntPtr h) { return (int)SendMessage(h, 0x0188, IntPtr.Zero, IntPtr.Zero); }
  public static int LbTop(IntPtr h) { return (int)SendMessage(h, 0x018E, IntPtr.Zero, IntPtr.Zero); }
  public static string LbText(IntPtr h, int i) { var sb = new StringBuilder(1024); SendMessage(h, 0x0189, (IntPtr)i, sb); return sb.ToString(); }
  public static int DefId(IntPtr h) { return (int)SendMessage(h, 0x0400, IntPtr.Zero, IntPtr.Zero) & 0xFFFF; }
}
'@
}

function Assert-Monitor2([int]$x, [int]$y) {
    $r = Get-TtwMonitor2
    if ($x -lt $r.Left -or $x -ge $r.Right -or $y -lt $r.Top -or $y -ge $r.Bottom) { throw "refusing input at $x,$y (not on monitor 2)" }
}

function Invoke-TtwClick([int]$x, [int]$y, [switch]$Double) {
    Assert-Monitor2 $x $y
    [TtIn]::SetCursorPos($x, $y) | Out-Null; Start-Sleep -Milliseconds 120
    [TtIn]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 50; [TtIn]::mouse_event(4,0,0,0,[IntPtr]::Zero)
    if ($Double) { Start-Sleep -Milliseconds 60; [TtIn]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 50; [TtIn]::mouse_event(4,0,0,0,[IntPtr]::Zero) }
    Start-Sleep -Milliseconds 200
}

# Click the centre of a child window (button, list...) by handle.
function Invoke-TtwClickWindow([IntPtr]$h, [int]$dx = -1, [int]$dy = -1, [switch]$Double) {
    $r = New-Object TtIn+RECT; [TtIn]::GetWindowRect($h, [ref]$r) | Out-Null
    $x = if ($dx -ge 0) { $r.L + $dx } else { [int](($r.L + $r.R) / 2) }
    $y = if ($dy -ge 0) { $r.T + $dy } else { [int](($r.T + $r.B) / 2) }
    Invoke-TtwClick $x $y -Double:$Double
}

# Activation with click-check-retry: click the caption (inert) until the
# window is the foreground window. Returns the number of tries, or -1.
function Set-TtwActive([IntPtr]$top, [int]$tries = 6) {
    for ($i = 1; $i -le $tries; $i++) {
        if ([TtIn]::GetForegroundWindow() -eq $top) { return $i - 1 }
        $r = New-Object TtIn+RECT; [TtIn]::GetWindowRect($top, [ref]$r) | Out-Null
        Invoke-TtwClick ([int](($r.L + $r.R) / 2) + 40) ($r.T + 12)
        Start-Sleep -Milliseconds 250
    }
    if ([TtIn]::GetForegroundWindow() -eq $top) { return $tries }
    return -1
}

# keybd_event typing. Tokens: {ENTER} {TAB} {ESC} {UP} {DOWN} {BS} {ALT+x} {WAIT:ms} {F1}
$TTVK = @{ 'ENTER'=0x0D;'TAB'=0x09;'ESC'=0x1B;'UP'=0x26;'DOWN'=0x28;'LEFT'=0x25;'RIGHT'=0x27;'BS'=0x08;'HOME'=0x24;'END'=0x23;'PGUP'=0x21;'PGDN'=0x22;'SPACE'=0x20;'F1'=0x70;'F4'=0x73 }
function Send-TtwKeys([string]$Text, [int]$Delay = 40) {
    $i = 0
    function Tap([byte]$vk, [byte[]]$mods = @()) {
        foreach ($m in $mods) { [TtIn]::keybd_event($m, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 15 }
        $ext = if ($vk -in 0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28) { 1 } else { 0 }
        [TtIn]::keybd_event($vk, [byte][TtIn]::MapVirtualKey($vk, 0), $ext, [IntPtr]::Zero); Start-Sleep -Milliseconds 20
        [TtIn]::keybd_event($vk, [byte][TtIn]::MapVirtualKey($vk, 0), 2 -bor $ext, [IntPtr]::Zero)
        [array]::Reverse($mods); foreach ($m in $mods) { Start-Sleep -Milliseconds 15; [TtIn]::keybd_event($m, 0, 2, [IntPtr]::Zero) }
        Start-Sleep -Milliseconds $Delay
    }
    while ($i -lt $Text.Length) {
        $ch = $Text[$i]
        if ($ch -eq '{') {
            $end = $Text.IndexOf('}', $i); $tok = $Text.Substring($i + 1, $end - $i - 1); $i = $end + 1
            if ($TTVK.ContainsKey($tok)) { Tap $TTVK[$tok] }
            elseif ($tok -match '^WAIT:(\d+)$') { Start-Sleep -Milliseconds ([int]$Matches[1]) }
            elseif ($tok -match '^ALT\+F4$') { Tap 0x73 @(0x12) }
            elseif ($tok -match '^ALT\+(.)$') { Tap ([byte][char]$Matches[1].ToUpper()) @(0x12) }
            elseif ($tok -match '^SHIFT\+TAB$') { Tap 0x09 @(0x10) }
            else { throw "unknown token {$tok}" }
            continue
        }
        if ($ch -cmatch '[A-Z]') { Tap ([byte][char]$ch) @(0x10) }
        elseif ($ch -cmatch '[a-z0-9]') { Tap ([byte][char]([string]$ch).ToUpper()) }
        elseif ($ch -eq ' ') { Tap 0x20 }
        else { throw "no VK for '$ch'" }
        $i++
    }
}

# The topmost visible #32770 dialog of a process with the given caption.
function Get-TtwDialog($Run, [string]$Caption, [int]$TimeoutMs = 3000) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs) {
        foreach ($h in [TtW]::Windows([uint32]$Run.Proc.Id, $true)) {
            if ([TtW]::Cls($h) -eq "#32770" -and [TtW]::Text($h) -eq $Caption) { return $h }
        }
        Start-Sleep -Milliseconds 100
    }
    return [IntPtr]::Zero
}
