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

$nativePatch = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot `
    '..\config\ultrastarfox-native-runtime.patch')).Path
function Test-NativePatch {
    param([switch]$Reverse)
    # Windows PowerShell promotes a native process's expected stderr from a
    # failed --check to a terminating ErrorRecord under Stop. Suppress that
    # probe locally while still preserving the real git exit code.
    $savedPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $arguments = @('-C', $source, 'apply')
        if ($Reverse) { $arguments += '--reverse' }
        $arguments += @('--check', '--ignore-space-change',
            '--ignore-whitespace', $nativePatch)
        & git @arguments 2>&1 | Out-Null
        return $LASTEXITCODE -eq 0
    }
    finally {
        $ErrorActionPreference = $savedPreference
    }
}

if (Test-NativePatch) {
    & git -C $source apply --ignore-space-change --ignore-whitespace $nativePatch
    if ($LASTEXITCODE -ne 0) {
        throw "Could not apply the UltraStarFox native-runtime feature patch"
    }
}
else {
    if (-not (Test-NativePatch -Reverse)) {
        throw "UltraStarFox source does not match the native-runtime feature patch"
    }
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
