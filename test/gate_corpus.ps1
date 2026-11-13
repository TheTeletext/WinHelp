# gate_corpus.ps1 - TTWINHLP corpus gate (WinHelp Revival phase 1).
#
#   A. hlpwalk.exe per file (own process): load, every topic -> RTF ->
#      hidden RichEdit. PASS = exit 0, browse_fail 0.
#   B. TTWINHLP.EXE per file on MONITOR 2 (test INI placed before launch):
#      main window appears, every visible window stays on monitor 2,
#      screenshot, WM_CLOSE, exit code 0 within 4 s, no WER crash.
#   FIXME/ERR lines from the viewer's audit log are counted per file.
#
# Usage: gate_corpus.ps1 -Corpus D:\winhelp_corpus\wfw311 -Out <dir> [-Filter *GOPHER*]
param(
    [string]$Corpus = "D:\winhelp_corpus\wfw311_tree",
    [string]$Out = "D:\winhelp_corpus\gate",
    [string]$Filter = "*.HLP",
    [string]$Exe = "$PSScriptRoot\..\build\TTWINHLP.EXE",
    [string]$Walk = "$PSScriptRoot\..\build\hlpwalk.exe",
    [switch]$WalkOnly
)
. "$PSScriptRoot\ttw_lib.ps1"
New-Item -ItemType Directory -Force -Path $Out, "$Out\shots", "$Out\logs" | Out-Null
$ini = New-TtwIni -Path "$Out\gate.ini"
$rows = @()
$files = Get-ChildItem -Path $Corpus -Filter $Filter -Recurse | Sort-Object FullName
foreach ($f in $files) {
    # WinHelp magic 3F 5F 03 00; DOS help formats are out of scope
    $hdr = [System.IO.File]::ReadAllBytes($f.FullName)[0..3]
    if (-not ($hdr[0] -eq 0x3F -and $hdr[1] -eq 0x5F -and $hdr[2] -eq 3 -and $hdr[3] -eq 0)) { continue }
    $row = [ordered]@{ file = $f.FullName.Substring($Corpus.Length).TrimStart([char]92) }
    $flat = $row.file -replace '[\\/]', '__'

    # A. headless walk
    $wout = & $Walk $f.FullName 2>&1 | Out-String
    $row.walk_rc = $LASTEXITCODE
    foreach ($kv in [regex]::Matches($wout, '(\w+)=(\d+)')) { $row[$kv.Groups[1].Value] = [int64]$kv.Groups[2].Value }
    $row.walk = if ($LASTEXITCODE -eq 0 -and $row.browse_fail -eq 0) { "PASS" } else { "FAIL" }

    if (-not $WalkOnly) {
        # B. the real viewer, on monitor 2
        $log = "$Out\logs\$flat.log"
        Remove-Item $log -ErrorAction SilentlyContinue
        [System.IO.File]::WriteAllText($ini, [System.IO.File]::ReadAllText($ini))  # positions unchanged per file
        $run = Start-Ttw -Exe $Exe -Arguments ('"' + $f.FullName + '"') -Ini $ini -Log $log
        $bad = Test-TtwOnMonitor2 $run
        $row.mon2 = if ($bad.Count) { "OFF: $($bad -join '; ')" } else { "ok" }
        $row.window = if ($run.Main -ne [IntPtr]::Zero) { [TtW]::Text($run.Main) } else { "" }
        # any dialog up after load (e.g. "Cannot find ...") is recorded with its text
        $row.dialog = (@([TtW]::Windows([uint32]$run.Proc.Id, $true) | Where-Object { [TtW]::Cls($_) -eq "#32770" } |
            ForEach-Object { ([TtW]::Children($_) | Where-Object { [TtW]::Cls($_) -eq "Static" } | ForEach-Object { [TtW]::Text($_) }) -join " " }) -join " | ")
        if ($bad.Count) { $run.Proc.Kill(); $row.close = "KILLED(monitor)" }
        else {
            if ($run.Main -ne [IntPtr]::Zero) { Save-TtwShot -Hwnd $run.Main -Path "$Out\shots\$flat.png" | Out-Null }
            $row.close = Stop-Ttw $run
        }
        # Window positions are saved on close; rewrite the test INI so every file starts identically.
        $ini = New-TtwIni -Path "$Out\gate.ini"
        $lines = if (Test-Path $log) { Get-Content $log } else { @() }
        $row.err = @($lines | Where-Object { $_ -like "err:*" }).Count
        $row.fixme = @($lines | Where-Object { $_ -like "fixme:*" }).Count
        $row.viewer = if ($row.mon2 -eq "ok" -and $run.Main -ne [IntPtr]::Zero -and $row.close -eq "EXIT 0" -and -not $row.dialog) { "PASS" } else { "FAIL" }
    }
    $rows += [pscustomobject]$row
    $line = "{0,-40} walk={1} topics={2} viewer={3} close={4} fixme={5} err={6} {7}" -f $row.file, $row.walk, $row.topics, $row.viewer, $row.close, $row.fixme, $row.err, $row.dialog
    Write-Host $line
}
$rows | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 "$Out\gate.json"
$rows | Export-Csv -NoTypeInformation -Encoding utf8 "$Out\gate.csv"
$n = $rows.Count
$wp = @($rows | Where-Object { $_.walk -eq "PASS" }).Count
$vp = @($rows | Where-Object { $_.viewer -eq "PASS" }).Count
Write-Host ("GATE: {0} WinHelp files; walk PASS {1}/{0}; viewer PASS {2}/{0}" -f $n, $wp, $vp)
