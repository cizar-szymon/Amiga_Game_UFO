# Repro: New Game -> Beginner -> OK -> clicks on the globe (base placement),
# with a chosen binary. Restarts OUR WinUAE (by saved PID), never others.
#   .\test-baseplace.ps1 -Binary openxcom-rtg
#   .\test-baseplace.ps1 -Binary openxcom-rtg-rel     (official 0.9.14)
param([Parameter(Mandatory=$true)][string]$Binary, [int]$TimeoutSec = 240)

$ufo    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$work   = "$ufo\work"
$cfg    = "$ufo\winuae\amixcom-060-rtg.uae"
$pidf   = "$ufo\winuae\winuae.pid"
$remote = @("$ufo\tools\amiga_remote.py", "--config", "C:/amiga-dev/amiagent-winuae.cfg")

function Lf([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n")) }

# 1. fresh emulator
if (Test-Path $pidf) { Stop-Process -Id ([int](Get-Content $pidf)) -ErrorAction SilentlyContinue; Start-Sleep 2 }
Remove-Item "$work\autoinput.txt", "$work\autoinput.on", "$work\oxc.log", "$work\sdlmini.log", "$work\hang.log" -ErrorAction SilentlyContinue
$p = Start-Process "C:\Program Files\WinUAE\winuae64.exe" -ArgumentList '-f', $cfg, '-s', 'use_gui=no' -PassThru
$p.Id | Out-File $pidf -Encoding ascii
$deadline = (Get-Date).AddSeconds(120)
do { Start-Sleep 3; $o = python @remote ping 2>&1 } until ("$o" -match '^OK' -or (Get-Date) -gt $deadline)
if ("$o" -notmatch '^OK') { "FAIL: AmiAgent did not answer"; exit 1 }

# 2. start the game, wait for the main menu
Lf "$work\t-run" "CD AmiXcom:`nStack 1000000`n$Binary >oxc.log`n"
Lf "$work\autoinput.on" "on`n"
python @remote start "Execute AmiXcom:t-run" | Out-Null
$deadline = (Get-Date).AddSeconds($TimeoutSec)
do { Start-Sleep 3; $log = Get-Content "$work\oxc.log" -Raw -ErrorAction SilentlyContinue } until ($log -match 'state: resetAll done' -or (Get-Date) -gt $deadline)
if ($log -notmatch 'state: resetAll done') { "FAIL: no main menu"; exit 1 }

# 3. the clicks. Autoinput takes SCREEN (SDL) coordinates: in the 640x480 mode
# that is game*2 + 40 lines of band (menu positions from AmiXcom LEFTOFF.md,
# 320x200). 463 72 is the user's own click that trapped on 2026-10-19.
Lf "$work\autoinput.txt" @"
wait 2000
click 200 240
wait 3000
click 220 124
wait 1500
click 236 384
wait 10000
click 463 72
wait 5000
click 256 240
wait 5000
click 194 242
wait 5000
"@
$deadline = (Get-Date).AddSeconds(90)
do { Start-Sleep 3; $log = Get-Content "$work\oxc.log" -Raw } until ($log -match 'CPU TRAP' -or ((-not (Test-Path "$work\autoinput.txt")) -and $log -match 'autoinput: wait 5000[\s\S]*autoinput: wait 5000[\s\S]*autoinput: wait 5000') -or (Get-Date) -gt $deadline)
Start-Sleep 6
$log = Get-Content "$work\oxc.log" -Raw
Remove-Item "$work\autoinput.on", "$work\autoinput.txt" -ErrorAction SilentlyContinue   # never leave it behind
"=== $Binary"
($log -split "`n") | Select-String 'autoinput:|state: |CPU TRAP|Build|base' | Select -Last 30 | % { $_.Line }
if ($log -match 'CPU TRAP') { "RESULT: TRAP" } else { "RESULT: no trap" }
& "$ufo\AmiXcom\winuae\harness\capture_ours.ps1" -Out "$ufo\winuae\last-$Binary.png" -Config 'amixcom-060-rtg.uae'
