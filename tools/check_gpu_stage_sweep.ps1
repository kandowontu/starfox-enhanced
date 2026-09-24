param([string]$OutputDirectory='tmp/gpu-stage-sweep',
    [ValidateSet('direct3d12','vulkan')][string]$GpuDriver='direct3d12',
    [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
    [ValidateSet('4_3','16_9','32_9')][string]$DisplayMode='16_9',
    [switch]$IncludeSpecialRoutes,
    [switch]$RasterOnly,
    [switch]$AllowUniformFinal,
    [switch]$RequireNoCpuUpload,
    [switch]$LowPowerGpu,
    [switch]$TraceGpuModelDispatch,
    [string[]]$Levels=@(), [ValidateRange(0,8000)][int]$Ticks=1000,
    [ValidateRange(1,240)][int]$Frames=12)
$ErrorActionPreference='Stop'
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        STARFOX_TEST_DISPLAY_MODE=$DisplayMode; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_SHOW_FPS='0'
        STARFOX_TEST_GOD_MODE='1'; STARFOX_TEST_STEREO_OUTPUT='0'
        STARFOX_TEST_MSU1='0'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_SOFTWARE_SHADOWS='0'; STARFOX_TEST_REFLECTIVE_SURFACES='0'
        STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TEST_MODEL_SMOOTHING='0'; STARFOX_TEST_ANTI_ALIASING='0'
        STARFOX_TEST_2D_FILTER='0'
        STARFOX_TEST_MANIPULATION='0'; STARFOX_TEST_MATERIAL='0'
    }
    if($LowPowerGpu){$settings.STARFOX_TEST_LOW_POWER_GPU='1'}
    if($TraceGpuModelDispatch){$settings.STARFOX_TRACE_GPU_MODEL_DISPATCH='1'}
    # Environment upgrades are persisted in the user's configuration. Clearing
    # process variables alone does not reset them; baseline comparisons must
    # explicitly select OFF without modifying the saved configuration.
    for($field=0;$field -lt 6;++$field){$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
    foreach($key in $settings.Keys){Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    $passed=0
    foreach($experience in $Experiences) {
        $symbols=if($experience -eq 'EX'){'assets/symbols/starfox-ex.txt'}else{'upstream-ultrastarfox/SYMBOLS.TXT'}
        $rom=if($experience -eq 'EX'){'tmp/runtime-inputs/starfox-ex/SFES.SFC'}else{'upstream-ultrastarfox/SF.SFC'}
        $available=@(Get-Content -LiteralPath $symbols | ForEach-Object {
            if($_ -match '^(LEVEL(?:[1-7]_[1-9]|_BLACKHOLE|_SPECIAL|_COMET))\s'){$Matches[1]}
        } | Sort-Object -Unique)
        $selected=if($Levels.Count){$Levels}else{
            @($available | Where-Object {$IncludeSpecialRoutes -or $_ -match '^LEVEL[1-7]_[1-9]$'})
        }
        foreach($level in $selected) {
            if($level -notin $available){throw "Unknown stage: $experience $level"}
            # Fail immediately on a live timeout or parity/fallback error. The
            # underlying harness reports the PID; never restart that process.
            & "$PSScriptRoot/check_gpu_native.ps1" -OutputDirectory "$OutputDirectory/$experience-$level" `
                -Experience $experience -Rom $rom -Symbols $symbols -Level $level -GpuDriver $GpuDriver `
                -Ticks $Ticks -Frames $Frames -Warmup 0 -Scales 1 -PresentationFps 60 `
                -DefaultPipeline -Geometry:(!$RasterOnly) -RasterOnly:$RasterOnly `
                -PresentationCapture -AllowUniformFinal:$AllowUniformFinal `
                -RequireNoCpuUpload:$RequireNoCpuUpload -Bloom 0
            # Verify the requested aspect reached final presentation, rather
            # than comparing two equally wrong inherited display settings.
            $expectedWidth=switch($DisplayMode){'4_3'{299};'16_9'{400};'32_9'{800}}
            foreach($mode in @('cpu','gpu')) {
                $capture=Join-Path "$OutputDirectory/$experience-$level" "$experience-$level-$Ticks-60fps-1x-$mode-presentation.bmp"
                $header=[IO.File]::ReadAllBytes([IO.Path]::GetFullPath($capture))
                if($header.Length -lt 26 -or $header[0] -ne 66 -or $header[1] -ne 77 -or
                    [BitConverter]::ToInt32($header,18) -ne $expectedWidth -or
                    [Math]::Abs([BitConverter]::ToInt32($header,22)) -ne 224) {
                    throw "Unexpected final presentation dimensions: $capture ($DisplayMode)"
                }
            }
            ++$passed
            Write-Output "Stage sweep: $passed passed; $experience $level"
        }
    }
    Write-Output "Completed $passed stage execution/parity samples ($GpuDriver)"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){Set-Item -LiteralPath "Env:$key" -Value $saved[$key]}
}
