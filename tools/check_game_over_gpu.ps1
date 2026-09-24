param([string]$OutputDirectory='tmp/game-over-gpu-proof',
    [ValidateSet('16_9','32_9')][string]$DisplayMode='16_9',
    [ValidateRange(1,4)][int]$RenderScale=2,
    [ValidateRange(0,5)][int]$Filter=0,
    [ValidateRange(20,480)][int]$Fps=60,
    [switch]$PresentPacing,
    [switch]$EnhancedSky,
    [ValidateRange(0,600)][int]$PrerollTicks=30)
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='12'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_PREROLL_TICKS="$PrerollTicks"
        STARFOX_TEST_DISPLAY_MODE=$DisplayMode; STARFOX_TEST_RENDER_SCALE="$RenderScale"
        STARFOX_TEST_PRESENTATION_FPS="$Fps"; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
        STARFOX_TEST_SHOW_FPS='0'
        STARFOX_TEST_RENDERER='GPU'; STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_DLSS_SELECTION='0'
        STARFOX_TEST_2D_FILTER="$Filter"; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_FSR1_SELECTION='0'; STARFOX_TEST_SOFTWARE_SHADOWS='0'
        STARFOX_TEST_REFLECTIVE_SURFACES='0'; STARFOX_TEST_ANTI_ALIASING='0'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'; STARFOX_TEST_EFFECT='0'
        STARFOX_TEST_MATERIAL='0'; STARFOX_TEST_MANIPULATION='0'
        STARFOX_TEST_WORLD_EFFECT='0'; STARFOX_TEST_MODEL_SMOOTHING='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TRACE_GPU='1'
        STARFOX_TEST_ENVIRONMENT_0='0'; STARFOX_TEST_ENVIRONMENT_1='0'
        STARFOX_TEST_ENVIRONMENT_2='0'; STARFOX_TEST_ENVIRONMENT_3=[string][int]$EnhancedSky.IsPresent
        STARFOX_TEST_ENVIRONMENT_4='0'; STARFOX_TEST_ENVIRONMENT_5='0'
    }
    foreach($key in $settings.Keys) {[Environment]::SetEnvironmentVariable($key,$settings[$key],'Process')}
    if($PresentPacing) {
        Remove-Item Env:STARFOX_TEST_UNPACED
        $env:STARFOX_TEST_PRESENT_PACING='1'
        # Pacing is real; source phases are deterministic so pixel comparisons
        # across fresh processes don't compare different animation timestamps.
        $env:STARFOX_TEST_FIXED_RASTER='1'
    }
    foreach($experience in @('ORIGINAL','EX')) {
        $env:STARFOX_TEST_EXPERIENCE=$experience
        $arguments=if($experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt GAMEOVER'}
            else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT GAMEOVER'}
        $hashes=@{}
        $modes=@('gpu','cpu','software','native','fallback','stereo-fallback','half','full')
        if($EnhancedSky) {$modes+=@('sky-off')}
        foreach($mode in $modes) {
            Remove-Item Env:STARFOX_DISABLE_GPU_LATE_DUST,Env:STARFOX_TEST_FAIL_LATE_GPU,Env:STARFOX_TEST_STEREO_OUTPUT -ErrorAction SilentlyContinue
            $env:STARFOX_TEST_STEREO_OUTPUT='0'
            $env:STARFOX_TEST_ENVIRONMENT_3=if($EnhancedSky -and $mode -ne 'sky-off') {'1'} else {'0'}
            $env:STARFOX_TEST_DISPLAY_MODE=if($mode -eq 'native') {'4_3'} else {$DisplayMode}
            # 'cpu' only exercises the CPU late-star fallback. Also run the
            # complete software renderer so backend parity is actually tested.
            $env:STARFOX_TEST_RENDERER=if($mode -eq 'software') {'SOFTWARE'} else {'GPU'}
            if($mode -eq 'cpu') {$env:STARFOX_DISABLE_GPU_LATE_DUST='1'}
            if($mode -in @('fallback','stereo-fallback')) {$env:STARFOX_TEST_FAIL_LATE_GPU='1'}
            if($mode -eq 'stereo-fallback') {$env:STARFOX_TEST_STEREO_OUTPUT='2'}
            if($mode -eq 'half') {$env:STARFOX_TEST_STEREO_OUTPUT='1'}
            if($mode -eq 'full') {$env:STARFOX_TEST_STEREO_OUTPUT='2'}
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$experience-$mode.bmp"
            $log=Join-Path $proof "$experience-$mode.log"
            $process=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError $log
            $handle=$process.Handle
            if(!$process.WaitForExit(60000)) {throw "Capture still running: PID $($process.Id), $log"}
            if($process.ExitCode -ne 0) {throw "Capture failed: $log"}
            if($mode -eq 'gpu' -and !(Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU late margin stars' -Quiet)) {
                throw "Late GPU stars not selected: $log"
            }
            if($mode -eq 'software' -and (Select-String -LiteralPath $log -Pattern 'native-geometry: GPU|native-raster: GPU|native-pipeline: GPU late margin stars' -Quiet)) {
                throw "Software fixture unexpectedly selected native GPU rendering: $log"
            }
            if($mode -in @('fallback','stereo-fallback') -and
                ((Select-String -LiteralPath $log -SimpleMatch 'native-pipeline: GPU late margin stars' -Quiet) -or
                !(Select-String -LiteralPath $log -Pattern 'native-pipeline: (GPU scene readback|CPU scene replay)' -Quiet))) {
                throw "Late GPU failure did not execute complete fallback: $log"
            }
            if($mode -eq 'stereo-fallback' -and !(Select-String -LiteralPath $log -SimpleMatch 'stereo failure: eye effects:' -Quiet)) {
                throw "Stereo failure was not exercised: $log"
            }
            if($mode -in @('half','full') -and
                (!(Select-String -LiteralPath $log -SimpleMatch 'stereo presented:' -Quiet) -or
                 (Select-String -LiteralPath $log -SimpleMatch 'stereo failure:' -Quiet))) {
                throw "Successful stereo presentation was not exercised: $log"
            }
            $hashes[$mode]=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH -Algorithm SHA256).Hash
        }
        Add-Type -AssemblyName System.Drawing
        foreach($mode in @('cpu','software','fallback','stereo-fallback')) {
            if($hashes.gpu -eq $hashes[$mode]) {continue}
            if(!$EnhancedSky) {throw "Game Over GPU/$mode mismatch: $experience"}
            # The photograph's bilinear float sampler may round by one RGB
            # step between CPU and shader. Native indexed output stays exact.
            $reference=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-gpu.bmp"))
            $candidate=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-$mode.bmp"))
            try {
                if($candidate.Size -ne $reference.Size) {throw "Game Over dimensions differ: $experience $mode"}
                $different=0
                for($y=0;$y -lt $reference.Height;++$y) {for($x=0;$x -lt $reference.Width;++$x) {
                    $a=$reference.GetPixel($x,$y);$b=$candidate.GetPixel($x,$y)
                    $delta=[Math]::Max([Math]::Abs([int]$a.R-[int]$b.R),[Math]::Max([Math]::Abs([int]$a.G-[int]$b.G),[Math]::Abs([int]$a.B-[int]$b.B)))
                    if($delta -gt 1) {throw "Game Over GPU/$mode mismatch at ${x},${y}: $experience (delta $delta)"}
                    if($delta) {++$different}
                }}
                Write-Output "$experience $mode differs by one RGB step at $different pixels"
            } finally {$reference.Dispose();$candidate.Dispose()}
        }
        $bitmap=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-gpu.bmp"))
        try {
            if($EnhancedSky) {
                $original=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-sky-off.bmp"))
                try {
                    $changed=0
                    for($y=0;$y -lt $bitmap.Height;++$y) {for($x=0;$x -lt $bitmap.Width;++$x) {
                        $before=$original.GetPixel($x,$y).ToArgb() -band 0xffffff
                        $after=$bitmap.GetPixel($x,$y).ToArgb() -band 0xffffff
                        if($before -ne $after) {
                            if($before -ne 0) {throw "Enhanced Game Over changed authored character/star/text ink at ${x},${y}: $experience"}
                            ++$changed
                        }
                    }}
                    if(!$changed) {throw "Enhanced Game Over produced no visible sky change: $experience"}
                    Write-Output "$experience Enhanced Sky changes $changed blank pixels; authored ink remains exact"
                } finally {$original.Dispose()}
            }
            $left=($bitmap.Width-256*$RenderScale)/2
            $right=$bitmap.Width-$left
            $ink=@(0,0)
            for($y=0;$y -lt $bitmap.Height;++$y) {
                for($x=0;$x -lt $left;++$x) {if(($bitmap.GetPixel($x,$y).ToArgb() -band 0xffffff) -ne 0){++$ink[0]}}
                for($x=$right;$x -lt $bitmap.Width;++$x) {if(($bitmap.GetPixel($x,$y).ToArgb() -band 0xffffff) -ne 0){++$ink[1]}}
            }
            # The dense authored starfield must fill both margins. A handful
            # of moving dust pixels over an otherwise black border is not enough.
            $minimum=[Math]::Max(1,[Math]::Floor($left*$bitmap.Height/2000))
            if($ink[0] -lt $minimum -or $ink[1] -lt $minimum) {
                throw "Game Over starfield too sparse in margins: $experience ($($ink -join ','); minimum $minimum each)"
            }
            $native=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-native.bmp"))
            try {
                $nativeRasterWidth=256*$RenderScale
                $nativeDisplayWidth=[Math]::Floor(($native.Height*4+1)/3)
                if($native.Width -ne $nativeDisplayWidth -or $native.Height -ne $bitmap.Height) {
                    throw "Unexpected native Game Over reference dimensions: $experience"
                }
                for($y=0;$y -lt $native.Height;++$y) {
                    for($x=0;$x -lt $nativeRasterWidth;++$x) {
                        # Native output is stretched to 4:3 for presentation.
                        # Sample the known source-pixel center; no searched
                        # alignment or best-fit offset may hide a regression.
                        $nativeX=[Math]::Floor(($x+0.5)*$native.Width/$nativeRasterWidth)
                        if($native.GetPixel($nativeX,$y).ToArgb() -ne $bitmap.GetPixel($x+$left,$y).ToArgb()) {
                            throw "Wide Game Over changed its native center at ${x},${y}: $experience"
                        }
                    }
                }
            } finally {$native.Dispose()}
        } finally {$bitmap.Dispose()}
        $monoWidth=0
        $mono=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-gpu.bmp"))
        try {$monoWidth=$mono.Width; $monoHeight=$mono.Height} finally {$mono.Dispose()}
        foreach($stereoMode in @('half','full')) {
            $stereo=[Drawing.Bitmap]::FromFile((Join-Path $proof "$experience-$stereoMode.bmp"))
            try {
                $expectedWidth=if($stereoMode -eq 'full') {2*$monoWidth} else {$monoWidth}
                if($stereo.Width -ne $expectedWidth -or $stereo.Height -ne $monoHeight) {
                    throw "Wrong $stereoMode SBS dimensions: $experience ($($stereo.Width)x$($stereo.Height))"
                }
                $eyeWidth=$stereo.Width/2
                $nativeWidth=256*$RenderScale
                if($stereoMode -eq 'half') {$nativeWidth/=2}
                $margin=($eyeWidth-$nativeWidth)/2
                foreach($eye in @(0,1)) {
                    $marginInk=0
                    for($y=0;$y -lt $stereo.Height;++$y) {
                        for($x=0;$x -lt $eyeWidth;++$x) {
                            if($x -ge $margin -and $x -lt $eyeWidth-$margin) {continue}
                            if(($stereo.GetPixel([int]($eye*$eyeWidth+$x),$y).ToArgb() -band 0xffffff) -ne 0) {++$marginInk}
                        }
                    }
                    if(!$marginInk) {throw "No extended stars in $experience $stereoMode eye $eye"}
                }
            } finally {$stereo.Dispose()}
        }
        Write-Output "$experience Game Over: full software/GPU/fallback parity; native center unchanged; Half/Full SBS dimensions and both-eye margin stars pass"
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
