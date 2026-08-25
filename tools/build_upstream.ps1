param(
    [Parameter()]
    [string]$SourceRoot
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if ([string]::IsNullOrWhiteSpace($SourceRoot)) {
    $SourceRoot = Join-Path $PSScriptRoot '..\upstream-ultrastarfox'
}

$source = (Resolve-Path -LiteralPath $SourceRoot).Path
$dosbox = Join-Path $source 'dosbox-x.exe'
$expectedCommit = '270e959a47d82240d9290a6c6630032c9ec53ff5'
if (-not (Test-Path -LiteralPath $dosbox -PathType Leaf)) {
    throw "UltraStarFox DOSBox toolchain not found at $dosbox"
}

$actualCommit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $expectedCommit) {
    throw "UltraStarFox must be checked out at $expectedCommit (found $actualCommit)"
}

$batchPath = Join-Path $source '.starfox-port-build.bat'
$successPath = Join-Path $source '.starfox-port-build.ok'
if (Test-Path -LiteralPath $batchPath) {
    throw "Refusing to overwrite existing temporary build file: $batchPath"
}

$batch = @'
@echo off
set path=%path%;c:\bin
cd sf
make
if errorlevel 1 goto failed
cd ..
echo ok>.starfox-port-build.ok
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
        throw "UltraStarFox assembler build failed"
    }

    $outputs = @('SF.SFC', 'SYMBOLS.TXT', 'BANKS.CSV')
    foreach ($name in $outputs) {
        $path = Join-Path $source $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "UltraStarFox build did not produce $name"
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
