param(
    [string]$Executable='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/ex-menu-backgrounds',
    [int[]]$Choices=@(0..36)+@(99),
    [switch]$EnhancedSky,
    [switch]$EnhancedGround,
    [switch]$PpuSnapshot,
    [switch]$ReferencePhase,
    [ValidateRange(1,600)][int]$Frames=1,
    [ValidateRange(60,240)][int]$PresentationFps=60,
    [string]$Presses='',
    [ValidateRange(1,120)][int]$PressFrames=12,
    [ValidateSet('4_3','16_9','32_9')][string]$DisplayMode='16_9',
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU'
)
$ErrorActionPreference='Stop'
if($Presses -and $Presses -notmatch '^(?:[0-9]+|0x[0-9a-fA-F]+):(?:[0-9]+|0x[0-9a-fA-F]+)(?:,(?:[0-9]+|0x[0-9a-fA-F]+):(?:[0-9]+|0x[0-9a-fA-F]+))*$') {
    throw 'Presses must be frame:numeric-button-mask pairs'
}
$outputPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $env:SDL_AUDIODRIVER='dummy'
    $env:SDL_GPU_DRIVER='direct3d12'
    $env:STARFOX_TEST_HIDDEN='1'
    $env:STARFOX_TEST_FRAMES=[string]$Frames
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    $env:STARFOX_TEST_UNPACED='1'
    $env:STARFOX_TEST_EXPERIENCE='EX'
    $env:STARFOX_TEST_RENDERER=$Renderer
    $env:STARFOX_TEST_RENDER_SCALE='1'
    $env:STARFOX_TEST_DISPLAY_MODE=$DisplayMode
    if($ReferencePhase){$env:STARFOX_TEST_SOURCE_FRAME='1'}
    $env:STARFOX_TEST_PRESENTATION_FPS=[string]$PresentationFps
    if($Presses) {$env:STARFOX_TEST_PRESSES=$Presses;$env:STARFOX_TEST_PRESS_FRAMES=[string]$PressFrames}
    $env:STARFOX_TEST_TIMING_MODE='UNLOCKED'
    foreach($name in @('ANTI_ALIASING','SEPARATED_MODELS','BLOOM','BLOOM_2D','FSR1_SELECTION','DLSS_SELECTION',
        'STEREO_OUTPUT','RTX_LIGHTING','2D_FILTER','LANGUAGE','EFFECT','WORLD_EFFECT','MATERIAL','MANIPULATION',
        'MODEL_SMOOTHING','HDR_EFFECT','CHROMATIC_ABERRATION','MSU1','VSYNC','ENHANCED_SHADOWS','RAY_TRACING',
        'SOFTWARE_SHADOWS','REFLECTIVE_SURFACES','SHOW_FPS')) {
        Set-Item -LiteralPath "Env:STARFOX_TEST_$name" -Value '0'
    }
    foreach($field in 0..5) {Set-Item -LiteralPath "Env:STARFOX_TEST_ENVIRONMENT_$field" -Value '0'}
    $env:STARFOX_TEST_ENVIRONMENT_3=if($EnhancedSky){'1'}else{'0'}
    $env:STARFOX_TEST_ENVIRONMENT_0=if($EnhancedGround){'1'}else{'0'}
    $env:STARFOX_TRACE_RENDER_STATE='1'
    foreach($choice in $Choices) {
        if(($choice -lt 0 -or $choice -gt 36) -and $choice -ne 99) {throw "Invalid choice $choice"}
        $env:STARFOX_TEST_EX_MENU_BACKGROUND=[string]$choice
        if($ReferencePhase) {
            $reference=@(Import-Csv tests/data/ex_menu_reference_offsets.csv | Where-Object {[int]$_.choice -eq $choice})
            if($reference.Count -ne 1){throw "Missing unique reference record for $choice"}
            $env:STARFOX_TEST_EX_MENU_SCROLL_X=$reference[0].ppu_x_at_capture
        }
        $env:STARFOX_CAPTURE_PATH=Join-Path $outputPath "choice-$choice.bmp"
        $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $outputPath "choice-$choice-final.bmp"
        if($PpuSnapshot) {
            $env:STARFOX_TEST_PPU_DUMP='1'
            $env:STARFOX_CAPTURE_DIR=Join-Path $outputPath "choice-$choice-snapshot"
            $env:STARFOX_CAPTURE_START='0';$env:STARFOX_CAPTURE_INTERVAL='1'
            $env:STARFOX_CAPTURE_TITLE_LAYERS=Join-Path $outputPath "choice-$choice-layer"
        }
        $process=Start-Process -FilePath $Executable -ArgumentList 'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt TITLEMAP' -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $outputPath "choice-$choice.log")
        $captureHandle=$process.Handle
        if(!$process.WaitForExit(60000)) {throw "Capture still running: choice $choice, PID $($process.Id); inspect before restarting"}
        if($process.ExitCode -ne 0) {throw "EX menu capture failed: $choice (exit $($process.ExitCode))"}
        if(!(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH)) {throw "Missing final capture: $choice"}
        Write-Output "Captured EX menu choice $choice"
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($name in $saved.Keys) {Set-Item -LiteralPath "Env:$name" -Value $saved[$name]}
}
