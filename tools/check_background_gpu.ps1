param([string]$OutputDirectory='tmp/background-runtime-gpu-proof',
    [string[]]$Levels=@('LEVEL1_1'),[int]$PrerollTicks=1000,
    [ValidateSet('4_3','16_9','32_9')][string]$DisplayMode='16_9',
    [ValidateRange(1,4)][int]$RenderScale=2,
    [ValidateRange(0,5)][int]$Filter=0,
    [ValidateRange(0,5)][int]$Language=0,
    [int]$Message=-1,[switch]$RequireHostInk,
    [string]$ClearRoutine='',
    [switch]$EnhancedLighting,
    [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),[switch]$CaptureIsolatedSources,
    [ValidateRange(0,2)][int]$Stereo=0,[switch]$Effects,[switch]$RequireExLogoRepair,[switch]$RequireLateCartridge,[switch]$RequireIsolatedSources,
    [string]$Presses='', [ValidateRange(1,10000)][int]$Frames=6,[switch]$RendererCycle)
$ErrorActionPreference='Stop'
if($RendererCycle -and ($Frames -lt 33 -or [math]::Floor(($Frames-1)/8)%2 -ne 0)) {
    throw 'Renderer cycling requires at least 33 frames and must finish on GPU (e.g. 40 frames).'
}
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames"
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS="$PrerollTicks"
        STARFOX_TEST_DISPLAY_MODE=$DisplayMode;STARFOX_TEST_RENDER_SCALE="$RenderScale"
        STARFOX_TEST_STEREO_OUTPUT="$Stereo"
        STARFOX_TEST_PRESENTATION_FPS='60';STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';STARFOX_TEST_MSU1='0'
        STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TEST_RTX_LIGHTING='0'
        STARFOX_TEST_BLOOM='0';STARFOX_TEST_BLOOM_2D='0';STARFOX_TEST_EFFECT='0'
        STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_MODEL_SMOOTHING='0'
        STARFOX_TEST_DLSS_SELECTION='0'
        STARFOX_TEST_2D_FILTER="$Filter"
        STARFOX_TEST_LANGUAGE="$Language"
        STARFOX_TEST_HDR_EFFECT='0';STARFOX_TEST_CHROMATIC_ABERRATION='0';STARFOX_TRACE_GPU='1'
    }
    foreach($key in $settings.Keys) {[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    if($RendererCycle){$env:STARFOX_TEST_RENDERER_CYCLE='1'}
    if($EnhancedLighting){$env:STARFOX_TEST_RTX_LIGHTING='1'}
    if($Presses){$env:STARFOX_TEST_PRESSES=$Presses}
    if($Message -ge 0){$env:STARFOX_TEST_MESSAGE="$Message"}
    if($ClearRoutine){$env:STARFOX_TEST_CLEAR=$ClearRoutine}
    if($Effects) {
        $env:STARFOX_TEST_BLOOM='1';$env:STARFOX_TEST_BLOOM_2D='1'
        $env:STARFOX_TEST_EFFECT='1';$env:STARFOX_TEST_WORLD_EFFECT='2'
        $env:STARFOX_TEST_HDR_EFFECT='1';$env:STARFOX_TEST_CHROMATIC_ABERRATION='1'
    }
    foreach($experience in $Experiences) {foreach($level in $Levels) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $level"}
            else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $level"}
        $hashes=@{}
        $modes=if($Stereo) {@('gpu','cpu-background')}else{@('gpu','cpu-background','fallback','cpu-models')}
        if($RequireIsolatedSources -and !$Stereo) {$modes+= 'isolated-fallback'}
        if($RequireLateCartridge -and !$Stereo) {$modes+= 'late-fallback'}
        foreach($mode in $modes) {
            $env:STARFOX_DISABLE_GPU_BACKGROUND=if($mode -eq 'cpu-background') {'1'}else{$null}
            $env:STARFOX_DISABLE_GPU_LATE_CARTRIDGE=if($mode -eq 'cpu-background') {'1'}else{$null}
            $env:STARFOX_TEST_FAIL_BACKGROUND_GPU=if($mode -eq 'fallback') {'1'}else{$null}
            $env:STARFOX_TEST_FAIL_LATE_GPU=if($mode -eq 'late-fallback') {'1'}else{$null}
            $env:STARFOX_TEST_FAIL_ISOLATED_GPU=if($mode -eq 'isolated-fallback') {'1'}else{$null}
            $env:STARFOX_DISABLE_GPU_GEOMETRY=if($mode -eq 'cpu-models') {'1'}else{$null}
            $name="$experience-$level-$mode"
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$name.bmp"
            $env:STARFOX_CAPTURE_ISOLATED_PREFIX=if($CaptureIsolatedSources) {Join-Path $proof "$name-source-"}else{$null}
            $env:STARFOX_TEST_OVERLAY_DEBUG=if($CaptureIsolatedSources) {Join-Path $proof "$name-stage"}else{$null}
            $log=Join-Path $proof "$name.log"
            $process=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
            $handle=$process.Handle
            if(!$process.WaitForExit(60000)) {throw "Capture still running: PID $($process.Id), $log"}
            if($process.ExitCode -ne 0) {throw "Capture failed: $log"}
            if(Select-String -LiteralPath $log -SimpleMatch 'dlss-gameplay: evaluated' -Quiet) {
                throw "Background parity capture unexpectedly used temporal reconstruction: $log"
            }
            if($RendererCycle) {
                $cycles=@(Select-String -LiteralPath $log -Pattern '^renderer-cycle frame=')
                if($cycles.Count -ne [math]::Floor(($Frames-1)/8)) {throw "Missing renderer transitions: $log"}
                if($cycles[-1].Line -notmatch 'mode=GPU$') {throw "Did not return to GPU: $log"}
            }
            if($mode -eq 'gpu' -and !(Select-String -LiteralPath $log -SimpleMatch 'native-background: GPU resident ordered layers' -Quiet)) {
                throw "Scene did not exercise the resident background path: $log"
            }
            if($RequireIsolatedSources -and $mode -eq 'gpu' -and
                !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU resident isolated sources' -Quiet)) {
                throw "Scene did not exercise resident isolated sources: $log"
            }
            if($RequireHostInk -and $mode -eq 'gpu' -and
                !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU recorded host ink layer' -Quiet)) {
                throw "Scene did not exercise recorded host ink: $log"
            }
            if($RequireExLogoRepair -and $experience -eq 'EX' -and $mode -eq 'gpu' -and
                !(Select-String -LiteralPath $log -SimpleMatch 'native-background: GPU EX logo repair' -Quiet)) {
                throw "Scene did not exercise EX logo repair: $log"
            }
            if(($level -eq 'TITLEMAP' -or $RequireLateCartridge) -and $mode -eq 'gpu' -and
                !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU late cartridge layers' -Quiet)) {
                throw "Late cartridge foreground did not remain GPU resident: $log"
            }
            $hashes[$mode]=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH -Algorithm SHA256).Hash
        }
        foreach($mode in ($modes | Where-Object {$_ -ne 'gpu'})) {
            if($hashes[$mode] -ne $hashes.gpu) {throw "Background presentation differs: $experience $level $mode"}
        }
        Write-Output "$experience ${level}: $($modes -join ', ') captures byte-identical (stereo=$Stereo)"
    }}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
