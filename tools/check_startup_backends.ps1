param([string]$OutputDirectory='tmp/startup-backends-proof',
    [string]$Executable='build/current/starfox_pc.exe',
    [ValidateSet(0,46,47)][int]$TemporalEffect=0,[switch]$LowPowerGpu,
    [switch]$DisableDlssRuntime,[switch]$NoCycle,[switch]$CpuGeometry,[switch]$CpuPresentation,[switch]$GpuValidation,
    [ValidateSet('software','direct3d12','vulkan','default')][string[]]$Backends=@('software','direct3d12','vulkan'))
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
$startupLog=Join-Path (Split-Path ([IO.Path]::GetFullPath($Executable))) 'startup.log'
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='40'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_PREROLL_TICKS='240'
        STARFOX_TEST_MENU_PREVIEW='1'; STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'
        STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_RENDER_SCALE='1'
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_MSU1='0'; STARFOX_TEST_STEREO_OUTPUT='0'; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_RAY_TRACING='0'; STARFOX_TEST_SOFTWARE_SHADOWS='0'
        STARFOX_TEST_REFLECTIVE_SURFACES='0'; STARFOX_TEST_RTX_LIGHTING='0'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'; STARFOX_TEST_EFFECT="$TemporalEffect"
        STARFOX_TEST_WORLD_EFFECT='0'; STARFOX_TEST_2D_FILTER='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TEST_MODEL_SMOOTHING='0'; STARFOX_TEST_ANTI_ALIASING='0'
    }
    foreach($key in $settings.Keys) {Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    if($LowPowerGpu) {$env:STARFOX_TEST_LOW_POWER_GPU='1'}
    if($GpuValidation) {$env:STARFOX_TEST_GPU_VALIDATION='1'}
    if($CpuGeometry) {$env:STARFOX_DISABLE_GPU_GEOMETRY='1'}
    if($CpuPresentation) {$env:STARFOX_DISABLE_GPU_NATIVE='1'}
    if($DisableDlssRuntime) {
        $env:STARFOX_DLSS_ADAPTER=Join-Path $proof 'intentionally-unavailable-adapter.dll'
        $env:STARFOX_DLSS_BINARIES=$proof
    }
    if($TemporalEffect){$env:STARFOX_TRACE_GPU='1'}
    Add-Type -AssemblyName System.Drawing
    foreach($experience in @('ORIGINAL','EX')) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'}
            else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
        foreach($backend in $Backends) {
            $name="$experience-$backend"
            if($backend -eq 'default') {Remove-Item Env:SDL_GPU_DRIVER -ErrorAction SilentlyContinue}
            else {$env:SDL_GPU_DRIVER=$backend}
            $env:STARFOX_TEST_RENDERER=if($backend -eq 'software'){'SOFTWARE'}else{'GPU'}
            if($backend -eq 'software' -or $NoCycle) {Remove-Item Env:STARFOX_TEST_RENDERER_CYCLE -ErrorAction SilentlyContinue}
            else {$env:STARFOX_TEST_RENDERER_CYCLE='1'}
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$name.bmp"
            $before=if(Test-Path $startupLog){@(Get-Content $startupLog).Count}else{0}
            $run=Start-Process $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proof/$name.log"
            $handle=$run.Handle
            if(!$run.WaitForExit(45000)) {throw "Startup check remains running: PID $($run.Id), $name. Inspect before retrying."}
            if($run.ExitCode -ne 0){throw "Startup failure: $name, exit $($run.ExitCode)"}
            $journal=@(Get-Content $startupLog | Select-Object -Skip $before)
            if(!($journal -match 'first game/menu frame presented')) {throw "No first-frame milestone: $name"}
            $expected=if($backend -eq 'software'){'software'}else{'gpu'}
            if(!($journal -match "renderer selected: $expected$")){throw "Wrong startup renderer: $name"}
            $log=Get-Content "$proof/$name.log" -Raw
            $expectedCycles=if($NoCycle){0}else{4}
            if($backend -ne 'software' -and ([regex]::Matches($log,'renderer-cycle frame=').Count -ne $expectedCycles -or $log.Contains('SDL GPU unavailable:'))) {
                throw "GPU startup/cycling not exercised: $name"
            }
            if($TemporalEffect -and $backend -ne 'software' -and
                [regex]::Matches($log,'frame persistence reset: render options epoch=').Count -ne (1+$expectedCycles)) {
                throw "Temporal history was not reset exactly once at startup and each renderer switch: $name"
            }
            $bitmap=[Drawing.Bitmap]::FromFile($env:STARFOX_CAPTURE_PRESENTATION_PATH)
            try {
                $visible=0
                for($y=0;$y -lt $bitmap.Height;$y+=4){for($x=0;$x -lt $bitmap.Width;$x+=4){
                    if(($bitmap.GetPixel($x,$y).ToArgb() -band 0xffffff) -ne 0){++$visible}
                }}
                if($visible -lt 100){throw "Black or nearly empty preview: $name"}
            } finally {$bitmap.Dispose()}
            Write-Output "$name startup/preview passed ($visible nonblack samples)"
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){Set-Item -LiteralPath "Env:$key" -Value $saved[$key]}
}
