param([string]$BuildDirectory = 'build/current')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = (Resolve-Path (Join-Path $root $BuildDirectory)).Path
$captures = Join-Path $build 'hitlist-captures'
New-Item -ItemType Directory -Force -Path $captures | Out-Null
$names = @('SDL_VIDEODRIVER','SDL_AUDIODRIVER','STARFOX_TEST_FRAMES',
    'STARFOX_TEST_SKIP_PREROLL','STARFOX_TEST_UNPACED','STARFOX_TEST_PRESENTATION_FPS',
    'STARFOX_TEST_TIMING_MODE','STARFOX_TEST_DISPLAY_MODE','STARFOX_TEST_RENDER_SCALE',
    'STARFOX_TEST_VSYNC','STARFOX_TEST_ANTI_ALIASING','STARFOX_TEST_MSU1',
    'STARFOX_TEST_ENHANCED','STARFOX_TEST_RTX_LIGHTING','STARFOX_CAPTURE_DIR',
    'STARFOX_CAPTURE_INTERVAL','STARFOX_TEST_PREROLL_TICKS','STARFOX_TEST_EX_CROSSHAIR',
    'STARFOX_TEST_EXPERIENCE','STARFOX_TEST_PRESSES','STARFOX_TEST_RENDERER',
    'STARFOX_TEST_ENDING','STARFOX_TEST_ENDING_PREROLL')
$saved = @{}
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name) }
try {
    $env:SDL_VIDEODRIVER='dummy'; $env:SDL_AUDIODRIVER='dummy'
    $env:STARFOX_TEST_SKIP_PREROLL='1'; $env:STARFOX_TEST_UNPACED='1'
    $env:STARFOX_TEST_TIMING_MODE='ORIGINAL'; $env:STARFOX_TEST_DISPLAY_MODE='16_9'
    $env:STARFOX_TEST_VSYNC='0'; $env:STARFOX_TEST_ANTI_ALIASING='OFF'
    $env:STARFOX_TEST_ENHANCED='0'; $env:STARFOX_TEST_RTX_LIGHTING='0'
    $env:STARFOX_TEST_MSU1='0'
    $env:STARFOX_TEST_ENDING=$null; $env:STARFOX_TEST_ENDING_PREROLL=$null
    $cases = @(
        @('retail-scramble','ORIGINAL','LEVEL1_1',0,60,1),
        @('ex-scramble','EX','LEVEL1_1',0,120,4),
        # LEVEL1_END requires the preceding level's player/camera state;
        # direct entry produces black frames, not a valid interior fixture.
        # Tunnel HDMA is covered by hitlist_tests; visual playthrough remains.
        @('retail-blackhole','ORIGINAL','LEVEL_BLACKHOLE',120,120,4),
        @('ex-blackhole','EX','LEVEL_BLACKHOLE',120,120,4),
        @('retail-dimension','ORIGINAL','LEVEL_SPECIAL',100,120,4),
        @('ex-dimension','EX','LEVEL_SPECIAL',100,120,4),
        @('ex-reticle','EX','LEVEL1_2',200,120,4),
        @('software','ORIGINAL','LEVEL1_2',200,120,4)
    )
    foreach ($case in $cases) {
        $name,$experience,$map,$preroll,$fps,$scale = $case
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $env:STARFOX_TEST_PREROLL_TICKS="$preroll"
        $env:STARFOX_TEST_PRESENTATION_FPS="$fps"
        $env:STARFOX_TEST_RENDER_SCALE="$scale"
        $env:STARFOX_TEST_FRAMES="$(3 * $fps)"
        $env:STARFOX_CAPTURE_INTERVAL="$([int]($fps / 4))"
        $env:STARFOX_CAPTURE_DIR=Join-Path $captures $name
        $env:STARFOX_TEST_EX_CROSSHAIR=if ($name -eq 'ex-reticle') {'1'} else {$null}
        $env:STARFOX_TEST_RENDERER=if ($name -eq 'software') {'SOFTWARE'} else {'GPU'}
        $env:STARFOX_TEST_PRESSES=((0..39 | ForEach-Object { "${_}:0" }) -join ',')
        if ($name -eq 'ex-reticle') {
            $env:STARFOX_TEST_PRESSES=((0..59 | ForEach-Object { "$($_ * 3):512" }) -join ',')
        }
        $rom = if ($experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
        $symbols = if ($experience -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
        $timer = [Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath (Join-Path $build 'starfox_pc.exe') -WorkingDirectory $root `
            -ArgumentList "$rom $symbols $map" -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $captures "$name.log") `
            -RedirectStandardError (Join-Path $captures "$name.err")
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "$name failed: $(Get-Content (Join-Path $captures "$name.err") -Raw)" }
        Write-Output "$name captured ($([Math]::Round($timer.Elapsed.TotalSeconds, 2))s, $fps Hz, ${scale}x)"
    }
} finally {
    foreach ($name in $names) { [Environment]::SetEnvironmentVariable($name, $saved[$name]) }
}
