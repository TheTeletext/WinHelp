# run_watched.ps1 - run a GUI tool, keeping every window it shows on MONITOR 2
# (moved within 50 ms if one appears elsewhere; each move is reported).
param([Parameter(Mandatory=$true)][string]$Exe, [string[]]$ArgList = @(), [int]$TimeoutS = 120)
. "$PSScriptRoot\ttw_lib.ps1"
$m2 = Get-TtwMonitor2
$p = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
$sw = [Diagnostics.Stopwatch]::StartNew(); $moved = 0
while (-not $p.HasExited -and $sw.Elapsed.TotalSeconds -lt $TimeoutS) {
    foreach ($h in [TtW]::Windows([uint32]$p.Id, $true)) {
        $r = New-Object TtW+RECT; [TtW]::GetWindowRect($h, [ref]$r) | Out-Null
        $cx = [int](($r.L + $r.R) / 2)
        if ($cx -lt $m2.Left -or $cx -ge $m2.Right) {
            [MgtWinTt]::MoveWindow($h, $m2.X + 100, $m2.Y + 150, $r.R - $r.L, $r.B - $r.T, $true) | Out-Null
            $moved++; Write-Host ("moved [{0}] from {1},{2} at {3} ms" -f [TtW]::Text($h), $r.L, $r.T, $sw.ElapsedMilliseconds)
        }
    }
    Start-Sleep -Milliseconds 50
}
if (-not $p.HasExited) { $p.Kill(); "TIMEOUT, killed" } else { "exit $($p.ExitCode) after $([int]$sw.Elapsed.TotalSeconds) s; moved $moved" }
