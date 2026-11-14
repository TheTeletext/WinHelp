# gate_chrome.ps1 - phase 2a gap 4 gate: Windows 3.1 window chrome (w31.c,
# carried from Navigator), checked on SCREEN pixels with real input on
# monitor 2, at the real window size (679x596). Reference values: the real
# WINHELP.EXE 3.10 in the WfW 3.11 VM (vm_tt.png, real_tttest...png).
param([string]$Out = "D:\winhelp_corpus\p2a\gate")
. "$PSScriptRoot\ttw_lib.ps1"
. "$PSScriptRoot\ttw_input.ps1"
. "$PSScriptRoot\screen_shot.ps1"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pass = 0; $fail = 0
function Check($name, $cond, $detail = "") { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name  $detail" } }
function Px($bmp, $x, $y) { $c = $bmp.GetPixel($x, $y); return "{0:X2}{1:X2}{2:X2}" -f $c.R, $c.G, $c.B }
function Snap($h, $name) { Save-TtwScreenShot $h "$Out\$name.png"; return [System.Drawing.Bitmap]::FromFile("$Out\$name.png") }
& "$PSScriptRoot\vm_mouse.ps1" release 2>$null | Out-Null

$ini = New-TtwIni -Path "$Out\chrome.ini" -Width 679 -Height 596
$run = Start-Ttw -Exe "$PSScriptRoot\..\build\TTWINHLP.EXE" -Arguments "`"D:\winhelp_build\TTTEST.HLP`"" -Ini $ini -Log "$Out\chrome.log"
if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL"; exit 2 }
"activate: " + (Set-TtwActive $run.Main)
Start-Sleep -Milliseconds 400
$wr = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null
$W = $wr.R - $wr.L
"Main window (active):"
$b = Snap $run.Main "chrome_main_active"
Check "outer frame pixel is black (Win 3.1 frame line)" ((Px $b 0 100) -eq "000000") (Px $b 0 100)
# the real sizable frame: the double black outline (black, gray, black).
# Ours is 8 px (Windows 11's sizing border) where the real one is 4.
$fr = (0..8 | ForEach-Object { Px $b $_ 300 }) -join " "
Check "sizable frame = double black outline (black, gray..., black)" ($fr -match '^000000 C0C0C0 (C0C0C0 )*000000 FFFFFF') $fr
Check "caption navy when active"                       ((Px $b 200 10) -eq "000080") (Px $b 200 10)
Check "title text white, centred"                       ((0..$W | Where-Object { $_ -gt 250 -and $_ -lt 430 } | ForEach-Object { Px $b $_ 13 }) -contains "FFFFFF")
Check "system-menu box at the left (gray face)"        ((Px $b 12 6) -eq "C0C0C0") (Px $b 12 6)
Check "menu bar white to the right edge"               ((Px $b ($W - 12) 33) -eq "FFFFFF") (Px $b ($W - 12) 33)
# button bar: from the Contents button's own rect
$btn = [TtW]::Children($run.Main) | Where-Object { [TtW]::Text($_) -eq "&Contents" } | Select-Object -First 1
$br = New-Object TtW+RECT; [TtW]::GetWindowRect($btn, [ref]$br) | Out-Null
$bx = $br.L - $wr.L; $by = $br.T - $wr.T
Check "button frame black (left edge, mid-height)"     ((Px $b $bx ($by + 10)) -eq "000000") (Px $b $bx ($by + 10))
Check "button face #C0C0C0"                            ((Px $b ($bx + 30) ($by + 4)) -eq "C0C0C0") (Px $b ($bx + 30) ($by + 4))
Check "bar background #808080 right of the buttons"    ((Px $b ($W - 30) ($by + 10)) -eq "808080") (Px $b ($W - 30) ($by + 10))
$rule = [TtIn]::GetDlgItem($run.Main, 0x703); $rr = New-Object TtW+RECT; [TtW]::GetWindowRect($rule, [ref]$rr) | Out-Null
Check "band rule solid black"                          ((Px $b 300 ($rr.T - $wr.T)) -eq "000000") (Px $b 300 ($rr.T - $wr.T))
# 4 px sizable frame, as the real one: black, gray, gray, black, then the client
$fr4 = (0..4 | ForEach-Object { Px $b $_ 300 }) -join " "
Check "sizable frame is 4 px: black gray gray black | client (real main window)" ($fr4 -eq "000000 C0C0C0 C0C0C0 000000 FFFFFF") $fr4
$frR = (($W - 5)..($W - 1) | ForEach-Object { Px $b $_ 300 }) -join " "
Check "right edge mirrored"                            ($frR -eq "FFFFFF 000000 C0C0C0 C0C0C0 000000") $frR
$b.Dispose()
# the slim frame still resizes: a REAL drag of the right edge by 40 px
$x0 = $wr.R - 2; $y0 = $wr.T + 300
Assert-Monitor2 $x0 $y0; Assert-Monitor2 ($x0 + 40) $y0
[TtIn]::SetCursorPos($x0, $y0) | Out-Null; Start-Sleep -Milliseconds 150
[TtIn]::mouse_event(2,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 150
for ($i = 1; $i -le 8; $i++) { [TtIn]::SetCursorPos($x0 + 5 * $i, $y0) | Out-Null; Start-Sleep -Milliseconds 40 }
[TtIn]::mouse_event(4,0,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 400
$wr2 = New-Object TtW+RECT; [TtW]::GetWindowRect($run.Main, [ref]$wr2) | Out-Null
Check "drag on the 4 px right edge resizes (+40 px)" (($wr2.R - $wr2.L) - ($wr.R - $wr.L) -ge 36) ("{0} -> {1}" -f ($wr.R - $wr.L), ($wr2.R - $wr2.L))
[TtW]::GetWindowRect($run.Main, [ref]$wr) | Out-Null; $W = $wr.R - $wr.L

"Dialogs:"
Send-TtwKeys "{ALT+s}{WAIT:900}"
$dlg = Get-TtwDialog $run "Search" 2000
if ($dlg -ne [IntPtr]::Zero) {
    if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (search)"; exit 2 }
    $d = Snap $dlg "chrome_search"
    Check "Search: navy 3.1 caption"     ((Px $d 200 10) -eq "000080") (Px $d 200 10)
    Check "Search: white dialog face"    ((Px $d 200 120) -eq "FFFFFF") (Px $d 200 120)
    # the real fixed-dialog frame: 1 px black + 4 px navy (active), then white
    $edge = (0..5 | ForEach-Object { Px $d $_ 150 }) -join " "
    Check "Search frame = black + 4 px navy, as the real one (left)"  ($edge -eq "000000 000080 000080 000080 000080 FFFFFF") $edge
    $W2 = $d.Width
    $edge2 = (($W2 - 6)..($W2 - 1) | ForEach-Object { Px $d $_ 150 }) -join " "
    Check "Search frame = 4 px navy + black (right)"  ($edge2 -eq "FFFFFF 000080 000080 000080 000080 000000") $edge2
    $d.Dispose()
    # phase 2b: DEACTIVATED, the 3.1 default title colours (InactiveTitle white,
    # InactiveTitleText black): the frame band and the caption go white. (The
    # VM showed grey: its custom scheme "The Blues", read from its WIN.INI.)
    # Another window, made ON monitor 2, takes the activation by a real click.
    Add-Type -AssemblyName System.Windows.Forms
    $m2 = Get-TtwMonitor2
    $form = New-Object System.Windows.Forms.Form
    $form.StartPosition = "Manual"; $form.Location = New-Object System.Drawing.Point(($m2.X + 20), ($m2.Bottom - 200))
    $form.Size = New-Object System.Drawing.Size(220, 140); $form.Text = "other window"
    $form.Show(); [System.Windows.Forms.Application]::DoEvents()
    Invoke-TtwClick ($m2.X + 120) ($m2.Bottom - 120); [System.Windows.Forms.Application]::DoEvents(); Start-Sleep -Milliseconds 700
    $d = Snap $dlg "chrome_search_inactive"
    $edgeI = (0..5 | ForEach-Object { Px $d $_ 150 }) -join " "
    Check "Search deactivated: frame = black + 4 px WHITE (3.1 default InactiveTitle)" ($edgeI -eq "000000 FFFFFF FFFFFF FFFFFF FFFFFF FFFFFF") $edgeI
    Check "Search deactivated: caption white"  ((Px $d 200 10) -eq "FFFFFF") (Px $d 200 10)
    $d.Dispose()
    $form.Close(); $form.Dispose()
    Set-TtwActive $dlg | Out-Null
    Send-TtwKeys "{ESC}{WAIT:400}"
} else { Check "Search dialog opened" $false }
"  activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+h}{WAIT:300}a{WAIT:900}"
$ab = Get-TtwDialog $run "About Help" 2000
if ($ab -ne [IntPtr]::Zero) {
    $d = Snap $ab "chrome_about"
    Check "About: navy 3.1 caption"      ((Px $d 150 10) -eq "000080") (Px $d 150 10)
    $edge = (0..5 | ForEach-Object { Px $d $_ 120 }) -join " "
    Check "About frame = black + 4 px navy (real About Help)" ($edge -eq "000000 000080 000080 000080 000080 FFFFFF") $edge
    $d.Dispose()
    Send-TtwKeys "{ENTER}{WAIT:400}"
} else { Check "About opened" $false }
"  activate: " + (Set-TtwActive $run.Main)
Send-TtwKeys "{ALT+t}{WAIT:900}"
$hist = [TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "WHHistory" } | Select-Object -First 1
if ($hist) {
    if ((Test-TtwOnMonitor2 $run).Count) { $run.Proc.Kill(); "MONITOR FAIL (history)"; exit 2 }
    Start-Sleep -Milliseconds 300
    $m = Snap $run.Main "chrome_main_inactive"
    Check "main caption WHITE when inactive (Win 3.1 rule)" ((Px $m 200 10) -eq "FFFFFF") (Px $m 200 10)
    $m.Dispose()
    $h = Snap $hist "chrome_history"
    Check "History: 3.1 caption (navy, active)" ((Px $h 150 10) -eq "000080") (Px $h 150 10)
    $h.Dispose()
} else { Check "History opened" $false }
Check "monitor 2 guard" ((Test-TtwOnMonitor2 $run).Count -eq 0)
Stop-Ttw $run | Out-Null
"GATE CHROME: $pass PASS, $fail FAIL"
