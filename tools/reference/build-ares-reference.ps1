param(
    [string]$SourceDirectory = "tmp/ares-timing",
    [string]$ReferenceDirectory = "tmp/reference-snes9x-clean",
    [string]$BuildDirectory = "build/current"
)
$ErrorActionPreference = "Stop"
$revision = "0aafd85789215e84e1e43415c07d4c88461b7899"
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    if (!(Test-Path -LiteralPath $SourceDirectory)) {
        & git clone --filter=blob:none --depth 1 --branch v148 --sparse https://github.com/ares-emulator/ares.git $SourceDirectory
        if ($LASTEXITCODE) { throw "Ares clone failed" }
        & git -C $SourceDirectory sparse-checkout set ares/component/processor/gsu ares/sfc/coprocessor/superfx nall
        if ($LASTEXITCODE) { throw "Ares sparse checkout failed" }
    }
    $actual = & git -C $SourceDirectory rev-parse HEAD
    if ($LASTEXITCODE -or $actual -ne $revision) { throw "Ares revision differs from $revision" }
    $changes = & git -C $SourceDirectory status --porcelain --untracked-files=all
    if ($LASTEXITCODE -or $changes) { throw "Ares audit requires an unmodified pinned checkout" }
    $sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
    $referencePath = (Resolve-Path -LiteralPath $ReferenceDirectory).Path
    if (!(Test-Path -LiteralPath (Join-Path $referencePath 'libretro/snes9x_libretro.dll'))) {
        throw "Build the bootstrap reference first with tools/reference/build-reference.ps1"
    }
    & cmake -S . -B $BuildDirectory "-DSTARFOX_REFERENCE_CORE_DIR=$referencePath" "-DSTARFOX_REFERENCE_ARES_DIR=$sourcePath"
    if ($LASTEXITCODE) { throw "Ares audit configuration failed" }
    & cmake --build $BuildDirectory --target starfox_reference_ares_render starfox_reference_ares_tests starfox_reference_view -j4
    if ($LASTEXITCODE) { throw "Ares audit build failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_ares_tests.exe')
    if ($LASTEXITCODE) { throw "Ares adapter checks failed" }
} finally { Pop-Location }
