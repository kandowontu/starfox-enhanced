param(
    [ValidateSet('ORIGINAL', 'EX')][string]$Experience = 'ORIGINAL',
    [ValidateSet('0', '1')][string]$Msu = '0',
    [string]$OutputDirectory = 'tmp/runtime-ending-audio',
    [string]$Runtime = 'build/current/starfox_pc.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Push-Location $root
try {
    $runtimePath = (Resolve-Path -LiteralPath $Runtime).Path
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $outputPath = (Resolve-Path -LiteralPath $OutputDirectory).Path
    $env:SDL_VIDEODRIVER = 'dummy'
    $env:SDL_AUDIODRIVER = 'dummy'
    $env:STARFOX_TEST_FRAMES = '30000'
    $env:STARFOX_TEST_UNPACED = '1'
    $env:STARFOX_TEST_ENDING = '1'
    $env:STARFOX_TEST_SKIP_PREROLL = '1'
    $env:STARFOX_TEST_RENDER_SCALE = '1'
    $env:STARFOX_TEST_PRESENTATION_FPS = '20'
    $env:STARFOX_TEST_TIMING_MODE = 'ORIGINAL'
    $env:STARFOX_TEST_EXPERIENCE = $Experience
    $env:STARFOX_TEST_MSU1 = $Msu
    $env:STARFOX_TEST_DISPLAY_MODE = '0'
    $env:STARFOX_TEST_ANTI_ALIASING = '0'
    $env:STARFOX_TEST_ENHANCED = '0'
    $env:STARFOX_TEST_RTX_LIGHTING = '0'
    $env:STARFOX_TEST_VSYNC = '0'
    $env:STARFOX_TRACE_AUDIO = Join-Path $outputPath 'audio.csv'
    $env:STARFOX_TRACE_MSU1 = '1'
    $process = Start-Process -FilePath $runtimePath -ArgumentList 'LEVEL1_6' `
        -WorkingDirectory $outputPath -WindowStyle Hidden -PassThru -Wait `
        -RedirectStandardOutput (Join-Path $outputPath 'stdout.log') `
        -RedirectStandardError (Join-Path $outputPath 'stderr.log')
    if ($process.ExitCode) { throw "Runtime failed: $($process.ExitCode); see $outputPath/stderr.log" }
    Get-Content -LiteralPath (Join-Path $outputPath 'stderr.log') -Tail 10
} finally { Pop-Location }
