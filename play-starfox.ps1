param(
    [string]$Map = "BOOT"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$RomPath = Join-Path $ProjectRoot "upstream-ultrastarfox\SF.SFC"
$SymbolsPath = Join-Path $ProjectRoot "upstream-ultrastarfox\SYMBOLS.TXT"
$ExRomPath = Join-Path $ProjectRoot "upstream-star-fox-ex\SFES\SFES.SFC"
$ExSymbolsPath = Join-Path $ProjectRoot "upstream-star-fox-ex\SYMBOLS.TXT"
$BuildPath = Join-Path $ProjectRoot "build\release"
$ExecutablePath = Join-Path $BuildPath "starfox_pc.exe"

if (-not (Test-Path -LiteralPath $RomPath) -or
    -not (Test-Path -LiteralPath $SymbolsPath) -or
    -not (Test-Path -LiteralPath $ExRomPath) -or
    -not (Test-Path -LiteralPath $ExSymbolsPath)) {
    throw "Missing Original or EX build output. Run tools\build_upstream.ps1 and tools\build_starfox_ex.ps1 first."
}

if (-not (Test-Path -LiteralPath $ExecutablePath)) {
    cmake -S $ProjectRoot -B $BuildPath -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }
    cmake --build $BuildPath --target starfox_pc -j 8
    if ($LASTEXITCODE -ne 0) { throw "Star Fox build failed." }
}

& $ExecutablePath $RomPath $SymbolsPath $Map
