# Shared by perf-run.ps1 (WinUAE) and perf-real.ps1 (real Amiga): the
# autoinput script of one scene. Phases are marked by odd waits (11..15) that
# tools\perf_summary.py looks for. Coordinates are game pixels (320x200),
# converted to the screen of the chosen mode.
#   320  - 320x200, the original
#   640  - the port's 640x480 mode: the 320x200 picture scaled 2x, 40-line bands
#   n640 - native 640x400 (geoscapeScale/battlescapeScale = SCALE_SCREEN):
#          320x200 dialogs are centred, so their buttons move by +160,+100
#   mixed - amigaNativeScale: Geoscape on a 320x200 screen, battles on a
#          native 640x400 one (the screen switches between them)
function Get-PerfScript([string]$Scene, [string]$Res) {
    if ($Res -eq 'mixed') { $Res = if ($Scene -eq 'geo') { '320' } else { 'n640' } }
    function P([int]$x, [int]$y) {
        if ($Res -eq '640') { "$($x * 2) $($y * 2 + 40)" }
        elseif ($Res -eq 'n640') { "$($x + 160) $($y + 100)" }
        else { "$x $y" }
    }
    $s = New-Object System.Collections.Generic.List[string]
    if ($Scene -eq 'battle') {
        $s.Add("wait 5000"); $s.Add("click $(P 160 100)"); $s.Add("wait 5000")     # dismiss "TURN 1 - press button"
        # Centre on a soldier (keyBattleNextUnit = Tab). Without it the camera
        # looks at undiscovered (black) tiles and the map costs almost nothing.
        $s.Add("key tab"); $s.Add("wait 3000")
        $s.Add("move $(P 160 70)"); $s.Add("wait 11"); $s.Add("wait 20000")           # phase 11: idle
        $s.Add("wait 12")                                                            # phase 12: cursor moves
        for ($i = 0; $i -lt 120; $i++) {
            $x = 40 + ($i * 37) % 240; $y = 20 + ($i * 23) % 110
            $s.Add("move $(P $x $y)"); $s.Add("wait 120")
        }
        # Edge scrolling does not react to injected pointer moves (cache: scroll 0),
        # so scroll with the arrow keys (keyBattleLeft/Right...), one step per press.
        $s.Add("move $(P 160 70)")
        $s.Add("wait 13")                                                            # phase 13: scroll right/down
        for ($i = 0; $i -lt 40; $i++) { $s.Add("key right"); $s.Add("wait 150"); $s.Add("key down"); $s.Add("wait 150") }
        $s.Add("wait 14")                                                            # phase 14: scroll left/up
        for ($i = 0; $i -lt 40; $i++) { $s.Add("key left"); $s.Add("wait 150"); $s.Add("key up"); $s.Add("wait 150") }
        $s.Add("move $(P 160 70)"); $s.Add("wait 15"); $s.Add("wait 3000")
    } elseif ($Scene -eq 'scroll') {
        # continuous scrolling, as with the mouse at the screen edge: key after
        # key with no gap - autoinput holds each one ~120 ms, the map scrolls
        # while it is held, so it moves every frame
        $s.Add("wait 5000"); $s.Add("click $(P 160 100)"); $s.Add("wait 5000")     # dismiss "TURN 1"
        $s.Add("key tab"); $s.Add("wait 3000"); $s.Add("move $(P 160 70)")
        # Edge scrolling starts within 5 px of the screen edge (Camera::
        # SCROLL_BORDER) - raw screen coordinates, not the dialog offset of P()
        $sw = if ($Res -eq 'n640') { 640 } else { 320 }
        $sh = if ($Res -eq 'n640') { 400 } else { 200 }
        # back and forth, 1.5 s each way: the camera stops at the map's edge,
        # so one long push would scroll for a moment and then stand still
        $ym = [int]($sh * 0.35)
        $s.Add("wait 11"); $s.Add("wait 8000")                                       # phase 11: idle reference
        $s.Add("wait 13")                                                            # phase 13: left-right
        for ($i = 0; $i -lt 6; $i++) { $s.Add("move $($sw - 1) $ym"); $s.Add("wait 1500"); $s.Add("move 0 $ym"); $s.Add("wait 1500") }
        $s.Add("move $([int]($sw / 2)) $ym")
        $s.Add("wait 14")                                                            # phase 14: up-down (top edge / just above the icons)
        for ($i = 0; $i -lt 6; $i++) { $s.Add("move $([int]($sw / 2)) 0"); $s.Add("wait 1500"); $s.Add("move $([int]($sw / 2)) $([int]($sh * 0.7))"); $s.Add("wait 1500") }
        $s.Add("move $([int]($sw / 2)) $ym")
        $s.Add("wait 15"); $s.Add("wait 2000")
    } elseif ($Scene -eq 'geoq') {
        # A real Geoscape, time running: the player's save, copied to
        # user/<ruleset>/_quick_.asav by the caller, quick-loaded with F9 from
        # the auto battle (autoinput cannot pick rows of the Load list).
        $s.Add("wait 5000"); $s.Add("click $(P 160 100)"); $s.Add("wait 4000")     # dismiss "TURN 1"
        $s.Add("key f9"); $s.Add("wait 30000")                                     # quick load -> Geoscape
        $s.Add("move $(P 300 190)")
        $s.Add("wait 11"); $s.Add("wait 20000")                                      # phase 11: time running, no input
        $s.Add("wait 12")                                                            # phase 12: rotate
        for ($i = 0; $i -lt 80; $i++) { $s.Add("key left"); $s.Add("wait 130") }
        for ($i = 0; $i -lt 40; $i++) { $s.Add("key up");   $s.Add("wait 130") }
        $s.Add("wait 13")                                                            # phase 13: zoom in/out
        for ($i = 0; $i -lt 4; $i++) { $s.Add("key +"); $s.Add("wait 2500") }
        for ($i = 0; $i -lt 4; $i++) { $s.Add("key -"); $s.Add("wait 2500") }
        $s.Add("wait 15"); $s.Add("wait 3000")
    } else {
        $s.Add("wait 2000"); $s.Add("click $(P 100 100)"); $s.Add("wait 3000")     # New Game
        $s.Add("click $(P 110 42)"); $s.Add("wait 1500")                           # Beginner
        $s.Add("click $(P 118 172)"); $s.Add("wait 12000")                         # OK -> select base site
        $s.Add("move $(P 160 190)")
        $s.Add("wait 11"); $s.Add("wait 20000")                                      # phase 11: globe idle
        $s.Add("wait 12")                                                            # phase 12: rotate
        for ($i = 0; $i -lt 80; $i++) { $s.Add("key left"); $s.Add("wait 130") }
        for ($i = 0; $i -lt 40; $i++) { $s.Add("key up");   $s.Add("wait 130") }
        $s.Add("wait 13")                                                            # phase 13: zoom in/out
        for ($i = 0; $i -lt 4; $i++) { $s.Add("key +"); $s.Add("wait 2500") }
        for ($i = 0; $i -lt 4; $i++) { $s.Add("key -"); $s.Add("wait 2500") }
        $s.Add("wait 15"); $s.Add("wait 3000")
    }
    return (($s -join "`n") + "`n")
}

# options.cfg values for a mode: displayWidth, displayHeight, geoscapeScale, battlescapeScale
function Get-ModeOptions([string]$Res) {
    switch ($Res) {
        '640'   { return @(@('displayWidth', 640), @('displayHeight', 480), @('geoscapeScale', 0), @('battlescapeScale', 0)) }
        'n640'  { return @(@('displayWidth', 640), @('displayHeight', 400), @('geoscapeScale', 5), @('battlescapeScale', 5),
                           @('nonSquarePixelRatio', 'false')) }
        'mixed' { return @(@('displayWidth', 640), @('displayHeight', 400), @('geoscapeScale', 0), @('battlescapeScale', 2),
                           @('nonSquarePixelRatio', 'false'), @('amigaNativeScale', 'true')) }
        default { return @(@('displayWidth', 320), @('displayHeight', 200), @('geoscapeScale', 0), @('battlescapeScale', 0)) }
    }
}

function Set-Options([string]$text, $pairs) {
    foreach ($kv in $pairs) {
        $re = "(?m)^(\s*$($kv[0])\s*:\s*).*$"
        if ($text -match $re) {
            $text = [regex]::Replace($text, $re, "`${1}$($kv[1])")
        } elseif ($text -match "(?m)^(\s*)amigaPerfLog\s*:.*$") {
            # an option newer than this options.cfg: add it next to amigaPerfLog
            $line = $Matches[0]; $ind = $Matches[1]
            $text = $text.Replace($line, "$line`n$ind$($kv[0]): $($kv[1])")
        } else {
            throw "option $($kv[0]) not in options.cfg"
        }
    }
    return $text
}
