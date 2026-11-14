# screen_shot.ps1 - capture a window as it is ON SCREEN (CopyFromScreen), for
# cases where PrintWindow returns a stale or blank frame. Monitor 2 only.
function Save-TtwScreenShot([IntPtr]$Hwnd, [string]$Path) {
    $r = New-Object TtW+RECT; [TtW]::GetWindowRect($Hwnd, [ref]$r) | Out-Null
    $m2 = Get-TtwMonitor2
    if ($r.L -lt $m2.Left -or $r.R -gt $m2.Right) { throw "window not wholly on monitor 2" }
    $bmp = New-Object System.Drawing.Bitmap(($r.R - $r.L), ($r.B - $r.T))
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size); $g.Dispose()
    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}
