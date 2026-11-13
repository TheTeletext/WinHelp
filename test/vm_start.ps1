# vm_start.ps1 - boot the WfW 3.11 reference VM (86Box) on MONITOR 2.
# The VM cfg carries window_remember/window_coordinates on monitor 2; this
# script also polls the 86Box window and moves it there if it shows up
# anywhere else (belt and braces for the monitor rule).
param([string]$VmDir = "D:\VM\86Box\Windows 3.11 WFW", [string]$Box = "D:\VM\86Box\86Box.exe")
. "$PSScriptRoot\ttw_lib.ps1"
$m2 = Get-TtwMonitor2
$p = Start-Process -FilePath $Box -ArgumentList @("-P", "`"$VmDir`"") -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew()
$moved = 0
while ($sw.ElapsedMilliseconds -lt 20000) {
    foreach ($h in [TtW]::Windows([uint32]$p.Id, $true)) {
        $r = New-Object TtW+RECT; [TtW]::GetWindowRect($h, [ref]$r) | Out-Null
        if ($r.L -ge $m2.Right -or $r.R -le $m2.Left) {
            [MgtWinTt]::MoveWindow($h, $m2.X + 20, $m2.Y + 10, $r.R - $r.L, $r.B - $r.T, $true) | Out-Null
            $moved++
            Write-Host ("moved [{0}] from {1},{2} at {3} ms" -f [TtW]::Text($h), $r.L, $r.T, $sw.ElapsedMilliseconds)
        }
    }
    Start-Sleep -Milliseconds 50
}
foreach ($h in [TtW]::Windows([uint32]$p.Id, $true)) {
    $r = New-Object TtW+RECT; [TtW]::GetWindowRect($h, [ref]$r) | Out-Null
    Write-Host ("86Box pid {0} [{1}] at {2},{3} {4}x{5}; moved {6}" -f $p.Id, [TtW]::Text($h), $r.L, $r.T, ($r.R - $r.L), ($r.B - $r.T), $moved)
}
