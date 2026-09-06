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
        & git -C $SourceDirectory sparse-checkout set ares/component/processor/gsu ares/component/processor/wdc65816 ares/sfc/coprocessor/superfx nall
        if ($LASTEXITCODE) { throw "Ares sparse checkout failed" }
    }
    $actual = & git -C $SourceDirectory rev-parse HEAD
    if ($LASTEXITCODE -or $actual -ne $revision) { throw "Ares revision differs from $revision" }
    if (!(Test-Path -LiteralPath (Join-Path $SourceDirectory 'ares/component/processor/wdc65816/wdc65816.hpp'))) {
        & git -C $SourceDirectory sparse-checkout add ares/component/processor/wdc65816
        if ($LASTEXITCODE) { throw "Ares CPU sparse checkout failed" }
    }
    if (!(Test-Path -LiteralPath (Join-Path $SourceDirectory 'ares/sfc/cpu/irq.cpp'))) {
        & git -C $SourceDirectory sparse-checkout add ares/sfc/cpu
        if ($LASTEXITCODE) { throw "Ares timer-controller sparse checkout failed" }
    }
    $changes = & git -C $SourceDirectory status --porcelain --untracked-files=all
    if ($LASTEXITCODE -or $changes) { throw "Ares audit requires an unmodified pinned checkout" }
    $sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
    $referencePath = (Resolve-Path -LiteralPath $ReferenceDirectory).Path
    if (!(Test-Path -LiteralPath (Join-Path $referencePath 'libretro/snes9x_libretro.dll'))) {
        throw "Build the bootstrap reference first with tools/reference/build-reference.ps1"
    }
    & cmake -S . -B $BuildDirectory "-DSTARFOX_REFERENCE_CORE_DIR=$referencePath" "-DSTARFOX_REFERENCE_ARES_DIR=$sourcePath"
    if ($LASTEXITCODE) { throw "Ares audit configuration failed" }
    & cmake --build $BuildDirectory --target starfox_reference_ares_render starfox_reference_ares_tests starfox_reference_gsu_device_tests starfox_reference_view starfox_reference_cpu_tests starfox_reference_interrupt_tests starfox_reference_halt_tests starfox_reference_dma_tests starfox_reference_timer_tests starfox_reference_raster_tests -j4
    if ($LASTEXITCODE) { throw "Ares audit build failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_ares_tests.exe')
    if ($LASTEXITCODE) { throw "Ares adapter checks failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_gsu_device_tests.exe')
    if ($LASTEXITCODE) { throw "Resumable GSU comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_cpu_tests.exe') (Join-Path $BuildDirectory 'reference-cpu.csv')
    if ($LASTEXITCODE) { throw "Ares CPU comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_interrupt_tests.exe') (Join-Path $BuildDirectory 'reference-interrupts.csv')
    if ($LASTEXITCODE) { throw "Ares interrupt-entry comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_halt_tests.exe')
    if ($LASTEXITCODE) { throw "Ares WAI/STP comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_dma_tests.exe')
    if ($LASTEXITCODE) { throw "Ares general DMA comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_timer_tests.exe')
    if ($LASTEXITCODE) { throw "Ares timer-controller comparisons failed" }
    & (Join-Path $BuildDirectory 'starfox_reference_raster_tests.exe')
    if ($LASTEXITCODE) { throw "Ares raster comparisons failed" }
} finally { Pop-Location }
