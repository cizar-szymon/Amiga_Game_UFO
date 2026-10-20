# Comparative performance run in OUR WinUAE (amixcom-060-rtg.uae).
# WinUAE numbers say nothing about the real Warp 060 - use them only to compare
# variants (resolution, binary) measured the same way, one after another.
# perf-real.ps1 runs the same scenes on the real Amiga.
#
#   .\perf-run.ps1 -Scene battle -Res 320 [-Binary openxcom-rtg] [-Tag run1]
#   .\perf-run.ps1 -Scene geo    -Res n640
#
# Scenes and modes: perf-scenes.ps1. tools\perf_summary.py averages the probes.
param(
    [Parameter(Mandatory=$true)][ValidateSet('battle','geo','menu','scroll','geoq','inv','geohold')][string]$Scene,
    [Parameter(Mandatory=$true)][ValidateSet('320','640','n640','mixed')][string]$Res,
    [string]$Binary = 'openxcom-rtg',
    [string]$Tag = '',
    [int]$TimeoutSec = 300,
    [int]$FrameSkip = -1,       # amigaFrameSkip 0/1/2; -1 = leave options.cfg alone
    [string]$Extra = ''         # more options.cfg values: 'amigaVramLayer=false;amigaScrollOwn=false'
)

$ufo    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$work   = "$ufo\work"
$cfg    = "$ufo\winuae\amixcom-060-rtg.uae"
$pidf   = "$ufo\winuae\winuae.pid"
$outdir = "$ufo\winuae\perf"
$remote = @("$ufo\tools\amiga_remote.py", "--config", "C:/amiga-dev/amiagent-winuae.cfg")
New-Item -ItemType Directory -Force $outdir | Out-Null
. "$ufo\winuae\perf-scenes.ps1"

function Lf([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n")) }

# 1. options for this run (the game rewrites options.cfg only on a clean exit;
#    the emulator is killed instead, so nothing leaks into the next run)
$optf = "$work\user\options.cfg"
$opt  = [IO.File]::ReadAllText($optf)
$opt  = Set-Options $opt ((Get-ModeOptions $Res) + @(,@('amigaPerfLog', 1)) + @(,@('amigaMusic', 0)) +
                          @(,@('amigaAutoBattle', $(if ($Scene -in 'battle','scroll','geoq','inv') { 1 } else { 0 }))))
if ($FrameSkip -ge 0) { $opt = Set-Options $opt @(,@('amigaFrameSkip', $FrameSkip)) }
foreach ($kv in ($Extra -split ';' | Where-Object { $_ })) { $k, $v = $kv -split '=', 2; $opt = Set-Options $opt @(,@($k, $v)) }
Lf $optf $opt

# 2. fresh emulator (ours only, by saved PID)
if (Test-Path $pidf) { Stop-Process -Id ([int](Get-Content $pidf)) -ErrorAction SilentlyContinue; Start-Sleep 2 }
Remove-Item "$work\autoinput.txt", "$work\autoinput.on", "$work\oxc.log", "$work\sdlmini.log", "$work\hang.log" -ErrorAction SilentlyContinue
$p = Start-Process "C:\Program Files\WinUAE\winuae64.exe" -ArgumentList '-f', $cfg, '-s', 'use_gui=no' -PassThru
$p.Id | Out-File $pidf -Encoding ascii
$deadline = (Get-Date).AddSeconds(120)
do { Start-Sleep 3; $o = python @remote ping 2>&1 } until ("$o" -match '^OK' -or (Get-Date) -gt $deadline)
if ("$o" -notmatch '^OK') { "FAIL: AmiAgent did not answer"; exit 1 }

# 3. start, wait until the scene is up (in WinUAE the shared folder can be read live)
Lf "$work\t-run" "CD AmiXcom:`nStack 1000000`n$Binary >oxc.log`n"
Lf "$work\autoinput.on" "on`n"
python @remote start "Execute AmiXcom:t-run" | Out-Null
$ready = if ($Scene -in 'battle','scroll','geoq','inv') { 'autobattle: in the battlescape' } else { 'state: resetAll done' }
$deadline = (Get-Date).AddSeconds($TimeoutSec)
do { Start-Sleep 3; $log = Get-Content "$work\oxc.log" -Raw -ErrorAction SilentlyContinue } until ($log -match [regex]::Escape($ready) -or $log -match 'CPU TRAP' -or (Get-Date) -gt $deadline)
$name = "$Scene-$Res-$Binary$(if ($Tag) { "-$Tag" })"
if ($log -notmatch [regex]::Escape($ready)) {
    "FAIL: scene not reached"
    Copy-Item "$work\oxc.log" "$outdir\$name.log" -ErrorAction SilentlyContinue
    & "$ufo\AmiXcom\winuae\harness\capture_ours.ps1" -Out "$outdir\$name.png" -Config 'amixcom-060-rtg.uae' | Out-Null
    exit 1
}

# 4. the scene ('menu' = just look at the main menu for a few seconds)
if ($Scene -eq 'menu') { Lf "$work\autoinput.txt" "wait 11`nwait 8000`nwait 15`nwait 1000`n" }
else { Lf "$work\autoinput.txt" (Get-PerfScript $Scene $Res) }

$deadline = (Get-Date).AddSeconds(240)
do { Start-Sleep 5; $log = Get-Content "$work\oxc.log" -Raw; $tt = python @remote titles 2>&1 } until ($log -match 'autoinput: script done' -or $log -match 'CPU TRAP' -or "$tt" -match 'Software Failure' -or (Get-Date) -gt $deadline)
if ("$tt" -match 'Software Failure') { "CRASH: Software Failure requester" }
Start-Sleep 4
Remove-Item "$work\autoinput.on", "$work\autoinput.txt" -ErrorAction SilentlyContinue   # never leave it behind

Copy-Item "$work\oxc.log" "$outdir\$name.log"
& "$ufo\AmiXcom\winuae\harness\capture_ours.ps1" -Out "$outdir\$name.png" -Config 'amixcom-060-rtg.uae' | Out-Null
if ($log -match 'CPU TRAP') { "WARNING: CPU TRAP during the run" }
python "$ufo\tools\perf_summary.py" "$outdir\$name.log"
