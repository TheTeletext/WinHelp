# ttw_lib.ps1 - drive TTWINHLP.EXE for the gate and the fidelity audit.
#
# Monitor rule (inherited from mgt-unicorn-suite\CLAUDE.md): every test
# window goes on the SECONDARY monitor before it is shown. TTWINHLP opens
# its main window at M_WindowPosition from its INI, so the harness writes a
# test-only INI (TTWINHLP_INI) placing it on monitor 2 BEFORE launch; the
# operator's own INI is never touched.
#
# Copyright 2026 The Teletext. GNU LGPL 2.1 or later.

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
if (-not ("TtW" -as [type])) {
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
public class TtW {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int m);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int m);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint f, IntPtr e);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  public static List<IntPtr> Windows(uint pid, bool visibleOnly) {
    var res = new List<IntPtr>();
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p);
      if (p == pid && (!visibleOnly || IsWindowVisible(h))) res.Add(h); return true; }, IntPtr.Zero);
    return res;
  }
  public static List<IntPtr> Children(IntPtr parent) {
    var res = new List<IntPtr>();
    EnumChildWindows(parent, (h, l) => { res.Add(h); return true; }, IntPtr.Zero);
    return res;
  }
  public static string Cls(IntPtr h) { var sb = new StringBuilder(256); GetClassName(h, sb, 256); return sb.ToString(); }
  public static string Text(IntPtr h) { var sb = new StringBuilder(512); GetWindowText(h, sb, 512); return sb.ToString(); }
  public static Bitmap Capture(IntPtr h) {
    RECT r; GetWindowRect(h, out r);
    int w = r.R - r.L, ht = r.B - r.T;
    if (w <= 0 || ht <= 0) return null;
    var bmp = new Bitmap(w, ht);
    using (var g = Graphics.FromImage(bmp)) {
      IntPtr dc = g.GetHdc();
      PrintWindow(h, dc, 2);   // PW_RENDERFULLCONTENT: works when occluded
      g.ReleaseHdc(dc);
    }
    return bmp;
  }
}
'@
}

function Get-TtwMonitor2 {
    $m2 = [System.Windows.Forms.Screen]::AllScreens | Where-Object { -not $_.Primary } | Select-Object -First 1
    if (-not $m2) { Write-Host "  NOTE: one monitor only; using the primary"; $m2 = [System.Windows.Forms.Screen]::PrimaryScreen }
    return $m2.WorkingArea
}

# Writes a test INI that places the main window on monitor 2.
function New-TtwIni {
    param([string]$Path, [int]$Width = 640, [int]$Height = 480, [int]$Margin = 20)
    $r = Get-TtwMonitor2
    $x = $r.X + $Margin; $y = $r.Y + $Margin
    $ini = "[Windows Help]`r`nM_WindowPosition=[$x,$y,$Width,$Height,0]`r`nH_WindowPosition=[$($x + $Width + 10),$y,300,300,0]`r`n"
    [System.IO.File]::WriteAllText($Path, $ini, [System.Text.Encoding]::ASCII)
    return $Path
}

# Launch the viewer on a file; returns a hashtable with the process and main hwnd.
function Start-Ttw {
    param([string]$Exe, [string]$Arguments, [string]$Ini, [string]$Log, [int]$TimeoutMs = 8000)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    $psi.Arguments = $Arguments
    $psi.UseShellExecute = $false
    $psi.EnvironmentVariables["TTWINHLP_INI"] = $Ini
    if ($Log) { $psi.EnvironmentVariables["TTWINHLP_LOG"] = $Log }
    $p = [System.Diagnostics.Process]::Start($psi)
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $main = [IntPtr]::Zero
    while ($sw.ElapsedMilliseconds -lt $TimeoutMs -and -not $p.HasExited) {
        foreach ($h in [TtW]::Windows([uint32]$p.Id, $true)) {
            if ([TtW]::Cls($h) -eq "MS_WINHELP") { $main = $h; break }
        }
        if ($main -ne [IntPtr]::Zero) { break }
        Start-Sleep -Milliseconds 100
    }
    Start-Sleep -Milliseconds 600   # let RichEdit lay out the topic
    return @{ Proc = $p; Main = $main }
}

function Save-TtwShot {
    param([IntPtr]$Hwnd, [string]$Path)
    $bmp = [TtW]::Capture($Hwnd)
    if ($bmp) { $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose(); return $true }
    return $false
}

function Stop-Ttw {
    param($Run, [int]$TimeoutMs = 4000)
    if (-not $Run.Proc.HasExited) {
        foreach ($h in [TtW]::Windows([uint32]$Run.Proc.Id, $true)) { [TtW]::PostMessage($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
        if (-not $Run.Proc.WaitForExit($TimeoutMs)) { $Run.Proc.Kill(); return "KILLED" }
    }
    return ("EXIT {0}" -f $Run.Proc.ExitCode)
}

# Guard: every visible window of the process must be on monitor 2.
# Returns a list of offending "hwnd class rect" strings (empty = clean).
function Test-TtwOnMonitor2 {
    param($Run)
    $r = Get-TtwMonitor2
    $bad = @()
    foreach ($h in [TtW]::Windows([uint32]$Run.Proc.Id, $true)) {
        $wr = New-Object TtW+RECT; [TtW]::GetWindowRect($h, [ref]$wr) | Out-Null
        $cx = [int](($wr.L + $wr.R) / 2); $cy = [int](($wr.T + $wr.B) / 2)
        if ($cx -lt $r.Left -or $cx -ge $r.Right -or $cy -lt $r.Top -or $cy -ge $r.Bottom) {
            $bad += ("{0} {1} [{2},{3} {4}x{5}]" -f $h, [TtW]::Cls($h), $wr.L, $wr.T, ($wr.R - $wr.L), ($wr.B - $wr.T))
        }
    }
    return ,$bad
}
