# Metal Mutant port — one-shot setup & build (Windows, VS 2022 + CMake).
#
#   .\scripts\setup.ps1 [-GameData <path to original .IO files>] [-Clean]
#
# Clones the pinned upstream ALIS engine, drops in the mmx module, applies
# the patch series, builds, and assembles a ready-to-run MetalMutant folder.
# Game data files are NOT distributed with this repo — bring your own copy.

param(
    [string]$GameData = '',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$Root  = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build'
$Alis  = Join-Path $Build 'alis'

$upstream = Get-Content (Join-Path $Root 'UPSTREAM') | Where-Object { $_ -and $_ -notmatch '^#' }
$repoUrl  = ($upstream[0] -split '\s+')[0]
$sha      = ($upstream[1] -split '\s+')[0]

if ($Clean -and (Test-Path $Build)) { Remove-Item -Recurse -Force $Build }
New-Item -ItemType Directory -Force $Build > $null

# --- 1. upstream engine at the pinned commit --------------------------------
if (-not (Test-Path $Alis)) {
    Write-Host "Cloning $repoUrl ..."
    git clone $repoUrl $Alis
}
git -C $Alis checkout --detach $sha
if ($LASTEXITCODE -ne 0) { throw "Cannot checkout pinned commit $sha" }

# --- 2. drop-in module + icon + patches -------------------------------------
Copy-Item (Join-Path $Root 'engine\*')          (Join-Path $Alis 'src\') -Force
Copy-Item (Join-Path $Root 'assets\alis32.ico') (Join-Path $Alis 'src\icons\') -Force

git -C $Alis am --abort 2>$null | Out-Null
$patches = Get-ChildItem (Join-Path $Root 'patches\*.patch') | Sort-Object Name
foreach ($p in $patches) {
    git -C $Alis am --3way $p.FullName
    if ($LASTEXITCODE -ne 0) {
        git -C $Alis am --abort
        throw "Patch failed to apply: $($p.Name). Update the patch series for this upstream commit."
    }
}
Write-Host "Applied $($patches.Count) patch(es) on $sha"

# --- 3. SDL2 -----------------------------------------------------------------
$sdlVer = '2.30.11'
$sdlDir = Join-Path $Build "SDL2-$sdlVer"
if (-not (Test-Path $sdlDir)) {
    $zip = Join-Path $Build 'sdl2.zip'
    Write-Host "Downloading SDL2 $sdlVer ..."
    Invoke-WebRequest "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVer/SDL2-devel-$sdlVer-VC.zip" -OutFile $zip
    Expand-Archive $zip -DestinationPath $Build -Force
    Remove-Item $zip
}

# --- 4. build ----------------------------------------------------------------
cmake -S $Alis -B (Join-Path $Alis 'build') -G 'Visual Studio 17 2022' -A x64 -DSDL2_DIR="$sdlDir\cmake"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build (Join-Path $Alis 'build') --config Release
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

# --- 5. assemble the game folder ---------------------------------------------
$Out = Join-Path $Build 'MetalMutant'
New-Item -ItemType Directory -Force $Out, (Join-Path $Out 'data') > $null
Copy-Item (Join-Path $Alis 'build\Release\alis.exe') (Join-Path $Out 'MetalMutant.exe') -Force
Copy-Item (Join-Path $sdlDir 'lib\x64\SDL2.dll') $Out -Force
Copy-Item (Join-Path $Root 'dist\*') $Out -Recurse -Force

if ($GameData -and (Test-Path $GameData)) {
    Copy-Item (Join-Path $GameData '*') (Join-Path $Out 'data\') -Force
    Write-Host "`nDone. Play: $Out\MetalMutant.exe"
}
else {
    Write-Host "`nBuilt OK. Now copy your original Metal Mutant files (*.IO etc.)"
    Write-Host "into $Out\data\ and run MetalMutant.exe"
}
