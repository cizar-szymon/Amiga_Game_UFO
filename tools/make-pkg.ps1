# Pack ufo\work into one LHA for the real Amiga (AmiAgent sends one file
# instead of ~1200). Top-level folder in the archive: AmiXcom/.
#   .\make-pkg.ps1 -Out <dir>          -> <dir>\AmiXcom-pkg.lha
param([Parameter(Mandatory=$true)][string]$Out, [switch]$NoData)

$ufo   = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$work  = "$ufo\work"
$stage = "$Out\stage\AmiXcom"
Remove-Item "$Out\stage", "$Out\AmiXcom-pkg.lha" -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$stage\user" | Out-Null

foreach ($f in 'openxcom-rtg', 'openxcom-rtg-fpu', 'AmiXcomPrefs', 'openxcom-rtg.info', 'openxcom-rtg-fpu.info',
               'AmiXcomPrefs.info', 'run-rtg', 'run-rtg.info', 'run-rtg-fpu', 'run-rtg-fpu.info') {
    Copy-Item "$work\$f" $stage
}
if (-not $NoData) {
    # .ybc = yaml cache written by the game next to its sources; never ship it
    robocopy "$work\data" "$stage\data" /E /XF *.ybc /NFL /NDL /NJH /NJS /NP | Out-Null
}

# options for measurements: native 320x200, no music, perf probes on
$opt = [IO.File]::ReadAllText("$work\user\options.cfg")
foreach ($kv in @(@('displayWidth', 320), @('displayHeight', 200), @('amigaMusic', 0),
                  @('amigaPerfLog', 1), @('amigaAutoBattle', 0))) {
    $opt = [regex]::Replace($opt, "(?m)^(\s*$($kv[0])\s*:\s*).*$", "`${1}$($kv[1])")
}
[IO.File]::WriteAllText("$stage\user\options.cfg", $opt.Replace("`r`n", "`n"))

$o = (Resolve-Path $Out).Path
docker run --rm -v "${o}:/o" amixcom-build sh -c "cd /o/stage && lha aq /o/AmiXcom-pkg.lha AmiXcom" 2>&1 | % { "$_" }
Get-Item "$Out\AmiXcom-pkg.lha" | Select Name, Length
