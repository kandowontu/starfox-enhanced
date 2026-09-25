param([string]$OutputDirectory='tmp/boss-roll-wipe-check', [switch]$GpuAudit,
    [ValidateSet(1,2,4)][int]$RenderScale=1, [int[]]$FrameCounts=@(1,451),
    [int]$TraceStart=-1)
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proofPath | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
Add-Type -AssemblyName System.Drawing
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_UNPACED='1'
        STARFOX_TEST_EXPERIENCE='ORIGINAL'; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_RENDER_SCALE="$RenderScale"
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_SHOW_FPS='0'
        STARFOX_TEST_ENDING='1'; STARFOX_TEST_ENDING_PREROLL='1800'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TEST_SOFTWARE_SHADOWS='0'; STARFOX_TEST_REFLECTIVE_SURFACES='0'
        STARFOX_TEST_ANTI_ALIASING='0'; STARFOX_TEST_2D_FILTER='0'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_STEREO_OUTPUT='0'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
        STARFOX_TRACE_RENDER_STATE='1'
    }
    foreach($key in $settings.Keys) {Set-Item "Env:$key" $settings[$key]}
    if($GpuAudit) {
        $env:STARFOX_TRACE_GPU='1'
        $env:STARFOX_TRACE_GPU_CPU_UPLOAD='1'
    }
    foreach($renderer in @('SOFTWARE','GPU')) {
        $env:STARFOX_TEST_RENDERER=$renderer
        foreach($frames in $FrameCounts) {
            $stem="$renderer-$frames"
            $env:STARFOX_TEST_FRAMES=[string]$frames
            $env:STARFOX_CAPTURE_DIR=$proofPath
            $env:STARFOX_CAPTURE_START=[string]$(if($TraceStart -ge 0){$TraceStart}else{$frames-1})
            $env:STARFOX_CAPTURE_INTERVAL=[string]$(if($TraceStart -ge 0){1}else{$frames})
            $env:STARFOX_CAPTURE_PATH=Join-Path $proofPath "$stem.bmp"
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofPath "$stem-final.bmp"
            $log=Join-Path $proofPath "$stem.log"
            $process=Start-Process build/current/starfox_pc.exe -WindowStyle Hidden -PassThru `
                -ArgumentList 'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_6' `
                -RedirectStandardError $log
            $processHandle=$process.Handle
            if(!$process.WaitForExit(60000)) {throw "Boss-roll capture still running: PID $($process.Id); inspect before restarting"}
            if($process.ExitCode -ne 0) {throw "Boss-roll capture failed: $log"}
            if(!(Select-String -LiteralPath $log -Pattern 'render-state flow=13 ' -Quiet)) {throw 'Did not capture the credits sequence'}
            if($frames -eq 451 -and !(Select-String -LiteralPath $log -Pattern 'boss=1 wipe=1/85/128,239 ' -Quiet)) {
                throw "Boss roll lost the cartridge's circular window state: $log"
            }
            if($GpuAudit -and $renderer -eq 'GPU') {
                $fallback=@(Select-String -LiteralPath $log -Pattern 'GPU scene readback for transition|GPU raster readback for CPU transition|CPU scene replay')
                $uploads=@(Select-String -LiteralPath $log -Pattern 'gpu-cpu-upload:')
                $nonzero=@($uploads | Where-Object {$_.Line -notmatch 'bytes=0$'})
                Write-Output "$stem GPU audit: fallback=$($fallback.Count) CPU-image=$($nonzero.Count)/$($uploads.Count) frames"
                $stripes=@(Select-String -LiteralPath $log -Pattern 'gpu-cpu-stripes:')
                if($fallback.Count -ne 0 -or $uploads.Count -ne $frames -or $nonzero.Count -ne 0 -or $stripes.Count -ne $frames) {
                    throw "Unexpected GPU boss-roll migration path: $log"
                }
            }
            $bitmap=[Drawing.Bitmap]::FromFile($env:STARFOX_CAPTURE_PRESENTATION_PATH)
            try {
                $ink=0
                for($y=0;$y -lt $bitmap.Height;++$y) {for($x=0;$x -lt $bitmap.Width;++$x) {
                    $pixel=$bitmap.GetPixel($x,$y)
                    if($pixel.R -or $pixel.G -or $pixel.B) {$ink++}
                }}
                if($frames -eq 1 -and $ink -lt 1000) {throw 'Boss dossier is missing before the wipe'}
                # Source M_WINB logic $55 opens the next dossier through a
                # partial round window here. The old all-black capture was
                # missing that logic snapshot, not a correct closed frame.
                if($frames -eq 451 -and ($ink -lt 10000 -or $ink -gt 15000)) {
                    throw "Boss-roll circular aperture has unexpected coverage: $ink pixels"
                }
                Write-Output "$stem final target: $ink nonblack pixels"
            } finally {$bitmap.Dispose()}
        }
    }
    if($GpuAudit) {foreach($frames in $FrameCounts) {
        $software=(Get-FileHash -LiteralPath (Join-Path $proofPath "SOFTWARE-$frames-final.bmp") -Algorithm SHA256).Hash
        $gpu=(Get-FileHash -LiteralPath (Join-Path $proofPath "GPU-$frames-final.bmp") -Algorithm SHA256).Hash
        if($software -ne $gpu) {throw "Boss-roll GPU/Software final image mismatch at frame $frames"}
    }}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {Set-Item "Env:$key" $saved[$key]}
}
