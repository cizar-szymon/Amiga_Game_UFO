# Does a binary leave the memory list damaged? Starts the game on the real
# Amiga in the auto battle (no scene script), lets it run, ends it with
# Ctrl-C like AmiAgent's stop, and looks for exec's "Software Failure"
# requester (81000005 corrupt memory list in FreeMem, 2026-10-20). The
# player's options.cfg is put back afterwards.
#   .\crash-test.ps1 -Binary openxcom-rtg-gl [-Extra 'amigaVramLayer=false'] [-RunSec 90]
param(
    [Parameter(Mandatory=$true)][string]$Binary,
    [string]$Extra = '',
    [int]$RunSec = 90,           # in the battle, after the window opened and loading ended
    [string]$Dir = 'WorkSD:AmiXcom',
    [string]$Config = ''         # AmiAgent config of another Amiga
)
$ufo    = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$tmp    = "$ufo\winuae\perf-real\tmp"
$remote = @("$ufo\tools\amiga_remote.py")
if ($Config) { $remote += @('--config', $Config) }
. "$ufo\winuae\perf-scenes.ps1"
function Lf([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n")) }

$t = python @remote titles 2>&1
if ("$t" -match 'AmiXcom 68K|Software Failure') { "FAIL: the Amiga is not idle: $t"; exit 1 }

python @remote get "$Dir/user/options.cfg" "$tmp\options.cfg" | Out-Null
Copy-Item "$tmp\options.cfg" "$tmp\options-player.cfg" -Force
$opt = [IO.File]::ReadAllText("$tmp\options.cfg")
$opt = Set-Options $opt ((Get-ModeOptions 'mixed') + @(,@('amigaPerfLog', 1)) + @(,@('amigaMusic', 0)) + @(,@('amigaAutoBattle', 1)))
foreach ($kv in ($Extra -split ';' | Where-Object { $_ })) { $k, $v = $kv -split '=', 2; $opt = Set-Options $opt @(,@($k, $v)) }
Lf "$tmp\options.cfg" $opt
python @remote put "$tmp\options.cfg" "$Dir/user/options.cfg" | Out-Null
python @remote run "Delete $Dir/autoinput.txt $Dir/autoinput.on $Dir/oxc.log QUIET" | Out-Null
Lf "$tmp\t-run" "CD $Dir`nStack 1000000`n$Binary >>oxc.log`n"
python @remote put "$tmp\t-run" "$Dir/t-run" | Out-Null
python @remote start "Execute $Dir/t-run" | Out-Null

$deadline = (Get-Date).AddSeconds(240)
do { Start-Sleep 5; $t = python @remote titles 2>&1 } until ("$t" -match 'AmiXcom 68K' -or (Get-Date) -gt $deadline)
if ("$t" -notmatch 'AmiXcom 68K') { "FAIL: no game window" }
Start-Sleep 60          # loading into the auto battle
Start-Sleep $RunSec
$t = python @remote titles 2>&1
$crashedEarly = "$t" -match 'Software Failure'
python @remote stop $Binary | Out-Null
Start-Sleep 25
$t = python @remote titles 2>&1
$result = if ("$t" -match 'Software Failure') { "CRASH (Software Failure$(if ($crashedEarly) { ', already before Ctrl-C' }))" }
          elseif ("$t" -match 'AmiXcom 68K') { "STILL RUNNING after Ctrl-C" } else { "clean exit" }
if ($result -match 'CRASH') { python @remote shot "$ufo\winuae\perf-real\crash-$Binary.png" | Out-Null }
python @remote put "$tmp\options-player.cfg" "$Dir/user/options.cfg" | Out-Null
"$Binary [$Extra]: $result"
