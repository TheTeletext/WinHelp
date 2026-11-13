# vm_mouse.ps1 - mouse for the 86Box guest (relative PS/2-style mouse).
#   capture        click at the 86Box client centre (86Box grabs the mouse)
#   home           slam the guest pointer to the top-left corner (0,0)
#   to X Y         home, then move to guest pixel X,Y (1:1 scale, no accel assumed)
#   click | dclick | rclick
#   release        Ctrl+End (86Box's release-mouse key)
# Refuses to click anywhere that is not on the secondary monitor.
param([Parameter(Mandatory=$true)][string]$Action, [int]$X = 0, [int]$Y = 0)
. "C:\Users\pa1067\box_lib.ps1"
& "$PSScriptRoot\vm_key.ps1" "{WAIT:1}" | Out-Null      # loads [VmKey]
if (-not ("VmMo" -as [type])) {
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public class VmMo {
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
}
'@
}
$h = [Box]::FindEmu(); if ($h -eq [IntPtr]::Zero) { "NOWIN"; exit 1 }
[VmKey]::Fg($h); Start-Sleep -Milliseconds 150
function LClick { [VmMo]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 70; [VmMo]::mouse_event(4,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 120 }
function Move-Rel([int]$dx, [int]$dy) {
    # small steps so the guest driver neither accelerates nor coalesces
    $sx = [Math]::Sign($dx); $sy = [Math]::Sign($dy); $ax = [Math]::Abs($dx); $ay = [Math]::Abs($dy)
    while ($ax -gt 0 -or $ay -gt 0) {
        $mx = [Math]::Min(2, $ax); $my = [Math]::Min(2, $ay)
        [VmMo]::mouse_event(1, $sx * $mx, $sy * $my, 0, [IntPtr]::Zero); $ax -= $mx; $ay -= $my
        Start-Sleep -Milliseconds 8
    }
}
switch ($Action) {
    "capture" {
        $cr = New-Object Box+RECT; [void][Box]::GetClientRect($h, [ref]$cr)
        $tl = New-Object Box+POINT; [void][Box]::ClientToScreen($h, [ref]$tl)
        $cx = $tl.X + [int](($cr.Right - $cr.Left) / 2); $cy = $tl.Y + [int](($cr.Bottom - $cr.Top) / 2)
        if ($cx -ge 0) { "REFUSED: $cx,$cy is not on monitor 2"; exit 2 }
        [VmMo]::SetCursorPos($cx, $cy) | Out-Null; Start-Sleep -Milliseconds 150; LClick; "CAPTURED at $cx,$cy"
    }
    "home" { 1..4 | ForEach-Object { [VmMo]::mouse_event(1, -2000, -2000, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 40 }; "HOME" }
    "to" { 1..4 | ForEach-Object { [VmMo]::mouse_event(1, -2000, -2000, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 40 }; Move-Rel $X $Y; "AT $X,$Y" }
    "move" { Move-Rel $X $Y; "MOVED $X,$Y" }
    "click" { LClick; "CLICK" }
    "dclick" { LClick; Start-Sleep -Milliseconds 60; LClick; "DCLICK" }
    "rclick" { [VmMo]::mouse_event(8,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 70; [VmMo]::mouse_event(16,0,0,0,[IntPtr]::Zero); "RCLICK" }
    "release" { [VmKey]::Key(0x1D,$false,$false); [VmKey]::Key(0x4F,$false,$true); Start-Sleep -Milliseconds 60; [VmKey]::Key(0x4F,$true,$true); [VmKey]::Key(0x1D,$true,$false); "RELEASED" }
}
