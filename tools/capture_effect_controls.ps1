param([string]$OutputDirectory='tmp/effect-controls-proof',[switch]$TwoD)
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';SDL_GPU_DRIVER='direct3d12';STARFOX_TEST_HIDDEN='1'
        STARFOX_TEST_FRAMES='90';STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000'
        STARFOX_TEST_STATE_ACTIONS='10:6'
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';STARFOX_TEST_PRESENTATION_FPS='60'
        STARFOX_TEST_TIMING_MODE='ORIGINAL';STARFOX_TEST_EXPERIENCE='ORIGINAL'
        STARFOX_TEST_DISPLAY_MODE='16_9';STARFOX_TEST_RENDER_SCALE='2';STARFOX_TEST_RENDERER='GPU'
        STARFOX_TEST_EFFECT='1';STARFOX_TEST_MATERIAL='57';STARFOX_TEST_MANIPULATION='51'
        STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0'
        STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_BLOOM='0';STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_2D_FILTER='0';STARFOX_TEST_ANTI_ALIASING='0';STARFOX_TEST_HDR_EFFECT='0'
        STARFOX_TEST_CHROMATIC_ABERRATION='0';STARFOX_TEST_MODEL_SMOOTHING='0'
        STARFOX_TEST_FSR1_SELECTION='0';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_STEREO_OUTPUT='0'
        STARFOX_TEST_LANGUAGE='0';STARFOX_TEST_MSU1='0';STARFOX_TEST_SOFTWARE_SHADOWS='0'
        STARFOX_TEST_PRESSES='20:0x0800,30:0x0080,40:0x0800,50:0x0800,60:0x0800,70:0x0800'
        STARFOX_CAPTURE_PRESENTATION_PATH=(Join-Path $proof 'menu.bmp')
    }
    if($TwoD) {
        $settings.STARFOX_TEST_PRESSES='20:0x0800,30:0x0800,40:0x0080'
        $settings.STARFOX_TEST_EFFECT='0';$settings.STARFOX_TEST_MATERIAL='0';$settings.STARFOX_TEST_MANIPULATION='0'
        $settings.STARFOX_TEST_ENVIRONMENT_0='1';$settings.STARFOX_TEST_ENVIRONMENT_3='1'
    }
    foreach($key in $settings.Keys){Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    $run=Start-Process build/current/starfox_pc.exe -ArgumentList 'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1' -WindowStyle Hidden -PassThru -RedirectStandardError "$proof/menu.log"
    $handle=$run.Handle
    if(!$run.WaitForExit(60000)){throw "Capture still running: $($run.Id)"}
    if($run.ExitCode -ne 0 -or !(Test-Path "$proof/menu.bmp")){throw 'Menu capture failed'}
    Write-Output "Captured independent controls and inactive material: $proof/menu.bmp"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){Set-Item -LiteralPath "Env:$key" -Value $saved[$key]}
}
