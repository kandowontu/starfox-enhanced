param([string]$OutputDirectory='tmp/dlss-game-lifecycle-fixed',[string]$Executable='build/current/starfox_pc.exe',[switch]$Evaluate,[switch]$CompareSerialized,[switch]$RequireContinuousHistory,[switch]$CaptureBackground,[switch]$DumpPpu,[switch]$AuditTerrain,
    [ValidateRange(1,10000)][int]$Frames=16,[ValidateRange(20,1000)][int]$PresentationFPS=60,[ValidateRange(1,4)][int]$RenderScale=2,
    [ValidatePattern('^[A-Za-z0-9_]+$')][string]$Level='LEVEL1_1',
    [ValidateSet('ORIGINAL','EX')][string[]]$Experiences=@('ORIGINAL','EX'),
    [switch]$TestNeuralControl,
    [string]$Presses='',
    [ValidateSet('','0','1')][string]$NeuralSelection='',
    [ValidateSet('DLAA','QUALITY','BALANCED','PERFORMANCE')][string]$DlssMode='DLAA',[switch]$NativeRaster,[switch]$Jitter,[switch]$MenuSelection,[switch]$InstalledRuntime,[switch]$RequireNeural,[switch]$CaptureSequence,[switch]$NoJitter,[switch]$RendererCycle,[switch]$ExpectNoEvaluation)
if($ExpectNoEvaluation -and (!$Evaluate -or !$NativeRaster -or $RequireNeural -or $CompareSerialized -or $RequireContinuousHistory -or $AuditTerrain -or $Jitter)) {
    throw 'Unevaluated viewport check requires native raster preparation and no evaluation-only assertions'
}
if($RendererCycle -and (!$Evaluate -or !$MenuSelection -or $Frames -lt 17 -or [math]::Floor(($Frames-1)/8)%2 -ne 0 -or $RequireContinuousHistory)) {
    throw 'Renderer cycle requires evaluated menu selection, ending on GPU after at least 17 frames, without continuous-history assertion'
}
if($NoJitter -and $Jitter){throw 'Choose jitter or no jitter, not both'}
if($RequireNeural -and !$Evaluate){throw 'Neural verification requires Evaluate'}
if($MenuSelection -and !$Evaluate){throw 'Menu selection requires Evaluate'}
if($CompareSerialized -and !$Evaluate){throw 'Serialization comparison requires Evaluate'}
if($Jitter -and (!$Evaluate -or !$NativeRaster)){throw 'Jitter requires native raster evaluation'}
if($AuditTerrain -and (!$Evaluate -or $Frames -lt 2)){throw 'Terrain audit requires Evaluate and at least two frames'}
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $proof){throw 'Use a new proof directory'}
New-Item -ItemType Directory -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value;Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    if($TestNeuralControl){$env:STARFOX_TEST_DLSS5_CONTROL='1'}
    if($RendererCycle){$env:STARFOX_TEST_RENDERER_CYCLE='1'}
    $settings=@{
        SDL_GPU_DRIVER='direct3d12';SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES="$Frames"
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_PREROLL_TICKS='1000';STARFOX_TEST_RENDERER='GPU'
        STARFOX_TEST_UNPACED='1';STARFOX_TEST_MSU1='0';STARFOX_TEST_TEMPORAL_INPUTS='1';STARFOX_TRACE_GPU='1'
        STARFOX_TEST_DISPLAY_MODE='16_9';STARFOX_TEST_RENDER_SCALE="$RenderScale";STARFOX_TEST_PRESENTATION_FPS="$PresentationFPS"
        STARFOX_TEST_TIMING_MODE='ORIGINAL';STARFOX_TEST_VSYNC='0';STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_STEREO_OUTPUT='0'
        STARFOX_TEST_RTX_LIGHTING='0';STARFOX_TEST_HDR_EFFECT='0';STARFOX_TEST_BLOOM='0';STARFOX_TEST_WORLD_EFFECT='0';STARFOX_TEST_EFFECT='0'
        STARFOX_TEST_DLSS_MODE=$DlssMode
        STARFOX_DLSS_ADAPTER=(Resolve-Path build/streamline-probe-msvc/starfox_dlss_native.dll).Path
        STARFOX_DLSS_BINARIES=(Resolve-Path tmp/streamline-sdk-2.14.1/sdk/bin/x64).Path
    }
    foreach($key in $settings.Keys){[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    if($Presses){$env:STARFOX_TEST_PRESSES=$Presses;$env:STARFOX_TEST_PRESS_FRAMES='3'}
    foreach($experience in $Experiences) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX'){"tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt $Level"}
            else{"upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT $Level"}
        $hashes=@{}
        $modes=if($CompareSerialized){@('off','on','serialized')}else{@('off','on')}
        foreach($mode in $modes) {
            $env:STARFOX_TEST_NEURAL_SELECTION=if($mode -ne 'off' -and $NeuralSelection -ne ''){$NeuralSelection}else{$null}
            # Explicit installed runtime paths now enable capability discovery
            # without diagnostic switches. The off control must omit them.
            $env:STARFOX_DLSS_ADAPTER=if($mode -eq 'off'){$null}else{$settings.STARFOX_DLSS_ADAPTER}
            $env:STARFOX_DLSS_BINARIES=if($mode -eq 'off'){$null}else{$settings.STARFOX_DLSS_BINARIES}
            if($InstalledRuntime) {
                foreach($name in @('SDL_GPU_DRIVER','STARFOX_DLSS_ADAPTER','STARFOX_DLSS_BINARIES')) {
                    Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
                }
            }
            $env:STARFOX_TEST_DLSS_LIFECYCLE=if($mode -ne 'off'){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_EVALUATE=if($mode -ne 'off' -and $Evaluate){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_SERIALIZE=if($mode -eq 'serialized'){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_NATIVE_RASTER=if($NativeRaster -and $mode -ne 'off'){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_JITTER=if($Jitter -and $mode -ne 'off'){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_SELECTION='0'
            if($MenuSelection) {
                $env:STARFOX_TEST_DLSS_SELECTION=if($mode -eq 'off'){'0'}else{[string](@{QUALITY=1;BALANCED=2;PERFORMANCE=3;DLAA=4}[$DlssMode])}
                foreach($name in @('STARFOX_TEST_DLSS_LIFECYCLE','STARFOX_TEST_DLSS_EVALUATE','STARFOX_TEST_DLSS_NATIVE_RASTER','STARFOX_TEST_DLSS_JITTER','STARFOX_TEST_DLSS_MODE','STARFOX_TEST_TEMPORAL_INPUTS')) {
                    Remove-Item -LiteralPath "Env:$name" -ErrorAction SilentlyContinue
                }
            }
            if($NoJitter){$env:STARFOX_TEST_DLSS_JITTER='0'}
            if($CaptureSequence){$env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE='1'}
            $env:STARFOX_TEST_DLSS_AUDIT_TERRAIN=if($AuditTerrain -and $mode -ne 'off'){'1'}else{$null}
            $env:STARFOX_TEST_DLSS_WORLD_CAPTURE=if($mode -eq 'on' -and $Evaluate){Join-Path $proof "$experience-world.bmp"}else{$null}
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$experience-$mode.bmp"
            $env:STARFOX_CAPTURE_TITLE_LAYERS=if($CaptureBackground){Join-Path $proof "$experience-$mode"}else{$null}
            $env:STARFOX_TEST_PPU_DUMP=if($DumpPpu){'1'}else{$null}
            $env:STARFOX_CAPTURE_DIR=if($DumpPpu){Join-Path $proof "$experience-$mode-ppu"}else{$null}
            $env:STARFOX_CAPTURE_START=if($DumpPpu){[string]($Frames-1)}else{$null}
            $env:STARFOX_CAPTURE_INTERVAL=if($DumpPpu){[string]$Frames}else{$null}
            $log=Join-Path $proof "$experience-$mode.log"
            $p=Start-Process $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log -RedirectStandardOutput (Join-Path $proof "$experience-$mode-stdout.log")
            $handle=$p.Handle
            if(!$p.WaitForExit(60000)){throw "Still running PID $($p.Id): $log"}
            if($p.ExitCode -ne 0){throw "Game failed ($($p.ExitCode)): $log"}
            $addonLog=Join-Path (Split-Path ([IO.Path]::GetFullPath($Executable))) 'ReShade.log'
            if(Test-Path -LiteralPath $addonLog) {
                $savedAddonLog=Join-Path $proof "$experience-$mode-addon.log"
                Copy-Item -LiteralPath $addonLog -Destination $savedAddonLog
                if($RequireNeural -and $mode -ne 'off' -and
                    !(Select-String -LiteralPath $savedAddonLog -SimpleMatch 'inline feature 18 evaluation succeeded' -Quiet)) {
                    throw "Neural add-on did not evaluate gameplay: $savedAddonLog"
                }
            } elseif($RequireNeural -and $mode -ne 'off') {throw 'Neural add-on log missing'}
            if($InstalledRuntime -and $mode -eq 'off' -and
                (Select-String -LiteralPath $log -SimpleMatch 'dlss-presentation: upgraded' -Quiet)) {
                throw "DLSS OFF wrapped the native swapchain: $log"
            }
            if($mode -ne 'off') {
                foreach($message in @('initialized before SDL','actual game GPU bound','shutdown before renderer destruction','dlss-presentation: upgraded','dlss-presentation: restored')) {
                    if(!(Select-String -LiteralPath $log -SimpleMatch $message -Quiet)){throw "Missing $message in $log"}
                }
                if($ExpectNoEvaluation -and (!(Select-String -LiteralPath $log -SimpleMatch 'viewport configured' -Quiet) -or
                    (Select-String -LiteralPath $log -Pattern 'dlss-gameplay: evaluated|Release DLSS viewport:|dlss-sdk-error:' -Quiet))) {
                    throw "Expected safely configured but unevaluated viewport: $log"
                }
                if($Evaluate -and !$ExpectNoEvaluation -and (!(Select-String -LiteralPath $log -SimpleMatch 'dlss-gameplay: evaluated' -Quiet) -or
                    (Select-String -LiteralPath $log -Pattern 'dlss-gameplay: failed|dlss-sdk-error:' -Quiet))) {throw "Gameplay evaluation failed/missing: $log"}
                if($Evaluate -and !$ExpectNoEvaluation) {
                    $expectedMode=@{QUALITY=1;BALANCED=2;PERFORMANCE=3;DLAA=4}[$DlssMode]
                    if(!(Select-String -LiteralPath $log -Pattern " mode=$expectedMode render=[1-9][0-9]*x[1-9][0-9]*" -Quiet)) {
                        throw "Requested DLSS mode was not evaluated: $log"
                    }
                    if($AuditTerrain -and !(Select-String -LiteralPath $log -Pattern 'dlss-terrain-audit: covered=[1-9][0-9]* valid_depth=[1-9][0-9]*' -Quiet)) {
                        throw "Missing usable terrain depth: $log"
                    }
                    if($AuditTerrain -and !(Select-String -LiteralPath $log -Pattern 'valid_motion=[1-9][0-9]* moving=[0-9]+ mismatches=0' -Quiet)) {
                        throw "Missing verified terrain motion: $log"
                    }
                    $evaluations=@(Select-String -LiteralPath $log -SimpleMatch 'dlss-gameplay: evaluated')
                    if($Jitter -and $Frames -gt 1) {
                        $phases=@($evaluations | ForEach-Object {if($_.Line -match ' jitter=([^ ]+)'){$Matches[1]}} | Sort-Object -Unique)
                        if($phases.Count -lt 2){throw "Missing changing raster jitter: $log"}
                    }
                    if($NativeRaster) {
                        foreach($evaluation in $evaluations) {
                            if($evaluation.Line -notmatch 'size=(\d+x\d+).*render=(\d+x\d+)' -or $Matches[1] -ne $Matches[2]) {
                                throw "DLSS did not receive native render dimensions: $log"
                            }
                        }
                    }
                    $expectedFrames=$Frames
                    if($Presses) {
                        $pausedFrames=@(Select-String -LiteralPath $log -Pattern '^temporal-paused-native: frame=(\d+)$' | ForEach-Object {[int]$_.Matches[0].Groups[1].Value})
                        if(!$pausedFrames.Count){throw "Scripted pause did not reach native paused presentation: $log"}
                        # DLSS logs its evaluation count, not presentation
                        # serial; after a pause those indices intentionally differ.
                        $expectedFrames-=$pausedFrames.Count
                    }
                    if($RendererCycle) {
                        $expectedFrames=@(0..($Frames-1) | Where-Object {[math]::Floor($_/8)%2 -eq 0}).Count
                        $restarts=[math]::Floor(($Frames-1)/16)
                        if(@(Select-String -LiteralPath $log -SimpleMatch 'restarted before renderer creation').Count -ne $restarts) {
                            throw "Missing SDK restart after renderer switch: $log"
                        }
                        # SDL also recreates swapchains while resizing. Require
                        # a fresh upgrade and evaluation in each restart segment,
                        # not an exact total of swapchain creations.
                        $segments=(Get-Content -LiteralPath $log -Raw) -split 'dlss-lifecycle: restarted before renderer creation'
                        foreach($segment in $segments) {
                            if($segment -notmatch 'dlss-presentation: upgraded' -or $segment -notmatch 'dlss-gameplay: evaluated frame=0 reset=1') {
                                throw "Replacement renderer did not resume reconstruction: $log"
                            }
                        }
                        if(@(Select-String -LiteralPath $log -SimpleMatch 'dlss-presentation: upgraded').Count -lt ($restarts+1)) {
                            throw "Missing replacement swapchain upgrade: $log"
                        }
                    }
                    if($evaluations.Count -ne $expectedFrames){throw "Expected $expectedFrames evaluations, got $($evaluations.Count): $log"}
                    if($RequireContinuousHistory -and @($evaluations | Where-Object {$_.Line -match 'reset=1'}).Count -ne 1){
                        throw "Expected only initial history reset in stable scene: $log"
                    }
                }
            }
            $hashes[$mode]=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH).Hash
        }
        if(!$Evaluate -and $hashes.off -ne $hashes.on){throw "SDK lifecycle changed gameplay: $experience"}
        if($Evaluate -and !$ExpectNoEvaluation -and $hashes.off -eq $hashes.on){throw "Evaluated output was not displayed: $experience"}
        if($CompareSerialized -and $hashes.on -ne $hashes.serialized){throw "Queue-ordered/serialized output differs: $experience"}
        "${experience}: SDK startup/binding/shutdown passed; evaluated=$($Evaluate -and !$ExpectNoEvaluation)"
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
