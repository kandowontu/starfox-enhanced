param([string]$OutputDirectory='tmp/reflections-live', [int]$Frames=30,
    [ValidateRange(0,2)][int]$Stereo=0,[ValidateRange(0,32)][int]$Effect=0,
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL',
    [ValidatePattern('^[A-Z0-9_]+$')][string]$Level='LEVEL1_1',
    [ValidateRange(0,100000)][int]$PrerollTicks=1000,[switch]$ExColourWarp,
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU',
    [ValidateRange(1,3)][int]$Quality=3,[switch]$SoftwareShadows,
    [string]$Binary='build/current/starfox_pc.exe')
$ErrorActionPreference='Stop'
if($ExColourWarp -and $Experience -ne 'EX') {throw 'ExColourWarp requires EX'}
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $proof -Force | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames";
        STARFOX_TEST_PREROLL_TICKS="$PrerollTicks";STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_EXPERIENCE=$Experience;
        STARFOX_TEST_RENDERER=$Renderer;STARFOX_TEST_DISPLAY_MODE='16_9';STARFOX_TEST_TIMING_MODE='ORIGINAL';
        STARFOX_TEST_PRESENTATION_FPS='60';STARFOX_TEST_UNPACED='1';STARFOX_TEST_VSYNC='0';
        STARFOX_TEST_RENDER_SCALE='2';STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_STEREO_OUTPUT="$Stereo";
        STARFOX_TEST_ANTI_ALIASING='0';STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_2D_FILTER='0';
        STARFOX_TEST_BLOOM='0';STARFOX_TEST_BLOOM_2D='0';STARFOX_TEST_EFFECT=$(if($Effect -in @(27,28,29,30)) {'0'} else {"$Effect"});STARFOX_TEST_WORLD_EFFECT='0';
        STARFOX_TEST_MATERIAL=$(if($Effect -in @(27,28,29,30)) {"$Effect"} else {'0'});STARFOX_TEST_MANIPULATION='0';
        STARFOX_TEST_MODEL_SMOOTHING='0';STARFOX_TEST_HDR_EFFECT='0';STARFOX_TEST_CHROMATIC_ABERRATION='0';
        STARFOX_TEST_RAY_TRACING=$(if($Renderer -eq 'GPU') {'1'} else {'0'});
        STARFOX_TEST_SOFTWARE_SHADOWS=$(if($SoftwareShadows) {'1'} else {'0'});
        STARFOX_TRACE_GPU_RAYS='1';STARFOX_TRACE_GPU='1';STARFOX_TRACE_PROFILE='1'
    }
    foreach($entry in $settings.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
    if($ExColourWarp) {$env:STARFOX_TEST_EX_COLOR_WARP='1'}
    foreach($reflectionStrength in 0,$Quality) {
        $env:STARFOX_TEST_REFLECTIVE_SURFACES="$reflectionStrength"
        $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$reflectionStrength.bmp"
        $log=Join-Path $proof "$reflectionStrength.log"
        $cartridgeArguments=if($Experience -eq 'EX') {
            "tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $Level"
        } else { "upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $Level" }
        $process=Start-Process $Binary -ArgumentList $cartridgeArguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
        $handle=$process.Handle
        if(!$process.WaitForExit(60000)) {throw "Still running: PID $($process.Id), $log"}
        if($process.ExitCode -ne 0) {throw "Capture failed: $log"}
        $expectedBackend=if($Renderer -eq 'GPU') {'reflection-scene: GPU resident'} else {'reflection-scene: CPU single bounce'}
        if($reflectionStrength -ne 0 -and !(Select-String -LiteralPath $log -SimpleMatch $expectedBackend -Quiet)) {
            throw "Live reflections did not execute: $log"
        }
        if($Renderer -eq 'GPU' -and $reflectionStrength -ne 0 -and !(Select-String -LiteralPath $log -SimpleMatch 'background=authored BG2' -Quiet)) {
            throw "Reflections used the flat fallback instead of authored background tiles: $log"
        }
        if($Renderer -eq 'GPU' -and $reflectionStrength -ne 0 -and $ExColourWarp -and !(Select-String -LiteralPath $log -SimpleMatch 'colour-warp=1' -Quiet)) {
            throw "Captured frame did not retain EX colour warp: $log"
        }
    }
    if((Get-FileHash -LiteralPath (Join-Path $proof '0.bmp')).Hash -eq
        (Get-FileHash -LiteralPath (Join-Path $proof "$Quality.bmp")).Hash) {throw 'Reflection toggle did not change the captured frame'}
    Write-Output "Live reflection captures differ: $proof"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($entry in $saved.GetEnumerator()) {Set-Item -LiteralPath "Env:$($entry.Key)" -Value $entry.Value}
}
