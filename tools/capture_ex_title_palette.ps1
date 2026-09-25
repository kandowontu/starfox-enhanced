param([string]$OutputDirectory='tmp/ex-title-palette-proof')
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_EXPERIENCE='EX'
        STARFOX_TEST_FRAMES='128'; STARFOX_TEST_PREROLL_TICKS='200'; STARFOX_TEST_SKIP_PREROLL='1'
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
        STARFOX_TEST_RENDERER='GPU'; STARFOX_TEST_RENDER_SCALE='1'; STARFOX_TEST_DISPLAY_MODE='16_9'
        STARFOX_TEST_PPU_DUMP='1'; STARFOX_CAPTURE_DIR=$proof; STARFOX_CAPTURE_INTERVAL='4'
        STARFOX_TRACE_RENDER_STATE='1'; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'; STARFOX_TEST_MATERIAL='0'
        STARFOX_TEST_MANIPULATION='0'; STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_RAY_TRACING='0'; STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_ANTI_ALIASING='0'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'; STARFOX_TEST_STEREO_OUTPUT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'; STARFOX_TEST_2D_FILTER='0'
    }
    foreach($field in 0..5) {$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
    foreach($key in $settings.Keys) {[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    $log=Join-Path $proof 'capture.log'
    $process=Start-Process build/current/starfox_pc.exe -ArgumentList 'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt TITLEMAP' -WindowStyle Hidden -PassThru -RedirectStandardError $log
    $handle=$process.Handle
    if(!$process.WaitForExit(60000)) {throw "Title capture remains running: PID $($process.Id), $log"}
    if($process.ExitCode -ne 0) {throw "Title capture failed: $log"}
    if(!(Select-String -LiteralPath $log -Pattern 'render-state flow=1 ' -Quiet)) {throw 'Title flow was not reached'}
    python tools/check_ex_irq_palette_capture.py --require-title-cycle $proof
    if($LASTEXITCODE -ne 0) {throw 'Title palette cycle did not match source tables'}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
