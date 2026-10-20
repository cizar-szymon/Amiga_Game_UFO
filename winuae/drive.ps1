# Drive the game on a real Amiga step by step: autoinput scripts sent one at
# a time (the game polls PROGDIR:autoinput.txt every 0.5 s while idle and
# deletes it when the script is done), a screenshot after each.
#   . .\drive.ps1 -Config C:/amiga-dev/amiagent-2.cfg
#   Start-Game openxcom-rtg-pool ; Send-Steps "key f9`nwait 20000" ; Shot geo1
param([string]$Config = '', [string]$Dir = 'WorkSD:AmiXcom')
$ufo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$script:remote = @("$ufo\tools\amiga_remote.py")
if ($Config) { $script:remote += @('--config', $Config) }
$script:tmp = "$ufo\winuae\perf-real\tmp"
$script:dir = $Dir
$script:shots = "$ufo\winuae\perf-real\shots"
New-Item -ItemType Directory -Force $script:shots | Out-Null

function Lf([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text.Replace("`r`n", "`n")) }

function Start-Game([string]$Binary) {
    Lf "$script:tmp\autoinput.on" "on`n"
    python @script:remote put "$script:tmp\autoinput.on" "$script:dir/autoinput.on" | Out-Null
    python @script:remote run "Delete $script:dir/autoinput.txt $script:dir/oxc.log QUIET" | Out-Null
    Lf "$script:tmp\t-run" "CD $script:dir`nStack 1000000`n$Binary >>oxc.log`n"
    python @script:remote put "$script:tmp\t-run" "$script:dir/t-run" | Out-Null
    python @script:remote start "Execute $script:dir/t-run" | Out-Null
    $deadline = (Get-Date).AddSeconds(240)
    do { Start-Sleep 5; $t = python @script:remote titles 2>&1 } until ("$t" -match 'AmiXcom 68K' -or (Get-Date) -gt $deadline)
    if ("$t" -match 'AmiXcom 68K') { "game window open" } else { "NO WINDOW" }
}

# send a script and wait until the game has run it (file gone)
function Send-Steps([string]$Text, [int]$TimeoutSec = 300) {
    Lf "$script:tmp\autoinput.txt" ($Text.Trim() + "`n")
    python @script:remote put "$script:tmp\autoinput.txt" "$script:dir/autoinput.txt" | Out-Null
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    do {
        Start-Sleep 3
        $o = python @script:remote run "List $script:dir/autoinput.txt" 2>&1
    } until ("$o" -match 'not found|No information|object not found|rc 20' -or (Get-Date) -gt $deadline)
    if ((Get-Date) -gt $deadline) { "TIMEOUT: script still pending" } else { "script done" }
}

function Shot([string]$Name) {
    python @script:remote shot "$script:shots\$Name.png" | Out-Null
    "$script:shots\$Name.png"
}

# The mission start from the player's save: menu > Load (row $Row) > spin
# and zoom the globe > Begin Mission > briefing > equip screen OK > turn 1,
# then quit. Returns the log lines that tell what happened.
function Run-Mission([string]$Binary, [int]$Row = 62, [int]$LoadSec = 120, [int]$OkX = 414, [int]$OkY = 112, [int]$Spin = 3) {
    Start-Game $Binary | Out-Null
    Start-Sleep $LoadSec
    Send-Steps "click 110 128`nwait 6000`nclick 60 $Row`nwait 40000" 300 | Out-Null
    $s = New-Object System.Collections.Generic.List[string]
    for ($r = 0; $r -lt $Spin; $r++) {
        for ($i = 0; $i -lt 25; $i++) { $s.Add("key left"); $s.Add("wait 130") }
        for ($i = 0; $i -lt 10; $i++) { $s.Add("key up"); $s.Add("wait 130") }
        foreach ($k in '+', '+', '-', '-') { $s.Add("key $k"); $s.Add("wait 1500") }
    }
    $s.Add("wait 20000")
    Send-Steps ($s -join "`n") 400 | Out-Null
    Shot "$Binary-prompt" | Out-Null
    Send-Steps "click 80 160`nwait 6000`nclick 160 172`nwait 60000" 400 | Out-Null
    Shot "$Binary-equip" | Out-Null
    $r1 = Send-Steps "click $OkX $OkY`nwait 20000`nclick 320 200`nwait 40000" 300
    Shot "$Binary-battle" | Out-Null
    $r2 = Send-Steps "quit" 180
    Start-Sleep 20
    $t = python @script:remote titles 2>&1
    Remove-Item "$script:tmp\mission-oxc.log" -ErrorAction SilentlyContinue
    python @script:remote get "$script:dir/oxc.log" "$script:tmp\mission-oxc.log" 2>&1 | Out-Null
    "turn 1: $r1, quit: $r2, titles: $t"
    if (Test-Path "$script:tmp\mission-oxc.log") {
        Select-String "$script:tmp\mission-oxc.log" -Pattern 'heapguard: (WRITE|DAMAGED|OVERRUN|DOUBLE|FREE OF)|heapguard: frame|save: build|main: bye|CPU TRAP' | Select -Last 12 | % Line
    } else { "oxc.log unreadable (the game is still holding it)" }
}

function Stop-Drive() {
    python @script:remote run "Delete $script:dir/autoinput.on $script:dir/autoinput.txt QUIET" | Out-Null
}
