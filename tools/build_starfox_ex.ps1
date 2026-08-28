param(
    [Parameter()]
    [string]$SourceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $PSScriptRoot '..\upstream-star-fox-ex'
}

$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$dosbox = Join-Path $source 'dosbox-x.exe'
$expectedCommit = 'b5e2d837a15a72a532cd019bfe332b7a4b660924'
if (-not (Test-Path -LiteralPath $dosbox -PathType Leaf)) {
    throw "Star Fox EX DOSBox toolchain not found at $dosbox"
}

$actualCommit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "Star Fox EX must be checked out at $expectedCommit (found $actualCommit)"
}

$batchPath = Join-Path $source '.starfox-port-ex-build.bat'
$successPath = Join-Path $source '.starfox-port-ex-build.ok'
if (Test-Path -LiteralPath $batchPath) {
    throw "Refusing to overwrite existing temporary build file: $batchPath"
}

$batch = @'
@echo off
set path=%path%;c:\bin
set sasmheap=14400
cd sfes
make clean
if errorlevel 1 goto failed
make hardware=0 newface=1
if errorlevel 1 goto failed
copy sfes.sfc ..\sfes.sfc
if errorlevel 1 goto failed
cd ..
echo ok>.starfox-port-ex-build.ok
exit
:failed
cd ..
exit
'@

try {
    if (Test-Path -LiteralPath $successPath) {
        Remove-Item -LiteralPath $successPath -Force
    }
    [IO.File]::WriteAllText($batchPath, $batch, [Text.Encoding]::ASCII)
    $process = Start-Process `
        -FilePath $dosbox `
        -ArgumentList @('-fastlaunch', (Split-Path -Leaf $batchPath)) `
        -WorkingDirectory $source `
        -WindowStyle Hidden `
        -Wait `
        -PassThru

    if ($process.ExitCode -ne 0 -or -not (Test-Path -LiteralPath $successPath)) {
        throw 'Star Fox EX assembler build failed'
    }

    $outputs = @('SFES\SFES.SFC', 'SYMBOLS.TXT', 'BANKS.CSV')
    foreach ($name in $outputs) {
        $path = Join-Path $source $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Star Fox EX build did not produce $name"
        }
        Get-Item -LiteralPath $path | Select-Object Name, Length, LastWriteTime
    }
}
finally {
    if (Test-Path -LiteralPath $batchPath) {
        Remove-Item -LiteralPath $batchPath -Force
    }
    if (Test-Path -LiteralPath $successPath) {
        Remove-Item -LiteralPath $successPath -Force
    }
}
