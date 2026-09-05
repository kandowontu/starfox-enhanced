param([string]$BuildDirectory = 'build/current')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = (Resolve-Path (Join-Path $root $BuildDirectory)).Path
$captures = Join-Path $build 'level-clear-captures'
New-Item -ItemType Directory -Force -Path $captures | Out-Null
$settings = @{
    SDL_VIDEODRIVER='dummy'; SDL_AUDIODRIVER='dummy'
    STARFOX_TEST_FRAMES='2400'; STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_UNPACED='1'
    STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
    STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_RENDER_SCALE='1'
    STARFOX_TEST_VSYNC='0'; STARFOX_TEST_ANTI_ALIASING='OFF'; STARFOX_TEST_MSU1='0'
    STARFOX_TEST_ENHANCED='0'; STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_RENDERER='GPU'
    STARFOX_CAPTURE_INTERVAL='60'; STARFOX_CAPTURE_START='0'; STARFOX_CAPTURE_PATH=$null
    STARFOX_TEST_CLEAR='CL_WARP'; STARFOX_TEST_PREROLL_TICKS='200'
    STARFOX_TEST_ENDING=$null; STARFOX_TEST_ENDING_PREROLL=$null
    STARFOX_TEST_EX_CROSSHAIR=$null; STARFOX_TEST_FAST_FORWARD=$null
    STARFOX_TEST_PRESSES='0:0'; STARFOX_TEST_EXPERIENCE=$null; STARFOX_CAPTURE_DIR=$null
}
$saved = @{}
foreach ($name in $settings.Keys) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    foreach ($name in $settings.Keys) {
        if ($null -eq $settings[$name]) { Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($name, $settings[$name]) }
    }
    foreach ($variant in @('ORIGINAL','EX')) {
        $env:STARFOX_TEST_EXPERIENCE=$variant
        $env:STARFOX_CAPTURE_DIR=Join-Path $captures $variant
        $rom = if ($variant -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
        $symbols = if ($variant -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
        $process = Start-Process -FilePath (Join-Path $build 'starfox_pc.exe') -WorkingDirectory $root `
            -ArgumentList "$rom $symbols LEVEL1_2" -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $captures "$variant.log") `
            -RedirectStandardError (Join-Path $captures "$variant.err")
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "$variant capture failed; see $captures/$variant.err" }
        Write-Output "${variant}: clear/tally/warp captured at 60 FPS, 16:9"
    }
} finally {
    foreach ($name in $saved.Keys) {
        if ($null -eq $saved[$name]) { Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    }
}
