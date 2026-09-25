param(
    [string]$OutputDirectory='tmp/state-continuation',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [string]$Map='LEVEL1_1',
    [ValidateSet('direct3d12','vulkan')][string]$GpuDriver='direct3d12',
    [switch]$GodMode,
    [int]$Preroll=0,
    [ValidateRange(1,30000)][int]$SaveFrame=20,
    [ValidateRange(1,2000)][int]$CompareFrames=30,
    [switch]$Revival,
    [ValidateRange(1,9900)][int]$RevivalFrame=2400,
    [switch]$Msu,
    [switch]$FreshProcess,
    [switch]$ResumeAfterBaseline,
    [switch]$Presentation,
    [ValidateRange(-1,2)][int]$SkyMotion=-1
)
$ErrorActionPreference='Stop'
if($ResumeAfterBaseline -and !$FreshProcess) {throw 'ResumeAfterBaseline requires FreshProcess and a completed baseline from the same arguments'}
if($Revival) {
    if(!$PSBoundParameters.ContainsKey('SaveFrame')) {$SaveFrame=$RevivalFrame+20}
    if($SaveFrame -le $RevivalFrame) {throw 'Revival SaveFrame must follow RevivalFrame'}
}
if($Preroll -ne 0) {
    throw 'Audible continuation requires Preroll=0: fast preroll discards APU startup writes. Use a later SaveFrame to advance through normal audio-synchronized presentation frames.'
}
if($Msu -and $Experience -ne 'ORIGINAL') {
    throw 'MSU continuation is supported only for ORIGINAL; the runtime disables MSU for EX. Use the native-audio EX check instead.'
}
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$savedEnvironment=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $savedEnvironment[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
$env:SDL_GPU_DRIVER=$GpuDriver
$env:STARFOX_TEST_GOD_MODE=if($GodMode){'1'}else{'0'}
foreach($setting in @('BLOOM_2D','SOFTWARE_SHADOWS','REFLECTIVE_SURFACES','RTX_LIGHTING',
    'EFFECT','WORLD_EFFECT','HDR_EFFECT','CHROMATIC_ABERRATION','MODEL_SMOOTHING',
    'ANTI_ALIASING','2D_FILTER','DLSS_SELECTION','FSR1_SELECTION','STEREO_OUTPUT','LANGUAGE')) {
    Set-Item -LiteralPath "Env:STARFOX_TEST_$setting" -Value '0'
}
$env:SDL_AUDIODRIVER='dummy'
$env:STARFOX_TEST_HIDDEN='1'
$env:STARFOX_TEST_FRAMES='120'
$env:STARFOX_TEST_SKIP_PREROLL='1'
$env:STARFOX_TEST_EXPERIENCE=$Experience
$env:STARFOX_TEST_DISPLAY_MODE='16_9'
$env:STARFOX_TEST_PRESENTATION_FPS='60'
$env:STARFOX_TEST_UNPACED='1'
$env:STARFOX_TEST_VSYNC='0'
$env:STARFOX_TEST_MSU1=if($Msu) {'1'} else {'0'}
$env:STARFOX_TEST_AUDIO_SIGNATURES='1'
$env:STARFOX_TEST_RENDER_SCALE='1'
$env:STARFOX_TEST_RENDERER='GPU'
$env:STARFOX_TRACE_GPU=if($Presentation){'1'}else{$null}
$env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE=if($Presentation){'1'}else{$null}
$env:STARFOX_TEST_PREROLL_TICKS="$Preroll"
$env:STARFOX_TEST_BLOOM='0'
$env:STARFOX_TEST_ENHANCED_SHADOWS='0'
$env:STARFOX_TEST_RAY_TRACING='0'
for($field=0;$field -lt 6;$field++) {Set-Item "Env:STARFOX_TEST_ENVIRONMENT_$field" '0'}
if($SkyMotion -ge 0) {
    $env:STARFOX_TEST_ENVIRONMENT_3='1'
    $env:STARFOX_TEST_ENVIRONMENT_5=[string]$SkyMotion
}
$env:STARFOX_CAPTURE_INTERVAL='1'
Remove-Item Env:STARFOX_TEST_PRESSES,Env:STARFOX_TEST_REVIVAL,Env:STARFOX_TEST_REVIVAL_FRAME -ErrorAction SilentlyContinue
if($Revival) {$env:STARFOX_TEST_REVIVAL='1'}
$rom=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
$symbols=if($Experience -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
foreach($mode in 'baseline','restore') {
    Remove-Item Env:STARFOX_TEST_REVIVAL,Env:STARFOX_TEST_REVIVAL_FRAME -ErrorAction SilentlyContinue
    if($Revival -and ($mode -eq 'baseline' -or !$FreshProcess)) {
        $env:STARFOX_TEST_REVIVAL='1';$env:STARFOX_TEST_REVIVAL_FRAME="$RevivalFrame"
    }
    $runPath=Join-Path $proofPath $mode
    New-Item -ItemType Directory -Force -Path $runPath | Out-Null
    $env:STARFOX_CAPTURE_DIR=$runPath
    # Indexed comparisons need only the post-save window, not thousands of
    # warm-up images. Presentation sequences retain their separate numbering.
    $env:STARFOX_CAPTURE_START=if($FreshProcess -and $mode -eq 'restore') {'20'}else{"$SaveFrame"}
    $env:STARFOX_CAPTURE_PRESENTATION_PATH=if($Presentation){Join-Path $runPath 'presentation.bmp'}else{$null}
    $env:STARFOX_TEST_STATE_DIRECTORY=if($FreshProcess) {Join-Path $proofPath 'baseline/slots'} else {Join-Path $runPath 'slots'}
    $env:STARFOX_TEST_FRAMES=if($FreshProcess -and $mode -eq 'restore') {"$($CompareFrames+30)"} else {"$($SaveFrame+$CompareFrames+70)"}
    $env:STARFOX_TEST_STATE_ACTIONS=if($mode -eq 'baseline') {"${SaveFrame}:1"} elseif($FreshProcess) {'20:2'} else {"${SaveFrame}:1,$($SaveFrame+60):2"}
    if($mode -eq 'baseline' -and $ResumeAfterBaseline) {
        $lastCapture=if($Presentation) {Join-Path $runPath "presentation.bmp.frame-$($env:STARFOX_TEST_FRAMES).bmp"} else {Join-Path $runPath ('{0:D6}.bmp' -f ([int]$env:STARFOX_TEST_FRAMES-1))}
        if(!(Test-Path -LiteralPath $lastCapture)) {throw 'Completed baseline final capture is missing; do not restart a still-running baseline'}
    } else {
    $process=Start-Process -FilePath build/current/starfox_pc.exe -ArgumentList "`"$rom`" `"$symbols`" $Map" -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $runPath 'runtime.log')
    # Keep the native process handle while the fast hidden run exits. Windows
    # PowerShell otherwise sometimes loses ExitCode and reports a null failure.
    $processHandle=$process.Handle
    $completed=$false
    for($wait=0;$wait -lt 3 -and !$completed;++$wait) {
        $completed=$process.WaitForExit(60000)
        if(!$completed){Write-Output "Still running: $mode PID $($process.Id)"}
    }
    if(!$completed) {throw "Still running: $mode PID $($process.Id)"}
    if($process.ExitCode -ne 0) {throw "$mode exited with $($process.ExitCode)"}
    }
    $log=Get-Content (Join-Path $runPath 'runtime.log') -Raw
    if($Revival -and ($mode -eq 'baseline' -or !$FreshProcess) -and
        $log -notmatch "scheduled-revival frame=$RevivalFrame(?:\r|\n)") {throw "$mode did not exercise scheduled revival"}
    if($Presentation -and ($log -notmatch 'native-raster: GPU resident' -or
        $log -match 'CPU scene replay for transition/overlay composition|GPU scene readback for transition/overlay composition')) {
        throw "$mode did not preserve the resident GPU presentation path"
    }
    $mustSave=$mode -eq 'baseline' -or !$FreshProcess
    if(($mustSave -and $log -notmatch 'state saved slot=0') -or $log -match 'state operation failed' -or ($mode -eq 'restore' -and $log -notmatch 'state loaded slot=0')) {throw "$mode did not execute state actions"}
    if($FreshProcess -and $mode -eq 'restore' -and $log -match 'state saved slot=0') {throw 'Fresh-process restore unexpectedly saved over its input'}
}
# Skip the immediate interpolation snap, then compare the requested advancing frames.
$restoreStart=if($FreshProcess) {30} else {$SaveFrame+70}
for($frame=$restoreStart;$frame -lt ($restoreStart+$CompareFrames);++$frame) {
    $referenceFrame=$frame-$restoreStart+$SaveFrame+10
    $expected=Join-Path $proofPath ('baseline/{0:D6}.bmp' -f $referenceFrame)
    $actual=Join-Path $proofPath ('restore/{0:D6}.bmp' -f $frame)
    if($Presentation) {
        $expected=Join-Path $proofPath "baseline/presentation.bmp.frame-$referenceFrame.bmp"
        $actual=Join-Path $proofPath "restore/presentation.bmp.frame-$frame.bmp"
    }
    if((Get-FileHash -LiteralPath $expected).Hash -ne (Get-FileHash -LiteralPath $actual).Hash) {
        throw "Restored continuation mismatch: output frame $frame vs reference $referenceFrame"
    }
}
$baselineLog=Get-Content (Join-Path $proofPath 'baseline/runtime.log') -Raw
$restoreLog=Get-Content (Join-Path $proofPath 'restore/runtime.log') -Raw
$baselineAudio=@([regex]::Matches(($baselineLog -split 'state saved slot=0',2)[1],'audio-frame:[^\r\n]+') | ForEach-Object {$_.Value})
$restoredAudio=@([regex]::Matches(($restoreLog -split 'state loaded slot=0',2)[1],'audio-frame:[^\r\n]+') | ForEach-Object {$_.Value})
if($restoredAudio.Count -lt 8 -or $baselineAudio.Count -lt $restoredAudio.Count) {throw 'Insufficient post-restore audio blocks'}
for($block=0;$block -lt $restoredAudio.Count;++$block) {
    if($baselineAudio[$block] -ne $restoredAudio[$block]) {throw "Restored PCM/track mismatch at block $block"}
}
if(!($restoredAudio -match 'silent=0')) {throw 'Continuation produced only silent audio; not an audible-state proof'}
if($Msu -and $baselineLog -notmatch 'msu-playing=1') {throw 'MSU playback was not exercised'}
Write-Output "$CompareFrames restored frames and $($restoredAudio.Count) PCM block signatures match: $Experience $Map Revival=$Revival MSU=$Msu FreshProcess=$FreshProcess SaveFrame=$SaveFrame Presentation=$Presentation"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($entry in $savedEnvironment.GetEnumerator()){Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
