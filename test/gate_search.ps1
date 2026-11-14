# gate_search.ps1 - phase 2a gap 1 gate: the WinHelp 3.1 Search dialog,
# driven with REAL input (keybd_event / mouse_event) on monitor 2, at the
# real window size (679x596). Expected values come from the real
# WINHELP.EXE 3.10 in the WfW 3.11 VM (D:\winhelp_corpus\p2a\real_s*.png,
# real_g_item.png).
param([string]$Out = "D:\winhelp_corpus\p2a\gate")
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$probe = "$PSScriptRoot\search_probe.ps1"
$pass = 0; $fail = 0
function Check($name, $cond) { if ($cond) { $script:pass++; "  PASS $name" } else { $script:fail++; "  FAIL $name" } }

"TTTEST.HLP, 'telnet' (the deliberate duplicate keyword):"
$t = (& $probe -File "D:\winhelp_build\TTTEST.HLP" -Word telnet -Out $Out -Name tt) -join "`n"
Check "activation (click-check-retry)"        ($t -match 'activate tries: \d')
Check "opens: empty edit, first keyword, buttons off" ($t -match '0open: edit=\[\] kwsel=0\[connect\] top=0 show=False goto=False')
Check "type-ahead 'te' -> Teletext, top 1, Show Topics on" ($t -match '1prefix: edit=\[te\] kwsel=6\[Teletext\] top=1 show=True goto=False def=0x164')
Check "'telnet' -> Telnet, top 2"             ($t -match '2word: edit=\[telnet\] kwsel=7\[Telnet\] top=2 show=True')
Check "Show Topics: keyword spelling, Close, Go To default" ($t -match '3show: edit=\[Telnet\].*goto=True def=0x167 cancel=\[Close\]')
Check "two topics, sorted: Connecting|Settings" ($t -match 'topics=\[Connecting\|Settings\] topicsel=0')
Check "Down selects Settings"                 ($t -match '4down: .*topicsel=1')
Check "dialog at +36,+46, 399x331 outside (real pixel scan 2026-11-13: client 389x302)" ($t -match 'dialog at \+36,\+46 size 399x331')
Check "Go To closes the dialog"               ($t -match 'dialog gone=True')
Check "Go To opened Settings"                 ($t -match 'topic text: Settings')
Check "monitor 2 guard"                       ($t -notmatch 'MONITOR FAIL')

"GOPHER.HLP (Chameleon), 'item' (13 topics):"
$g = (& $probe -File "D:\winhelp_corpus\wfw311_tree\NETMANAG\GOPHER.HLP" -Word item -Out $Out -Name gopher) -join "`n"
Check "'item' -> Item"                        ($g -match '2word: edit=\[item\] kwsel=5\[Item\]')
Check "Show Topics scrolls Item to the top"   ($g -match '3show: edit=\[Item\] kwsel=5\[Item\] top=5')
Check "13 topics in the real order" ($g -match 'topics=\[Add Gopher Server\|Close Item\|Copy To File\|Go Back\|Item\|Item properties\|Move To File\|Open Item\|Remove Item\|Retreive Item\|Retrieve Info\|Search\|Set Bookmark\]')
Check "Go To opened Close Item"               ($g -match 'topic text: Close Item')
Check "monitor 2 guard"                       ($g -notmatch 'MONITOR FAIL')
"GATE SEARCH: $pass PASS, $fail FAIL"
