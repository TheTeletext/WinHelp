# gate_nsr.ps1 - phase 2a gap 2 gate: the non-scrolling region (\keepn band),
# real input on monitor 2. Reference: the real WINHELP.EXE 3.10 in the VM
# (TTTEST band 36 px + 1 px black rule under the button bar; GLOSHLP A..Z
# strip in the glossary band, a letter jumps inside the glossary topic).
param([string]$Out = "D:\winhelp_corpus\p2a\gate")
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$pass = 0; $fail = 0
function Check($name, $cond) { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name" } }
$t = (& "$PSScriptRoot\nsr_probe.ps1" -Out $Out -Name nsr 2>&1) -join "`n"
Check "activation (click-check-retry)"           ($t -match 'activate tries: \d')
Check "TTTEST band shows the \keepn title"        ($t -match 'band0: vis=True text=\[Teletext Test Help\]')
Check "band 36 px, as the real one"               ($t -match 'band0: .* h=36 ')
Check "1 px rule directly under the band"         ($t -match 'rule=1px at band\+36 text at rule\+1')
Check "4x Down scrolls the text"                  ($t -match 'scrolled=True')
Check "the band does not move or change"          ($t -match 'band_fixed=True')
Check ">> browses: band follows the topic"        ($t -match 'band2 \(after >>\): text=\[Connecting\]')
Check "GOPHER contents band"                      ($t -match 'gopher band: \[Gopher Contents\]')
Check "Glossary window has the band (A..Z strip)" ($t -match 'glossary: .* band vis=True')
Check "real click on M in the band jumps to M"    ($t -match 'after click M: first=([1-9]\d*)')
Check "monitor 2 guard"                           ($t -notmatch 'MONITOR FAIL|refusing')
"GATE NSR: $pass PASS, $fail FAIL"
