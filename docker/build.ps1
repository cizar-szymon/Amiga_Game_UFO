# Build AmiXcom in Docker without touching AmiXcom/build/build.sh.
# The container sees the repo and the deploy folder at the author's WSL paths.
#   .\build.ps1            # aga / rtg / ask (soft float)
#   .\build.ps1 -Fpu       # the -fpu variants (AMIGA_NO_PATCH=1 is set too: see LEFTOFF.md)
#   .\build.ps1 -Clean     # re-unpack upstream first
#   .\build.ps1 -Tune060   # -mtune=68060 variant (binaries *-t060, AMIGA_NO_PATCH=1)
param([switch]$Fpu, [switch]$Clean, [switch]$NoPatch, [switch]$Rebuild, [switch]$Tune060, [switch]$HeapGuard)

# Not 'Stop': PowerShell 5.1 turns docker's progress output on stderr into a
# terminating error. Native exit codes are checked by hand instead.
$ErrorActionPreference = 'Continue'
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo   = (Resolve-Path "$here\..\AmiXcom").Path
$deploy = "$here\..\work"            # Work: for WinUAE (outside the repo)
New-Item -ItemType Directory -Force $deploy | Out-Null
$deploy = (Resolve-Path $deploy).Path
$image  = 'amixcom-build'

# The build number the game shows under the FPS counter: the commit count of
# the tree, plus the short hash (with "+" when it has uncommitted changes) in
# sdlmini.log. Written into native/ (ignored by git) for the compiler.
$bnum  = (git -C $repo rev-list --count HEAD).Trim()
$bhash = (git -C $repo rev-parse --short HEAD).Trim()
if (git -C $repo status --porcelain --untracked-files=no) { $bhash += '+' }
$bid = "#define AMIGA_BUILD_NUM $bnum`n#define AMIGA_BUILD_ID `"$bnum $bhash`"`n"
[IO.File]::WriteAllText("$repo\native\amiga_buildid.h", $bid)
"build number $bnum ($bhash)"

if ($Rebuild -or -not (docker images -q $image)) {
    docker build -t $image $here
    if ($LASTEXITCODE) { throw "docker build failed" }
}

$envArgs = @('-e', 'HOME=/b')
if ($Fpu)               { $envArgs += @('-e', 'AMIGA_FPU=1', '-e', 'AMIGA_NO_PATCH=1') }
elseif ($NoPatch)       { $envArgs += @('-e', 'AMIGA_NO_PATCH=1') }
if ($Tune060)           { $envArgs += @('-e', 'AMIGA_TUNE060=1'); if (-not $Fpu -and -not $NoPatch) { $envArgs += @('-e', 'AMIGA_NO_PATCH=1') } }
if ($HeapGuard)         { $envArgs += @('-e', 'AMIGA_HEAPGUARD=1'); if (-not $Fpu -and -not $NoPatch) { $envArgs += @('-e', 'AMIGA_NO_PATCH=1') } }
$shArgs = @('/mnt/i/GITHUB/Amiga_OpenXCOM/build/build.sh')
if ($Clean) { $shArgs += 'clean' }

# amixcom-build volume = $HOME/build (objects, patched tree) on the Linux
# filesystem: compiling from a Windows bind mount is several times slower.
docker run --rm `
    -v "${repo}:/mnt/i/GITHUB/Amiga_OpenXCOM" `
    -v "${deploy}:/mnt/c/temp/amiga_oxcom/work" `
    -v "amixcom-build:/b/build" `
    -v "${here}\cache:/cache:ro" `
    -v "${here}\prepare.sh:/prepare.sh:ro" `
    @envArgs $image sh /prepare.sh @shArgs
exit $LASTEXITCODE
