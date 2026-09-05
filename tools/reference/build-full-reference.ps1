param(
    [string]$SourceDirectory = 'tmp/ares-timing',
    [string]$BuildDirectory = 'tmp/full-reference-build',
    [string]$CoreBuildDirectory = 'build/current'
)
$ErrorActionPreference = 'Stop'
$revision = '0aafd85789215e84e1e43415c07d4c88461b7899'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    if (!(Test-Path -LiteralPath $SourceDirectory)) {
        & git clone --filter=blob:none --depth 1 --branch v148 --sparse https://github.com/ares-emulator/ares.git $SourceDirectory
        if ($LASTEXITCODE) { throw 'Ares clone failed' }
    }
    $actual = & git -C $SourceDirectory rev-parse HEAD
    if ($LASTEXITCODE -or $actual -ne $revision) { throw "Ares revision differs from $revision" }
    $changes = & git -C $SourceDirectory status --porcelain --untracked-files=all
    if ($LASTEXITCODE -or $changes) { throw 'Ares reference requires an unmodified pinned checkout' }
    & git -C $SourceDirectory sparse-checkout add ares/ares ares/sfc `
        ares/component/processor/gsu ares/component/processor/wdc65816 `
        ares/component/processor/arm7tdmi ares/component/processor/hg51b `
        ares/component/processor/spc700 ares/component/processor/upd96050 `
        libco thirdparty/sljit nall mia/Database mia/system 'mia/Firmware/Super Famicom'
    if ($LASTEXITCODE) { throw 'Ares full-system sparse checkout failed' }
    & cmake --build $CoreBuildDirectory --target starfox_core -j4
    if ($LASTEXITCODE) { throw 'Build the port before building its full-system reference' }
    $sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path.Replace('\','/')
    $corePath = (Resolve-Path -LiteralPath $CoreBuildDirectory).Path.Replace('\','/')
    & cmake -S tools/reference/full-system -B $BuildDirectory -G Ninja `
        -DCMAKE_BUILD_TYPE=Release "-DARES_SOURCE=$sourcePath" "-DSTARFOX_CORE_BUILD_DIR=$corePath"
    if ($LASTEXITCODE) { throw 'Full reference configuration failed' }
    & cmake --build $BuildDirectory -j4
    if ($LASTEXITCODE) { throw 'Full reference build failed' }
} finally { Pop-Location }
