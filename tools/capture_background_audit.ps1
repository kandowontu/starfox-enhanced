param(
    [string]$OutputDirectory='tmp/background-audit',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [string[]]$Levels=@(),
    [ValidateRange(0,8000)][int[]]$Ticks=@(1000,6000),
    [string[]]$Aspects=@('4_3','16_9','32_9'),
    [string]$Executable='build/current/starfox_pc.exe',
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU',
    [switch]$LegacyD3D11,
    [switch]$DisableGpuEffects,
    [switch]$TraceGpu,
    [switch]$Layers,
    [switch]$PpuSnapshot,
    [switch]$TraceObjects,
    [switch]$ModelLayers,
    [switch]$SourceFrame,
    [switch]$FinalTarget,
    [ValidateRange(0,30000)][int]$PresentationInterval=0,
    [switch]$EnhancedSky,
    [switch]$EnhancedGround,
    [switch]$UnbatchedTerrain,
    [switch]$ArmadaApproach,
    [switch]$GodMode,
    [switch]$AudioPreroll,
    [ValidateRange(1,30000)][int]$Frames=1,
    [ValidateRange(20,240)][int]$PresentationFps=60,
    [ValidateSet('ORIGINAL','UNLOCKED')][string]$TimingMode='ORIGINAL',
    [ValidateRange(1,300)][int]$WaitSeconds=60,
    [string]$Presses='',
    [ValidateRange(1,240)][int]$PressFrames=3,
    [ValidateRange(0,30000)][int]$CaptureStart=0,
    [ValidateRange(0,30000)][int]$CaptureInterval=0,
    [switch]$IncludeSpecialRoutes
)
$ErrorActionPreference='Stop'
if($PresentationInterval -and !$FinalTarget) {throw 'PresentationInterval requires FinalTarget'}
if($AudioPreroll -and $TimingMode -ne 'ORIGINAL') {
    throw 'AudioPreroll currently requires ORIGINAL source timing (one 50 ms audio frame per logic tick)'
}
if ($Presses -and $Presses -notmatch '^(?:[0-9]+|0x[0-9a-fA-F]+):(?:[0-9]+|0x[0-9a-fA-F]+)(?:,(?:[0-9]+|0x[0-9a-fA-F]+):(?:[0-9]+|0x[0-9a-fA-F]+))*$') {
    throw 'Presses must be frame:numeric-button-mask pairs (for example 1200:64 for X/boost), not button names'
}
$auditPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $auditPath | Out-Null
$rom=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
$symbols=if($Experience -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
$availableLevels=@(Get-Content -LiteralPath $symbols | ForEach-Object {
    if($_ -match '^(LEVEL(?:[1-7]_[1-9]|_BLACKHOLE|_SPECIAL|_COMET)|GAMEOVER|INTROMAP|TITLEMAP|CONTMAP)\s') {$Matches[1]}
} | Sort-Object -Unique)
# GAMEOVER is an application-supported front-end entry, not a ROM symbol.
$availableLevels+=@('GAMEOVER')
if(!$Levels.Count) {
    $Levels=@($availableLevels | Where-Object {$_ -match '^LEVEL' -and ($IncludeSpecialRoutes -or $_ -match '^LEVEL[1-7]_[1-9]$')})
}
if(!$Levels.Count) {throw 'No level entry points found'}
foreach($level in $Levels) {
    if($level -notin $availableLevels) {throw "Unknown background entry for ${Experience}: $level"}
}
# Isolate this fixture from other diagnostic hooks in the invoking shell.
$savedEnvironment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $savedEnvironment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $env:SDL_AUDIODRIVER='dummy'
    $env:STARFOX_TEST_HIDDEN='1'
    $env:STARFOX_TEST_FRAMES=[string]$Frames
    $env:STARFOX_TEST_GOD_MODE=if($GodMode) {'1'} else {'0'}
    if($AudioPreroll) {$env:STARFOX_TEST_PREROLL_AUDIO='1'}
    if($Presses) {
        $env:STARFOX_TEST_PRESSES=$Presses
        $env:STARFOX_TEST_PRESS_FRAMES=[string]$PressFrames
    }
    if($ArmadaApproach) {
        if($Levels.Count -ne 1 -or $Levels[0] -ne 'LEVEL1_3') {throw 'Armada approach requires LEVEL1_3 only'}
        $env:STARFOX_TEST_ARMADA_APPROACH='1'
    }
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    $env:STARFOX_TEST_UNPACED='1'
    $env:STARFOX_TEST_EXPERIENCE=$Experience
    $env:STARFOX_TEST_RENDERER=$Renderer
    if($LegacyD3D11) {$env:STARFOX_TEST_D3D11_GPU='1'}
    if($DisableGpuEffects) {$env:STARFOX_DISABLE_GPU_EFFECTS='1'}
    if($TraceGpu) {$env:STARFOX_TRACE_GPU='1'}
    $env:STARFOX_TEST_RENDER_SCALE='1'
    $env:STARFOX_TEST_ANTI_ALIASING='0'
    $env:STARFOX_TEST_SEPARATED_MODELS='0'
    $env:STARFOX_TEST_PRESENTATION_FPS=[string]$PresentationFps
    $env:STARFOX_TEST_TIMING_MODE=$TimingMode
    $env:STARFOX_TEST_SHOW_FPS='0'
    if($PresentationInterval) {
        $env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE='1'
        $env:STARFOX_CAPTURE_PRESENTATION_INTERVAL=[string]$PresentationInterval
    }
    if($SourceFrame) {$env:STARFOX_TEST_SOURCE_FRAME='1'}
    $env:STARFOX_TEST_BLOOM='0'
    $env:STARFOX_TEST_BLOOM_2D='0'
    $env:STARFOX_TEST_FSR1_SELECTION='0'
    $env:STARFOX_TEST_DLSS_SELECTION='0'
    $env:STARFOX_TEST_STEREO_OUTPUT='0'
    $env:STARFOX_TEST_RTX_LIGHTING='0'
    $env:STARFOX_TEST_2D_FILTER='0'
    $env:STARFOX_TEST_LANGUAGE='0'
    $env:STARFOX_TEST_EFFECT='0'
    $env:STARFOX_TEST_WORLD_EFFECT='0'
    $env:STARFOX_TEST_MATERIAL='0'
    $env:STARFOX_TEST_MANIPULATION='0'
    foreach($field in 0..5) {Set-Item -LiteralPath "Env:STARFOX_TEST_ENVIRONMENT_$field" -Value '0'}
    if($EnhancedSky){$env:STARFOX_TEST_ENVIRONMENT_3='1'}
    if($EnhancedGround){$env:STARFOX_TEST_ENVIRONMENT_0='1'}
    if($UnbatchedTerrain){$env:STARFOX_TEST_UNBATCHED_TERRAIN='1'}
    $env:STARFOX_TEST_MODEL_SMOOTHING='0'
    $env:STARFOX_TEST_HDR_EFFECT='0'
    $env:STARFOX_TEST_CHROMATIC_ABERRATION='0'
    $env:STARFOX_TEST_MSU1='0'
    $env:STARFOX_TEST_VSYNC='0'
    $env:STARFOX_TEST_ENHANCED_SHADOWS='0'
    $env:STARFOX_TEST_RAY_TRACING='0'
    $env:STARFOX_TEST_SOFTWARE_SHADOWS='0'
    $env:STARFOX_TEST_REFLECTIVE_SURFACES='0'
    # Record the reached flow and background ID, not just the requested level.
    # A direct-entry sample can already be at its boss or another background.
    $env:STARFOX_TRACE_RENDER_STATE='1'
    if($TraceObjects) {$env:STARFOX_TRACE_OBJECTS='1'}
    foreach($level in $Levels) {
        foreach($tick in $Ticks) {
            if($tick -lt 0) {throw 'Negative preroll'}
            foreach($aspect in $Aspects) {
                if($aspect -notin @('4_3','16_9','32_9')) {throw "Unsupported audit aspect: $aspect"}
                $stem="$Experience-$level-$tick-$aspect"
                $env:STARFOX_TEST_PREROLL_TICKS="$tick"
                $env:STARFOX_TEST_DISPLAY_MODE=$aspect
                $env:STARFOX_CAPTURE_PATH=Join-Path $auditPath "$stem.bmp"
                if($CaptureInterval -gt 0) {
                    # Sparse sequence captures avoid one full bitmap per frame.
                    $env:STARFOX_CAPTURE_DIR=Join-Path $auditPath "$stem-sequence"
                    $env:STARFOX_CAPTURE_START=[string]$CaptureStart
                    $env:STARFOX_CAPTURE_INTERVAL=[string]$CaptureInterval
                }
                if($FinalTarget) {$env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $auditPath "$stem-final.bmp"}
                if($Layers) {$env:STARFOX_CAPTURE_TITLE_LAYERS=Join-Path $auditPath "$stem-layer"}
                if($ModelLayers) {
                    $env:STARFOX_CAPTURE_MODEL_LAYERS=Join-Path $auditPath "$stem-model"
                    $env:STARFOX_TRACE_FINAL_MODEL_POSES='1'
                }
                if($PpuSnapshot) {
                    $env:STARFOX_TEST_PPU_DUMP='1'
                    $env:STARFOX_CAPTURE_DIR=Join-Path $auditPath "$stem-snapshot"
                    # A snapshot request should not dump every preceding frame's
                    # VRAM and bitmap during a long natural-route replay.
                    if($CaptureInterval -eq 0) {
                        $env:STARFOX_CAPTURE_START=[string]($Frames-1)
                        $env:STARFOX_CAPTURE_INTERVAL='1'
                    }
                }
                $process=Start-Process -FilePath $Executable -ArgumentList "`"$rom`" `"$symbols`" $level" -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $auditPath "$stem.log")
                $captureProcessHandle=$process.Handle
                $waitClock=[Diagnostics.Stopwatch]::StartNew()
                while (!$process.WaitForExit([Math]::Min(30000,$WaitSeconds*1000))) {
                    if($waitClock.Elapsed.TotalSeconds -ge $WaitSeconds) {
                        throw "Capture still running: $stem, PID $($process.Id); inspect before restarting"
                    }
                    Write-Output "Capture running: $stem, PID $($process.Id)"
                }
                if($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PATH)) {throw "Capture failed: $stem"}
                if($FinalTarget -and !(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH)) {throw "Final capture missing: $stem"}
                Write-Output "Captured $stem"
            }
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $savedEnvironment.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
