# build_hlp.ps1 - help source -> .HLP: hlpsrc.py (RTF + HPJ), then the
# Microsoft Help Compiler 3.10.505 headless (test\hc_headless.ps1).
# HC.EXE and HC.ERR are NOT in this repository (Microsoft licence): they must
# be in $HcDir. The finished .HLP is copied to $OutDir.
#   build_hlp.ps1 -Base TTWINHLP -Src help\src\ttwinhlp.txt -OutDir help
param([Parameter(Mandatory=$true)][string]$Base, [Parameter(Mandatory=$true)][string[]]$Src,
      [Parameter(Mandatory=$true)][string]$OutDir, [string]$HcDir = "D:\winhelp_build")
$ErrorActionPreference = "Stop"
$work = Join-Path $HcDir ($Base.ToLower())
New-Item -ItemType Directory -Force -Path $work | Out-Null
Copy-Item "$HcDir\HC.EXE", "$HcDir\HC.ERR" $work -Force
Get-ChildItem $work -File | Where-Object { $_.Extension -in ".RTF", ".HPJ", ".HLP", ".LOG", ".BMP", ".TXT" } | Remove-Item -Force
$srcs = @($Src | ForEach-Object { (Resolve-Path $_).Path })
& python "$PSScriptRoot\hlpsrc.py" $work $Base @srcs
if ($LASTEXITCODE) { "SOURCE ERRORS: nothing compiled"; exit 1 }
foreach ($d in ($srcs | ForEach-Object { Split-Path $_ } | Sort-Object -Unique)) {
    Get-ChildItem "$d\*.bmp" -ErrorAction SilentlyContinue | Copy-Item -Destination $work
}
$hc = & "$PSScriptRoot\..\test\hc_headless.ps1" -Project "$Base.HPJ" -Build $work -TimeoutS 300 2>$null
($hc | Select-Object -First 1)
$log = Get-Content "$work\$Base.LOG" -Raw
$log.Trim()
if (-not (Test-Path "$work\$Base.HLP")) { "HC FAILED: no $Base.HLP"; exit 1 }
if ($log -match '(?i)\berror\b|\bwarning\b') { "HC reported errors or warnings"; exit 1 }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item "$work\$Base.HLP" (Join-Path $OutDir "$Base.HLP") -Force
$f = Get-Item (Join-Path $OutDir "$Base.HLP")
"{0} {1:N0} B sha256 {2}" -f $f.Name, $f.Length, (Get-FileHash $f.FullName -Algorithm SHA256).Hash.ToLower()
