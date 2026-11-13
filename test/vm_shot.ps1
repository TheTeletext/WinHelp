# vm_shot.ps1 - capture the 86Box guest display (client area) to a PNG.
# Phase 2a. Usage: vm_shot.ps1 -Path D:\...\x.png
# The 86Box window lives on monitor 2 (vm_start.ps1), so CopyFromScreen reads
# monitor 2 only. Refuses if the window is not there.
param([Parameter(Mandatory=$true)][string]$Path)
. "C:\Users\pa1067\box_lib.ps1"
Add-Type -AssemblyName System.Drawing
$h = [Box]::FindEmu(); if ($h -eq [IntPtr]::Zero) { "NOWIN"; exit 1 }
$cr = New-Object Box+RECT; [void][Box]::GetClientRect($h, [ref]$cr)
$tl = New-Object Box+POINT; [void][Box]::ClientToScreen($h, [ref]$tl)
if ($tl.X -ge 0) { "REFUSED: 86Box is not on monitor 2"; exit 2 }
$w = $cr.Right - $cr.Left; $ht = $cr.Bottom - $cr.Top
$bmp = New-Object System.Drawing.Bitmap($w, $ht)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($tl.X, $tl.Y, 0, 0, (New-Object System.Drawing.Size($w, $ht)))
$g.Dispose(); $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
$Path
