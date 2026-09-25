param(
    [string]$Executable = 'build/current/starfox_pc.exe',
    [string]$OutputDirectory = 'tmp/gpu-native-check',
    [string]$Rom = 'upstream-ultrastarfox/SF.SFC',
    [string]$Symbols = 'upstream-ultrastarfox/SYMBOLS.TXT',
    [string]$Experience = 'ORIGINAL',
    [ValidateSet('vulkan','direct3d12','metal')][string]$GpuDriver = 'vulkan',
    [ValidatePattern('^[A-Za-z0-9_]+$')][string]$Level = 'LEVEL1_1',
    [int]$Ticks = 1000,
    [int]$Frames = 120,
    [int]$Warmup = 16,
    [ValidateSet(30,60,120,144,240)][int]$PresentationFps=60,
    [switch]$Resident,
    [switch]$DefaultPipeline,
    [switch]$Geometry,
    [switch]$RasterOnly,
    [switch]$RayTracing,
    [switch]$SeparatedModels,
    [switch]$PresentationCapture,
    [switch]$AllowUniformFinal,
    [switch]$Sequence,
    [switch]$AllowFallback,
    [switch]$CpuUploadTrace,
    [switch]$RequireNoCpuUpload,
    [switch]$Bomb,
    [switch]$ScrambleWipe,
    [ValidateRange(0,3)][int]$Bloom=2,
    [ValidateSet(1,2,4)][int[]]$Scales=@(1,2,4),
    [switch]$FpsOverlay,
    [ValidateSet('','Slot','Exit')][string]$PanelOverlay='',
    [ValidateRange(-1,255)][int]$Message=-1
)
$ErrorActionPreference = 'Stop'
# A fresh Corneria entry can still own the player during scripted launch, so
# X/A input there is not a reliable bomb fixture. Use the verified playable
# asteroid checkpoint unless the caller explicitly selects another scenario.
if($Bomb) {
    if(!$PSBoundParameters.ContainsKey('Level')) {$Level='LEVEL1_2'}
    if(!$PSBoundParameters.ContainsKey('Ticks')) {$Ticks=200}
    if(!$PSBoundParameters.ContainsKey('Frames')) {$Frames=360}
}
if($RasterOnly -and $Geometry) { throw 'RasterOnly and Geometry are mutually exclusive' }
if($Sequence -and (!$PresentationCapture -or $Frames -lt 1 -or $Frames -gt 240)) {
    throw 'Sequence requires PresentationCapture and 1..240 frames'
}
Remove-Item Env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE -ErrorAction SilentlyContinue
if($Sequence) {$env:STARFOX_CAPTURE_PRESENTATION_SEQUENCE='1'}
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$env:SDL_AUDIODRIVER = 'dummy'
$env:SDL_GPU_DRIVER = $GpuDriver
$env:STARFOX_TEST_HIDDEN = '1'
$env:STARFOX_TEST_FRAMES = "$Frames"
$env:STARFOX_TEST_PROFILE_WARMUP = "$Warmup"
$env:STARFOX_TEST_UNPACED = '1'
$env:STARFOX_TEST_PREROLL_TICKS = "$Ticks"
$env:STARFOX_TEST_EXPERIENCE = $Experience
$env:STARFOX_TEST_RENDERER = 'GPU'
$env:STARFOX_TEST_PRESENTATION_FPS = "$PresentationFps"
$env:STARFOX_TEST_SDL_GPU = '1'
$env:STARFOX_DISABLE_GPU_NATIVE = '1'
$env:STARFOX_TEST_VSYNC = '0'
$env:STARFOX_TEST_BLOOM = "$Bloom"
$env:STARFOX_TEST_DLSS_SELECTION = '0'
$env:STARFOX_TEST_FSR1_SELECTION = '0'
$env:STARFOX_TEST_SEPARATED_MODELS = if($SeparatedModels) {'1'} else {'0'}
$env:STARFOX_TEST_RAY_TRACING = if($RayTracing) {'1'} else {'0'}
# Baseline fixtures must not inherit persisted terrain/sky upgrades. Explicit
# diagnostic environment overrides still allow callers to test those features.
for($field=0;$field -lt 6;++$field) {
    $name="STARFOX_TEST_ENVIRONMENT_$field"
    if(!(Test-Path "Env:$name")) {Set-Item -LiteralPath "Env:$name" -Value '0'}
}
$env:STARFOX_TRACE_PROFILE = '1'
$env:STARFOX_TRACE_PROFILE_DISTRIBUTION = '1'
$env:STARFOX_TRACE_GPU = '1'
if($CpuUploadTrace -or $RequireNoCpuUpload) {$env:STARFOX_TRACE_GPU_CPU_UPLOAD='1'}
else {Remove-Item Env:STARFOX_TRACE_GPU_CPU_UPLOAD -ErrorAction SilentlyContinue}
Remove-Item Env:STARFOX_TEST_SCRAMBLE_WIPE -ErrorAction SilentlyContinue
if($ScrambleWipe) {$env:STARFOX_TEST_SCRAMBLE_WIPE='1'}
if($PresentationCapture) {
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    Add-Type -AssemblyName System.Drawing
}
if($Message -ge 0) {
    $env:STARFOX_TEST_MESSAGE="$Message"
    $env:STARFOX_TEST_DISPLAY_MODE='4_3'
    $env:STARFOX_TEST_PRESENTATION_FPS='60'
    $env:STARFOX_TEST_SKIP_PREROLL='1'
}
if($FpsOverlay) {
    if(!$DefaultPipeline -and !$Resident) {throw '-FpsOverlay requires a resident/default pipeline check'}
    $env:STARFOX_TEST_SHOW_FPS='1'
} else {$env:STARFOX_TEST_SHOW_FPS='0'}
if($PanelOverlay) {
    if(!$DefaultPipeline -and !$Resident) {throw '-PanelOverlay requires a resident/default pipeline check'}
    if(!$FpsOverlay) {$env:STARFOX_TEST_SHOW_FPS='0'}
    if($PanelOverlay -eq 'Slot') {$env:STARFOX_TEST_STATE_ACTIONS='1:3'}
    else {$env:STARFOX_TEST_EXIT_CONFIRMATION='1'}
}
if($Bomb) {
    if(!$DefaultPipeline -and !$Resident) {throw '-Bomb requires a resident/default pipeline check'}
    if($Frames -lt 36) {throw '-Bomb requires at least 36 frames to reach its scheduled input'}
    $env:STARFOX_TEST_PRESENTATION_FPS='60'
    $env:STARFOX_TEST_SKIP_PREROLL='1'
    $env:STARFOX_TEST_PRESSES='12:64,24:128'
    $env:STARFOX_TEST_PRESS_FRAMES='12'
}
# Message/bomb schedules intentionally use 60 FPS; label their actual cadence.
$effectiveFps=$env:STARFOX_TEST_PRESENTATION_FPS
foreach($scale in $Scales) {
    $env:STARFOX_TEST_RENDER_SCALE = "$scale"
    $hashes = @()
    $presentationHashes = @()
    $sequencePaths = @()
    foreach($mode in 'cpu','gpu') {
        Remove-Item Env:STARFOX_TEST_GPU_GEOMETRY -ErrorAction SilentlyContinue
        # DefaultPipeline must prove the ordinary path, without diagnostic opt-in.
        if($Geometry -and !$DefaultPipeline -and $mode -eq 'gpu') {$env:STARFOX_TEST_GPU_GEOMETRY='1'}
        Remove-Item Env:STARFOX_DISABLE_GPU_GEOMETRY -ErrorAction SilentlyContinue
        if($RasterOnly) {$env:STARFOX_DISABLE_GPU_GEOMETRY='1'}
        Remove-Item Env:STARFOX_TEST_GPU_NATIVE_PIPELINE -ErrorAction SilentlyContinue
        if($mode -eq 'gpu') { $env:STARFOX_TEST_GPU_RASTER='1' }
        else { Remove-Item Env:STARFOX_TEST_GPU_RASTER -ErrorAction SilentlyContinue }
        if($Resident -and $mode -eq 'gpu') { $env:STARFOX_TEST_GPU_NATIVE_PIPELINE='1' }
        if($DefaultPipeline) {
            Remove-Item Env:STARFOX_TEST_SDL_GPU -ErrorAction SilentlyContinue
            if(!$PSBoundParameters.ContainsKey('GpuDriver')) {
                Remove-Item Env:SDL_GPU_DRIVER -ErrorAction SilentlyContinue
            }
            Remove-Item Env:STARFOX_TEST_GPU_RASTER -ErrorAction SilentlyContinue
            Remove-Item Env:STARFOX_TEST_GPU_NATIVE_PIPELINE -ErrorAction SilentlyContinue
            if($mode -eq 'gpu') { Remove-Item Env:STARFOX_DISABLE_GPU_NATIVE -ErrorAction SilentlyContinue }
            else { $env:STARFOX_DISABLE_GPU_NATIVE='1' }
        }
        $stem = "$Experience-$Level-$Ticks-${effectiveFps}fps-${scale}x-$mode"
        $env:STARFOX_CAPTURE_PATH = Join-Path $outputPath "$stem.bmp"
        if($PresentationCapture) {
            $env:STARFOX_CAPTURE_PRESENTATION_PATH = Join-Path $outputPath "$stem-presentation.bmp"
            $sequencePaths += $env:STARFOX_CAPTURE_PRESENTATION_PATH
        }
        $log = Join-Path $outputPath "$stem.log"
        $process = Start-Process -FilePath $Executable -ArgumentList "`"$Rom`" `"$Symbols`" $Level" -WindowStyle Hidden -PassThru -RedirectStandardError $log
        # Retain the process handle before it exits: Windows PowerShell can
        # otherwise lose ExitCode for short-lived Start-Process children.
        $processHandle = $process.Handle
        if(-not $process.WaitForExit(60000)) { throw "Native raster check exceeded 60 seconds: $stem (PID $($process.Id))" }
        if($process.ExitCode -ne 0) { throw "Native raster check failed: $stem exit $($process.ExitCode)" }
        $required = if(($Resident -or $DefaultPipeline) -and $AllowFallback) {'native-raster: GPU resident'} elseif($Resident -or $DefaultPipeline) {'native-pipeline: resident raster'} else {'native-raster: GPU native scanlines'}
        if($mode -eq 'gpu' -and -not (Select-String -Path $log -Pattern $required -Quiet)) {
            throw "GPU raster was not exercised: $stem"
        }
        if($mode -eq 'gpu' -and ($Resident -or $DefaultPipeline) -and !$AllowFallback -and
            (Select-String -Path $log -Pattern 'CPU scene replay for transition/overlay|GPU scene readback for transition/overlay|GPU raster readback for CPU transition/overlay|replaying complete frame' -Quiet)) {
            throw "Resident GPU path used CPU replay/readback: $stem"
        }
        if($mode -eq 'gpu' -and $RequireNoCpuUpload) {
            $uploads=@(Select-String -LiteralPath $log -Pattern 'gpu-cpu-upload:.*bytes=([0-9]+)')
            if($uploads.Count -ne $Frames) {
                throw "Expected $Frames GPU CPU-upload records, found $($uploads.Count): $stem"
            }
            foreach($record in $uploads) {
                if([int]$record.Matches[0].Groups[1].Value -ne 0) {
                    throw "GPU frame uploaded a CPU image: $($record.Line.Trim()) ($stem)"
                }
            }
        }
        if($Geometry -and $mode -eq 'gpu') {
            if(!(Select-String -Path $log -Pattern 'native-geometry: GPU model batch resident' -Quiet)) {
                throw "Live GPU model geometry was not exercised: $stem"
            }
            if(Select-String -Path $log -Pattern 'replaying complete frame' -Quiet) {
                throw "Live GPU model geometry fell back: $stem"
            }
            if(!$AllowFallback -and (Select-String -Path $log -Pattern 'CPU scene replay for transition/overlay' -Quiet)) {
                throw "Live GPU model presentation used CPU scene replay: $stem"
            }
            if(!$AllowFallback -and (Select-String -Path $log -Pattern 'GPU scene readback for transition/overlay' -Quiet)) {
                throw "Live GPU model presentation used CPU composition: $stem"
            }
        }
        if($RayTracing -and $mode -eq 'gpu' -and ($Resident -or $DefaultPipeline) -and
            -not (Select-String -Path $log -Pattern 'shadow-backend: (Hardware DXR 1.1:|GPU-resident hardware DXR shadows)' -Quiet)) {
            throw "Hardware ray-traced shadows were not exercised: $stem"
        }
        if($RayTracing -and $mode -eq 'gpu' -and $GpuDriver -in @('direct3d12','vulkan') -and
            -not (Select-String -Path $log -Pattern 'shadow-backend: GPU-resident hardware DXR shadows' -Quiet)) {
            throw "$GpuDriver test did not exercise resident hardware shadows: $stem"
        }
        if($SeparatedModels -and $mode -eq 'gpu') {
            if(!(Select-String -Path $log -Pattern 'native-pipeline: GPU separated model layer' -Quiet)) {
                throw "Resident separated model layer was not exercised: $stem"
            }
            if(Select-String -Path $log -Pattern 'GPU raster readback for CPU transition/overlay' -Quiet) {
                throw "Separated model layer used CPU readback: $stem"
            }
        }
        $hashes += (Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PATH -Algorithm SHA256).Hash
        if($PresentationCapture) {
            $captureBitmap=[Drawing.Bitmap]::new($env:STARFOX_CAPTURE_PRESENTATION_PATH)
            try {
                $firstPixel=$captureBitmap.GetPixel(0,0).ToArgb()
                $varied=$false
                for($y=0;$y -lt $captureBitmap.Height -and !$varied;$y+=[Math]::Max(1,[int]($captureBitmap.Height/32))) {
                    for($x=0;$x -lt $captureBitmap.Width;$x+=[Math]::Max(1,[int]($captureBitmap.Width/32))) {
                        if($captureBitmap.GetPixel($x,$y).ToArgb() -ne $firstPixel) { $varied=$true; break }
                    }
                }
                if(!$varied -and !$AllowUniformFinal) { throw "Uniform final presentation capture: $stem" }
                if(!$varied -and $AllowUniformFinal) {
                    Write-Output "Explicit transition sample has uniform presentation: $stem"
                }
            } finally { $captureBitmap.Dispose() }
            $presentationHashes += (Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH -Algorithm SHA256).Hash
        }
        if($FpsOverlay -and $mode -eq 'gpu') {
            if(!(Select-String -Path $log -Pattern 'native-pipeline: GPU host FPS overlay' -Quiet)) {
                throw "GPU FPS overlay was not exercised: $stem"
            }
            if(Select-String -Path $log -Pattern 'GPU raster readback for CPU transition/overlay' -Quiet) {
                throw "FPS overlay used CPU composition: $stem"
            }
        }
        if($PanelOverlay -and $mode -eq 'gpu') {
            if(!(Select-String -Path $log -Pattern 'native-pipeline: GPU confirmation/slot overlay' -Quiet)) {
                throw "GPU panel overlay was not exercised: $stem"
            }
            if(Select-String -Path $log -Pattern 'GPU raster readback for CPU transition/overlay' -Quiet) {
                throw "Panel overlay used CPU composition: $stem"
            }
        }
        if($Bomb -and $mode -eq 'gpu' -and
            !(Select-String -Path $log -Pattern 'native-pipeline: GPU bomb colour disk' -Quiet)) {
            throw "GPU bomb disk was not exercised: $stem"
        }
        if($ScrambleWipe -and $mode -eq 'gpu' -and
            !(Select-String -Path $log -Pattern 'native-pipeline: GPU horizontal scramble wipe' -Quiet)) {
            throw "GPU horizontal scramble wipe was not exercised: $stem"
        }
        Get-Content -LiteralPath $log | Select-String 'render-profile-us|native-raster:'
    }
    if($hashes[0] -ne $hashes[1]) { throw "CPU/GPU capture mismatch: $Experience $Ticks ticks ${scale}x" }
    Write-Output "Exact native raster capture parity: $Experience $Level $Ticks ticks ${effectiveFps}fps ${scale}x"
    if($PresentationCapture) {
        if($Sequence) {
            for($frame=1;$frame -le $Frames;$frame++) {
                $left=Get-FileHash -LiteralPath ($sequencePaths[0]+".frame-$frame.bmp") -Algorithm SHA256
                $right=Get-FileHash -LiteralPath ($sequencePaths[1]+".frame-$frame.bmp") -Algorithm SHA256
                if($left.Hash -ne $right.Hash) {throw "CPU/GPU sequence mismatch: $Level ${scale}x frame $frame"}
            }
            Write-Output "Exact sequence parity: $Level ${scale}x, all $Frames frames"
        }
        if($presentationHashes[0] -ne $presentationHashes[1]) {
            throw "CPU/GPU final presentation mismatch: $Experience $Ticks ticks ${scale}x"
        }
        Write-Output "Exact final presentation parity: $Experience $Level $Ticks ticks ${effectiveFps}fps ${scale}x"
    }
}
