param(
    [string]$OutputDirectory='tmp/presentation-proof',
    [string]$Experience='ORIGINAL',
    [int]$Frames=48,
    [string]$Entry='LEVEL1_1',
    [int]$Preroll=1000,
    [string]$Presses='',
    [ValidateRange(1,240)][int]$PressFrames=3,
    [int]$Fps=60,
    [int]$Message=-1,
    [string]$DisplayMode='16_9',
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU',
    [ValidateSet('direct3d12','vulkan')][string]$GpuDriver='direct3d12',
    [ValidateRange(0,63)][int]$ModelEffect=0,
    [ValidateRange(0,63)][int]$WorldEffect=0,
    [ValidateRange(0,63)][int]$Material=0,
    [ValidateRange(0,60)][int]$Manipulation=0,
    [switch]$EnhancedGround,
    [switch]$EnhancedSky,
    [ValidateRange(0,7)][int]$GroundMaterial=0,
    [ValidateRange(0,3)][int]$GroundMotion=0,
    [ValidateRange(0,3)][int]$SkyStyle=0,
    [ValidateRange(0,2)][int]$SkyMotion=0,
    [ValidateRange(0,5)][int]$Language=0,
    [ValidateRange(0,4)][int]$Fsr1Mode=0,
    [switch]$RayTracing,
    [ValidateRange(0,3)][int]$Reflections=0,
    [switch]$CaptureNative,
    [switch]$FinalTarget,
    [switch]$LegacyGpuEffects,
    [switch]$Meter,
    [switch]$UpgradeFlash,
    [switch]$ScrambleWipe
)
$ErrorActionPreference='Stop'
if($WorldEffect -in @(46,47)) {throw 'Trails and Long Exposure are model-only effects'}
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$savedEnvironment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $savedEnvironment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
$env:SDL_AUDIODRIVER='dummy'
$env:SDL_GPU_DRIVER=$GpuDriver
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_RENDERER=$Renderer
if($LegacyGpuEffects) {
    if($Renderer -ne 'GPU') {throw 'Legacy GPU effects require the GPU renderer'}
    $env:STARFOX_DISABLE_GPU_NATIVE='1'
    $env:STARFOX_TRACE_GPU='1'
}
if($FinalTarget) {
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofPath 'presentation.bmp'
}
$env:STARFOX_TEST_FRAMES="$Frames"
$env:STARFOX_TEST_PREROLL_TICKS="$Preroll"
if($Presses) {$env:STARFOX_TEST_PRESSES=$Presses}
$env:STARFOX_TEST_PRESS_FRAMES="$PressFrames"
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_DISPLAY_MODE=$DisplayMode
$env:STARFOX_TEST_TIMING_MODE='ORIGINAL'
$env:STARFOX_TEST_PRESENTATION_FPS="$Fps"
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_BLOOM='0'
$env:STARFOX_TEST_ENHANCED_SHADOWS='0'
$env:STARFOX_TEST_RAY_TRACING=if($RayTracing){'1'}else{'0'}
$env:STARFOX_TEST_SOFTWARE_SHADOWS='0'
$env:STARFOX_TEST_REFLECTIVE_SURFACES="$Reflections"
$env:STARFOX_TEST_RTX_LIGHTING='0'
$env:STARFOX_TEST_BLOOM_2D='0'
$env:STARFOX_TEST_EFFECT="$ModelEffect"
$env:STARFOX_TEST_MATERIAL="$Material"
$env:STARFOX_TEST_MANIPULATION="$Manipulation"
$env:STARFOX_TEST_ENVIRONMENT_0=([int]$EnhancedGround.IsPresent).ToString()
$env:STARFOX_TEST_ENVIRONMENT_1="$GroundMaterial"
$env:STARFOX_TEST_ENVIRONMENT_2="$GroundMotion"
$env:STARFOX_TEST_ENVIRONMENT_3=([int]$EnhancedSky.IsPresent).ToString()
$env:STARFOX_TEST_ENVIRONMENT_4="$SkyStyle"
$env:STARFOX_TEST_ENVIRONMENT_5="$SkyMotion"
$env:STARFOX_TEST_WORLD_EFFECT="$WorldEffect"
$env:STARFOX_TEST_HDR_EFFECT='0'
$env:STARFOX_TEST_CHROMATIC_ABERRATION='0'
$env:STARFOX_TEST_MODEL_SMOOTHING='0'
$env:STARFOX_TEST_ANTI_ALIASING='0'
$env:STARFOX_TEST_2D_FILTER='0'
$env:STARFOX_TEST_DLSS_SELECTION='0'
$env:STARFOX_TEST_FSR1_SELECTION="$Fsr1Mode"
if($Fsr1Mode) {$env:STARFOX_TRACE_GPU='1'}
if($CaptureNative) {$env:STARFOX_TEST_FSR1_NATIVE_CAPTURE=Join-Path $proofPath 'native.bmp'}
$env:STARFOX_TEST_STEREO_OUTPUT='0'
$env:STARFOX_TEST_LANGUAGE="$Language"
if(!$FinalTarget) {
    $env:STARFOX_CAPTURE_DIR=$proofPath
    $env:STARFOX_CAPTURE_INTERVAL='1'
}
Remove-Item Env:STARFOX_TEST_MESSAGE,Env:STARFOX_TEST_SCRAMBLE_WIPE,Env:STARFOX_TEST_MESSAGE_METER,Env:STARFOX_TEST_UPGRADE_FLASH -ErrorAction SilentlyContinue
if($Message -ge 0) {$env:STARFOX_TEST_MESSAGE="$Message"}
if($ScrambleWipe) {$env:STARFOX_TEST_SCRAMBLE_WIPE='1'}
if($Meter) {$env:STARFOX_TEST_MESSAGE_METER='1'}
if($UpgradeFlash) {$env:STARFOX_TEST_UPGRADE_FLASH='1'}
$arguments=if($Experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $Entry"} else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $Entry"}
$proofProcess=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
$proofProcessHandle=$proofProcess.Handle
if(-not $proofProcess.WaitForExit(60000)) {throw "Capture still running (PID $($proofProcess.Id))"}
if($proofProcess.ExitCode -ne 0) {throw "Capture failed: exit $($proofProcess.ExitCode); see runtime.log"}
if($FinalTarget -and !(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH)) {
    throw 'Final presentation target was not captured'
}
Write-Output "Captured $Frames source-driven presentation frames in $proofPath"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $savedEnvironment.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
