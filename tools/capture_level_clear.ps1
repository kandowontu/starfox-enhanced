param([string]$BuildDirectory = 'build/current',
    [ValidatePattern('^[A-Za-z0-9_]+$')][string]$Level='LEVEL1_2',
    [ValidatePattern('^[A-Za-z0-9_]+$')][string]$ClearRoutine='CL_WARP',
    [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
    [ValidateRange(1,10000)][int]$Frames=2400,
    [switch]$RayTracing,
    [string]$OutputDirectory='')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = (Resolve-Path (Join-Path $root $BuildDirectory)).Path
$captures = if($OutputDirectory) {[IO.Path]::GetFullPath($OutputDirectory)} else {Join-Path $build 'level-clear-captures'}
New-Item -ItemType Directory -Force -Path $captures | Out-Null
$settings = @{
    SDL_VIDEODRIVER='dummy'; SDL_AUDIODRIVER='dummy'
    STARFOX_TEST_FRAMES="$Frames"; STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_UNPACED='1'
    STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
    STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_RENDER_SCALE='1'
    STARFOX_TEST_VSYNC='0'; STARFOX_TEST_ANTI_ALIASING='OFF'; STARFOX_TEST_MSU1='0'
    STARFOX_TEST_ENHANCED='0'; STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_RENDERER='GPU'
    STARFOX_CAPTURE_INTERVAL='60'; STARFOX_CAPTURE_START='0'; STARFOX_CAPTURE_PATH=$null
    STARFOX_TEST_CLEAR=$ClearRoutine; STARFOX_TEST_PREROLL_TICKS='200'
    STARFOX_TEST_DLSS_SELECTION='0'
    STARFOX_TEST_RAY_TRACING=([int][bool]$RayTracing).ToString()
    STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'; STARFOX_TEST_BLOOM='0'
    STARFOX_TEST_2D_FILTER='OFF'; STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
    STARFOX_TEST_MODEL_SMOOTHING='0'; STARFOX_TEST_SEPARATED_MODELS='0'; STARFOX_TEST_STEREO_OUTPUT='0'
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
    foreach ($variant in $Experiences) {
        $env:STARFOX_TEST_EXPERIENCE=$variant
        $env:STARFOX_CAPTURE_DIR=Join-Path $captures $variant
        $rom = if ($variant -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
        $symbols = if ($variant -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
        $process = Start-Process -FilePath (Join-Path $build 'starfox_pc.exe') -WorkingDirectory $root `
            -ArgumentList "$rom $symbols $Level" -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $captures "$variant.log") `
            -RedirectStandardError (Join-Path $captures "$variant.err")
        $handle=$process.Handle
        if(!$process.WaitForExit(60000)) {throw "Capture still running: PID $($process.Id), $captures/$variant.err"}
        if ($process.ExitCode -ne 0) { throw "$variant capture failed; see $captures/$variant.err" }
        Write-Output "${variant}: $Level/$ClearRoutine captured for $Frames presentation frames at 60 FPS, 16:9 (inspect captures for sequence coverage)"
    }
} finally {
    foreach ($name in $saved.Keys) {
        if ($null -eq $saved[$name]) { Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue }
        else { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
    }
}
