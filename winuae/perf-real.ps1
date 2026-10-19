# The same performance run as perf-run.ps1, on the REAL Amiga through
# AmiAgent (default C:/amiga-dev/amiagent.cfg). The game lives in WorkSD:AmiXcom
# (tools\make-pkg.ps1 + put + lha x). These are the numbers that count.
#   .\perf-real.ps1 -Scene battle -Res 320 [-Binary openxcom-rtg] [-Tag r1]
param(
    [Parameter(Mandatory=$true)][ValidateSet('battle','geo','geoq','scroll')][string]$Scene,
    [Parameter(Mandatory=$true)][ValidateSet('320','640','n640','mixed')][string]$Res,
    [string]$Binary = 'openxcom-rtg',
    [string]$Tag = '',
    [string]$Dir = 'WorkSD:AmiXcom',
    [int]$ReadySec = 150,       # load time allowed before the scene script is sent
    [int]$TimeoutSec = 480,     # scene + quit
    [switch]$KeepOptions,       # leave the test options in place afterwards
    [int]$FrameSkip = -1,       # amigaFrameSkip 0/1/2; -1 = leave options.cfg alone
    [string]$QuickSave = 'geoscape5sec.sav'   # geoq: the save F9 loads
)

$ufo    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$outdir = "$ufo\winuae\perf-real"
$tmp    = "$outdir\tmp"
$remote = @("$ufo\tools\amiga_remote.py")
New-Item -ItemType Directory -Force $tmp | Out-Null
. "$ufo\winuae\perf-scenes.ps1"

function Lf([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n")) }
function Put([string]$name, [string]$text) {
    Lf "$tmp\$name" $text
    python @remote put "$tmp\$name" "$Dir/$name" | Out-Null
}
function Get-Log() {
    Remove-Item "$tmp\oxc.log" -ErrorAction SilentlyContinue
    python @remote get "$Dir/oxc.log" "$tmp\oxc.log" 2>&1 | Out-Null
    if (Test-Path "$tmp\oxc.log") { return [IO.File]::ReadAllText("$tmp\oxc.log") } else { return "" }
}

$o = python @remote ping 2>&1
if ("$o" -notmatch '^OK') { "FAIL: AmiAgent did not answer: $o"; exit 1 }
$t = python @remote titles 2>&1
if ("$t" -match 'AmiXcom 68K') { "FAIL: AmiXcom is already running on the Amiga"; exit 1 }

# 1. options for this run (from the copy on the Amiga, so the player's other settings stay)
python @remote get "$Dir/user/options.cfg" "$tmp\options.cfg" | Out-Null
Copy-Item "$tmp\options.cfg" "$tmp\options-player.cfg" -Force   # put back after the run
$opt = [IO.File]::ReadAllText("$tmp\options.cfg")
$opt = Set-Options $opt ((Get-ModeOptions $Res) + @(,@('amigaPerfLog', 1)) + @(,@('amigaMusic', 0)) +
                         @(,@('amigaAutoBattle', $(if ($Scene -ne 'geo') { 1 } else { 0 }))))
if ($FrameSkip -ge 0) { $opt = Set-Options $opt @(,@('amigaFrameSkip', $FrameSkip)) }
Lf "$tmp\options.cfg" $opt
python @remote put "$tmp\options.cfg" "$Dir/user/options.cfg" | Out-Null
if ($Scene -eq 'geoq') {
    # the player's Geoscape save becomes the quick save that F9 loads
    python @remote run "Copy $Dir/user/xcom1/$QuickSave TO $Dir/user/xcom1/_quick_.asav" | Out-Null
}

# 2. start
python @remote run "Delete $Dir/autoinput.txt $Dir/oxc.log $Dir/hang.log QUIET" | Out-Null
Put "autoinput.on" "on`n"
# ">>" (append) and not ">": ">" opens the log with an EXCLUSIVE lock, and
# AmiAgent's GET then cannot read it while the game runs. The official run-rtg
# uses ">>" too. The old log is deleted above, so this still starts empty.
Put "t-run" "CD $Dir`nStack 1000000`n$Binary >>oxc.log`n"
python @remote start "Execute $Dir/t-run" | Out-Null

# 3. On OS 3.2 the running game holds oxc.log and sdlmini.log exclusively
#    (even with ">>"), so nothing can be read until it exits. Give it a fixed
#    time to load instead (first start with the yaml cache build: 79 s to the
#    menu on the Warp), then send the scene ending in "quit" and read the log
#    afterwards. perf_summary.py shows whether every phase got measured.
$deadline = (Get-Date).AddSeconds($ReadySec)
do { Start-Sleep 10; $t = python @remote titles 2>&1 } until ("$t" -notmatch 'AmiXcom 68K' -or (Get-Date) -gt $deadline)
if ("$t" -notmatch 'AmiXcom 68K') { "FAIL: the game window is gone before the scene (crash?)" }
Put "autoinput.txt" ((Get-PerfScript $Scene $Res) + "quit`n")
$deadline = (Get-Date).AddSeconds($TimeoutSec)
do { Start-Sleep 10; $t = python @remote titles 2>&1 } until ("$t" -notmatch 'AmiXcom 68K' -or (Get-Date) -gt $deadline)
Start-Sleep 3
$log = Get-Log
foreach ($m in 'splash: 99% at \d+ ms', 'autobattle: in the battlescape', 'autoinput: script loaded', 'autoinput: quit') {
    $x = [regex]::Match($log, $m); "  log: $(if ($x.Success) { $x.Value } else { "MISSING $m" })"
}
python @remote run "Delete $Dir/autoinput.on $Dir/autoinput.txt QUIET" | Out-Null   # never leave it behind
# The game saved options.cfg on quit, with amigaAutoBattle and the test mode in
# it: put the player's own file back, exactly as it was before the run.
if (-not $KeepOptions) {
    python @remote put "$tmp\options-player.cfg" "$Dir/user/options.cfg" | Out-Null
}
if ($Scene -eq 'geoq') { python @remote run "Delete $Dir/user/xcom1/_quick_.asav QUIET" | Out-Null }

$name = "$Scene-$Res-$Binary$(if ($Tag) { "-$Tag" })"
Copy-Item "$tmp\oxc.log" "$outdir\$name.log"
if ($log -match 'CPU TRAP') { "WARNING: CPU TRAP during the run" }
$still = python @remote titles 2>&1
if ("$still" -match 'AmiXcom 68K') { "WARNING: the game is still running on the Amiga" }
python "$ufo\tools\perf_summary.py" "$outdir\$name.log"
