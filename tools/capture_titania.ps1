param([string]$OutputDirectory='tmp/titania-proof',
    [ValidateSet('4_3','16_9','32_9')][string]$DisplayMode='16_9',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU',
    [ValidateSet('water','corridor')][string]$Scene='water',
    [ValidateRange(1,3600)][int]$Frames=180,
    [string]$Executable='build/current/starfox_pc.exe')
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$savedEnvironment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $savedEnvironment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_SKIP_PREROLL='1'
$env:STARFOX_TEST_FRAMES=[string]$Frames
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_DISPLAY_MODE=$DisplayMode
$env:STARFOX_TEST_RENDER_SCALE='2'
$env:STARFOX_TEST_MSU1='0'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_RENDERER=$Renderer
$env:STARFOX_TEST_TIMING_MODE='ORIGINAL'
$env:STARFOX_TEST_RTX_LIGHTING='0'
$env:STARFOX_TEST_RAY_TRACING='0'
$env:STARFOX_TEST_SOFTWARE_SHADOWS='0'
$env:STARFOX_TEST_REFLECTIVE_SURFACES='0'
$env:STARFOX_TEST_ANTI_ALIASING='0'
$env:STARFOX_TEST_2D_FILTER='OFF'
$env:STARFOX_TEST_BLOOM='0'
$env:STARFOX_TEST_EFFECT='0'
$env:STARFOX_TEST_WORLD_EFFECT='0'
$env:STARFOX_TEST_MATERIAL='0'
$env:STARFOX_TEST_MANIPULATION='0'
$env:STARFOX_TEST_FSR1_SELECTION='0'
foreach($field in 0..5) {Set-Item -LiteralPath "Env:STARFOX_TEST_ENVIRONMENT_$field" -Value '0'}
$env:STARFOX_TEST_HDR_EFFECT='0'
$env:STARFOX_TEST_CHROMATIC_ABERRATION='0'
$env:STARFOX_TEST_MODEL_SMOOTHING='0'
$env:STARFOX_TRACE_RENDER_STATE='1'
$env:STARFOX_TEST_TITANIA_END='1'
if($Scene -eq 'corridor') {$env:STARFOX_TEST_TITANIA_CORRIDOR='1'}
$env:STARFOX_TEST_DLSS_SELECTION='0'
$env:STARFOX_TEST_BLOOM_2D='0'
$env:STARFOX_TEST_STEREO_OUTPUT='0'
$env:STARFOX_TEST_LANGUAGE='0'
$env:STARFOX_TEST_PREROLL_TICKS='200'
$env:STARFOX_CAPTURE_PATH=Join-Path $proofPath 'titania.bmp'
$env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofPath 'presentation.bmp'
$arguments=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL2_3'} else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL2_3'}
$process=Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proofPath/runtime.log"
$processHandle=$process.Handle # Keep the process handle alive for reliable ExitCode retrieval.
if(-not $process.WaitForExit(30000)) {throw "Capture still running: PID $($process.Id)"}
if($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PATH)) {throw "Capture failed; see runtime.log"}
if(!(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH)) {throw 'Final presentation capture missing'}
$renderState=Get-Content -LiteralPath "$proofPath/runtime.log" | Where-Object {$_ -like 'render-state *'} | Select-Object -Last 1
if($Scene -eq 'water' -and ($renderState -notmatch 'mode=1 ' -or $renderState -notmatch 'hofs=1(?: |$)' -or $renderState -notmatch 'tunnel=0 inatunnel=2(?: |$)')) {
    throw "Titania capture did not reach the authored Mode 1 water scanline background: $renderState"
}
# SETBGINFOREQ_L calls VOFSOFFPLEASE for this corridor's authored VOFF,
# changing the initial Mode 2 setup to Mode 1. Classify by tunnel ownership,
# not by the temporary graphics mode (water also uses Mode 1).
if($Scene -eq 'corridor' -and $renderState -notmatch 'tunnel=1 inatunnel=1(?: |$)') {
    throw "Titania capture did not reach the authored enclosed corridor: $renderState"
}
Write-Output "Captured Titania: $proofPath"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $savedEnvironment.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
