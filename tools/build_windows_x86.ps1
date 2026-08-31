param(
    [Parameter(Mandatory = $true)]
    [string]$LlvmMingwRoot,
    [string]$BuildDirectory = "build/windows-x86",
    [string]$InstallDirectory = "dist/StarFoxEnhanced-windows-x86"
)

$ErrorActionPreference = "Stop"
$sourceRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$toolchainRoot = (Resolve-Path -LiteralPath $LlvmMingwRoot).Path
$buildPath = [IO.Path]::GetFullPath((Join-Path $sourceRoot $BuildDirectory))
$installPath = [IO.Path]::GetFullPath((Join-Path $sourceRoot $InstallDirectory))
$toolchainFile = Join-Path $sourceRoot "cmake/toolchains/windows-i686-llvm-mingw.cmake"

$configureArguments = @(
    "-S", $sourceRoot,
    "-B", $buildPath,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DLLVM_MINGW_ROOT=$toolchainRoot",
    "-DSTARFOX_BUILD_TESTS=OFF",
    "-DSTARFOX_BUILD_RUNTIME=ON",
    "-DSTARFOX_EMBED_RUNTIME_ASSETS=ON",
    "-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF"
)
& cmake @configureArguments
if ($LASTEXITCODE -ne 0) { throw "x86 CMake configure failed" }
& cmake --build $buildPath --target starfox_pc starfox_asset_builder
if ($LASTEXITCODE -ne 0) { throw "x86 build failed" }
& cmake --install $buildPath --prefix $installPath
if ($LASTEXITCODE -ne 0) { throw "x86 install failed" }
