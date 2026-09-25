param([string]$Executable='build/current/starfox_pc.exe',
 [string]$OutputDirectory='tmp/native-defaults-profile',
 [ValidateRange(120,10000)][int]$Frames=360,
 [ValidateSet(60,120,240)][int]$Fps=60,
 [switch]$Capture,
 [switch]$Visible,
 [switch]$Paced,
 [switch]$PresentPacing,
 [switch]$Vsync,
 [ValidateSet('default','direct3d12','vulkan')][string]$GpuDriver='default',
 [switch]$DisableDlssRuntime,
 [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
 [ValidateSet('SOFTWARE','GPU')][string[]]$Renderers=@('SOFTWARE','GPU'),
 [ValidateSet('LEVEL1_1','LEVEL2_3')][string[]]$Levels=@('LEVEL1_1','LEVEL2_3'),
 [switch]$RealAudio,
 [switch]$EnhancedGround,
 [switch]$EnhancedSky,
 [switch]$UnbatchedTerrain,
 [switch]$GodMode,
 [ValidateRange(0,1000000)][int]$SlowFrameUs=0)
$ErrorActionPreference='Stop'
if($Vsync -and !$Visible){throw 'VSync measurements require Visible; hidden swapchains can be heavily throttled.'}
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $proof -Force | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
 $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
 $settings=@{
  SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";
  STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000';STARFOX_TEST_UNPACED='1';
  STARFOX_TEST_PRESENTATION_FPS="$Fps";STARFOX_TEST_TIMING_MODE='ORIGINAL';
  STARFOX_TEST_DISPLAY_MODE='4_3';STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0';
  STARFOX_TRACE_PROFILE='1';STARFOX_TRACE_PROFILE_DISTRIBUTION='1';STARFOX_TEST_PROFILE_WARMUP='60'
 }
 foreach($name in @('MSU1','ENHANCED','SEPARATED_MODELS','ANTI_ALIASING','2D_FILTER',
  'RTX_LIGHTING','BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','MODEL_SMOOTHING',
  'HDR_EFFECT','CHROMATIC_ABERRATION','RAY_TRACING','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES',
  'DLSS_SELECTION','FSR1_SELECTION','NEURAL_SELECTION','STEREO_OUTPUT','LANGUAGE',
  'MANIPULATION','MATERIAL')) {$settings["STARFOX_TEST_$name"]='0'}
 # Baseline runs must not inherit persisted terrain/sky upgrades from the menu.
 for($field=0;$field -lt 6;++$field){$settings["STARFOX_TEST_ENVIRONMENT_$field"]='0'}
 if($EnhancedGround){$settings.STARFOX_TEST_ENVIRONMENT_0='1'}
 if($EnhancedSky){$settings.STARFOX_TEST_ENVIRONMENT_3='1'}
 if($UnbatchedTerrain){$settings.STARFOX_TEST_UNBATCHED_TERRAIN='1'}
 if($SlowFrameUs){$settings.STARFOX_TRACE_SLOW_FRAME_US=[string]$SlowFrameUs}
 $settings.STARFOX_TEST_GOD_MODE=if($GodMode){'1'}else{'0'}
 if($Visible) {$settings.Remove('STARFOX_TEST_HIDDEN')}
 if($GpuDriver -ne 'default'){$settings.SDL_GPU_DRIVER=$GpuDriver}
 if($DisableDlssRuntime) {
  # Exercise the optional-runtime failure path without renaming installed DLLs.
  $settings.STARFOX_DLSS_ADAPTER=Join-Path $proof 'intentionally-unavailable-adapter.dll'
  $settings.STARFOX_DLSS_BINARIES=$proof
 }
 if($Vsync){$settings.STARFOX_TEST_VSYNC='1'}
 if($Paced) {$settings.Remove('STARFOX_TEST_UNPACED')}
 if($PresentPacing) {
  if(!$Paced){throw 'PresentPacing requires Paced'}
  $settings.STARFOX_TEST_PRESENT_PACING='1'
 }
 if($RealAudio) {$settings.Remove('SDL_AUDIODRIVER')}
 foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
 foreach($experience in $Experiences) {foreach($level in $Levels) {foreach($renderer in $Renderers) {
  $env:STARFOX_TEST_EXPERIENCE=$experience;$env:STARFOX_TEST_RENDERER=$renderer
  $name="$experience-$level-$renderer";$log=Join-Path $proof "$name.log"
  # Readback/BMP encoding stalls the last frame; keep visual checks separate
  # from timing runs unless explicitly requested.
  if($Capture) {$env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$name.bmp"}
  $arguments=if($experience -eq 'EX') {"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $level"}
   else {"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $level"}
  $process=Start-Process $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
  $handle=$process.Handle
  $timeoutMs=[Math]::Max(60000,[int](1000*$Frames/$Fps)+60000)
  if(!$process.WaitForExit($timeoutMs)) {throw "Benchmark still running: PID $($process.Id), log $log"}
  if($process.ExitCode -ne 0) {throw "Benchmark failed: $name"}
  $summary=@(Get-Content -LiteralPath $log | Where-Object {$_ -match '^(logic/audio|frame-work|present-interval|input-to-present|render)-distribution-us|^render-profile-us|^presentation-pacing:'})
  if($summary.Count -ne 7) {throw "Incomplete profiling metrics: $name"}
  Write-Output $name;Write-Output $summary
 }}}
} finally {
 Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
 foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
