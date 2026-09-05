param(
    [string]$SourceDirectory = "tmp/reference-snes9x-clean",
    [string]$BuildDirectory = "build/current"
)
$ErrorActionPreference = "Stop"
$revision = "890b5d445538fe790aa3add3d5702c80f551e0ae"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    if (!(Test-Path -LiteralPath $SourceDirectory)) {
        & git clone https://github.com/libretro/snes9x.git $SourceDirectory
        if ($LASTEXITCODE) { throw "Reference clone failed" }
        & git -C $SourceDirectory checkout --detach $revision
        if ($LASTEXITCODE) { throw "Reference checkout failed" }
    }
    $actual = & git -C $SourceDirectory rev-parse HEAD
    if ($LASTEXITCODE -or $actual -ne $revision) { throw "Reference revision differs from $revision" }
    $sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
    $coreFile = Join-Path $sourcePath "libretro/libretro.cpp"
    $original = & git -C $sourcePath show HEAD:libretro/libretro.cpp
    if ($LASTEXITCODE) { throw "Cannot read pinned reference source" }
    $bridge = Get-Content -LiteralPath (Join-Path $PSScriptRoot "snes9x_bridge.inc") -Raw
    $expected = ($original -join "`n") + "`n`n" + $bridge
    $current = [IO.File]::ReadAllText($coreFile).Replace("`r`n", "`n").TrimEnd()
    $clean = ($original -join "`n").TrimEnd()
    if ($current -ne $clean -and $current -ne $expected.Replace("`r`n", "`n").TrimEnd()) {
        throw "Reference libretro.cpp contains other edits; use a fresh SourceDirectory"
    }
    $otherEdits = & git -C $sourcePath diff --name-only -- . ':!libretro/libretro.cpp'
    if ($otherEdits) { throw "Reference checkout contains unrelated source changes" }
    [IO.File]::WriteAllText($coreFile, $expected, [Text.UTF8Encoding]::new($false))
    & mingw32-make.exe -C (Join-Path $sourcePath "libretro") platform=win CC=gcc CXX=g++ LTO= GIT_VERSION= -j4
    if ($LASTEXITCODE) { throw "Reference build failed" }
    & cmake -S . -B $BuildDirectory "-DSTARFOX_REFERENCE_CORE_DIR=$sourcePath"
    if ($LASTEXITCODE) { throw "Audit configuration failed" }
    & cmake --build $BuildDirectory --target starfox_reference_render -j4
    if ($LASTEXITCODE) { throw "Audit build failed" }
} finally { Pop-Location }
