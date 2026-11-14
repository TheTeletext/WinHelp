# hc_headless.ps1 - run the Microsoft Help Compiler 3.10.505 under DOSBox
# Staging with NO window on any monitor (phase 1 s7: the windowed run flashed
# on the primary for < 50 ms). Every visible top-level window the process
# shows is still counted and reported, as proof.
# Method: SDL's no-window drivers crash DOSBox Staging 0.82.2 (0xC0000409),
# so DOSBox runs on a PRIVATE desktop (CreateDesktop + STARTUPINFO.lpDesktop):
# its window exists but is on no monitor.
# HC.EXE / HC.ERR are not in the repo (Microsoft licence); they live in $Build.
param([Parameter(Mandatory=$true)][string]$Project,   # e.g. TELETEXT.HPJ, in $Build
      [string]$Build = "D:\winhelp_build", [int]$TimeoutS = 300)
. "$PSScriptRoot\ttw_lib.ps1"
$conf = "$Build\hc_headless.conf"
@"
[sdl]
fullscreen = false
output = surface
[dosbox]
machine = svga_s3
memsize = 16
[cpu]
cycles = max
[mixer]
nosound = true
[speaker]
pcspeaker = false
"@ | Set-Content -Encoding ascii $conf
$log = [IO.Path]::GetFileNameWithoutExtension($Project).ToUpper() + ".LOG"
Remove-Item "$Build\$log", "$Build\HCDONE.TXT" -ErrorAction SilentlyContinue
if (-not ("HcDesk" -as [type])) {
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public class HcDesk {
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct SI {
    public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
  [StructLayout(LayoutKind.Sequential)] public struct PI { public IntPtr hProcess, hThread; public int pid, tid; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr CreateDesktop(string name, IntPtr dev, IntPtr dm, int flags, uint access, IntPtr sa);
  [DllImport("user32.dll")] public static extern bool CloseDesktop(IntPtr d);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern bool CreateProcess(string app, System.Text.StringBuilder cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string dir, ref SI si, out PI pi);
  [DllImport("kernel32.dll")] public static extern uint WaitForSingleObject(IntPtr h, uint ms);
  [DllImport("kernel32.dll")] public static extern bool GetExitCodeProcess(IntPtr h, out int code);
  [DllImport("kernel32.dll")] public static extern bool TerminateProcess(IntPtr h, int code);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
'@
}
# A private desktop: the DOSBox window is created there and is on NO monitor.
$deskName = "TtwHelpCompiler"
$desk = [HcDesk]::CreateDesktop($deskName, [IntPtr]::Zero, [IntPtr]::Zero, 0, 0x10000000, [IntPtr]::Zero)
if ($desk -eq [IntPtr]::Zero) { "CreateDesktop failed"; exit 3 }
$si = New-Object HcDesk+SI; $si.cb = [Runtime.InteropServices.Marshal]::SizeOf($si); $si.lpDesktop = $deskName
$si.dwFlags = 0x100   # STARTF_USESTDHANDLES with no handles: DOSBox's log goes nowhere
$pi = New-Object HcDesk+PI
$cmd = New-Object Text.StringBuilder ("`"C:\Program Files\DOSBox Staging\dosbox.exe`" -noprimaryconf -nolocalconf -conf `"$conf`" -c `"mount c $Build`" -c c: -c `"HC $Project > $log`" -c `"echo done > HCDONE.TXT`" -c exit")
$sw = [Diagnostics.Stopwatch]::StartNew()
if (-not [HcDesk]::CreateProcess([NullString]::Value, $cmd, [IntPtr]::Zero, [IntPtr]::Zero, $false, 0, [IntPtr]::Zero, $Build, [ref]$si, [ref]$pi)) { "CreateProcess failed: " + [Runtime.InteropServices.Marshal]::GetLastWin32Error(); [HcDesk]::CloseDesktop($desk) | Out-Null; exit 3 }
# proof: no window of this process on OUR desktop, polled every 20 ms
$seen = 0
while ([HcDesk]::WaitForSingleObject($pi.hProcess, 20) -ne 0) {
    $seen += @([TtW]::Windows([uint32]$pi.pid, $true)).Count
    # Staging blocks a quick "exit" ("program quit after only 1.2 seconds"):
    # HC is done when the sentinel exists; end DOSBox then.
    if (Test-Path "$Build\HCDONE.TXT") { Start-Sleep -Milliseconds 300; [HcDesk]::TerminateProcess($pi.hProcess, 0) | Out-Null; [HcDesk]::WaitForSingleObject($pi.hProcess, 5000) | Out-Null; break }
    if ($sw.Elapsed.TotalSeconds -gt $TimeoutS) { [HcDesk]::TerminateProcess($pi.hProcess, 1) | Out-Null; "TIMEOUT, killed"; break }
}
$code = 0; [HcDesk]::GetExitCodeProcess($pi.hProcess, [ref]$code) | Out-Null
[HcDesk]::CloseHandle($pi.hProcess) | Out-Null; [HcDesk]::CloseHandle($pi.hThread) | Out-Null
[HcDesk]::CloseDesktop($desk) | Out-Null
"exit $code after $([math]::Round($sw.Elapsed.TotalSeconds,1)) s on private desktop '$deskName'; windows seen on the user's desktop: $seen"
if (Test-Path "$Build\$log") { Get-Content "$Build\$log" }
