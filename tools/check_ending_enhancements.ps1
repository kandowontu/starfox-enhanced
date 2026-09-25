param([string]$OutputDirectory='tmp/ending-enhancements')
$ErrorActionPreference='Stop'
$proofPath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force $proofPath | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='1'
        STARFOX_TEST_SKIP_PREROLL='1'; STARFOX_TEST_UNPACED='1'
        STARFOX_TEST_EXPERIENCE='ORIGINAL'; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_DISPLAY_MODE='32_9'; STARFOX_TEST_RENDER_SCALE='1'
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_SHOW_FPS='0'; STARFOX_TEST_RENDERER='GPU'
        STARFOX_TEST_ENDING='1'; STARFOX_TEST_ENDING_PREROLL='6000'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'
        STARFOX_TEST_EFFECT='0'; STARFOX_TEST_WORLD_EFFECT='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TEST_RTX_LIGHTING='0'; STARFOX_TEST_RAY_TRACING='0'
        STARFOX_TEST_SOFTWARE_SHADOWS='0'; STARFOX_TEST_REFLECTIVE_SURFACES='0'
        STARFOX_TEST_ANTI_ALIASING='0'; STARFOX_TEST_2D_FILTER='0'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_STEREO_OUTPUT='0'; STARFOX_TEST_VSYNC='0'; STARFOX_TEST_MSU1='0'
    }
    foreach($case in @('OFF','LIGHT','RAY','REFLECTION','ALL')) {
        foreach($key in $settings.Keys) {Set-Item "Env:$key" $settings[$key]}
        if($case -in @('LIGHT','ALL')) {$env:STARFOX_TEST_RTX_LIGHTING='3'}
        if($case -in @('RAY','REFLECTION','ALL')) {$env:STARFOX_TEST_RAY_TRACING='1'}
        if($case -in @('REFLECTION','ALL')) {$env:STARFOX_TEST_REFLECTIVE_SURFACES='2'}
        $env:STARFOX_CAPTURE_PATH=Join-Path $proofPath "$case.bmp"
        $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proofPath "$case-final.bmp"
        $process=Start-Process build/current/starfox_pc.exe -WindowStyle Hidden -PassThru `
            -ArgumentList 'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_6' `
            -RedirectStandardError (Join-Path $proofPath "$case.log")
        $processHandle=$process.Handle
        if(!$process.WaitForExit(60000)) {throw "Capture still running PID $($process.Id); inspect before restarting"}
        if($process.ExitCode -ne 0) {throw "$case capture failed"}
        Write-Output "$case captured"
    }
    Add-Type -AssemblyName System.Drawing
    $baseline=[Drawing.Bitmap]::new((Join-Path $proofPath 'OFF-final.bmp'))
    try {
        foreach($case in @('LIGHT','RAY','REFLECTION','ALL')) {
            $image=[Drawing.Bitmap]::new((Join-Path $proofPath "$case-final.bmp"))
            try {
                if($image.Width -ne 800 -or $image.Height -ne 224) {throw 'Unexpected ending capture dimensions'}
                $modelChanges=0
                for($y=0;$y -lt $image.Height;$y++) {for($x=0;$x -lt $image.Width;$x++) {
                    if($baseline.GetPixel($x,$y).ToArgb() -eq $image.GetPixel($x,$y).ToArgb()) {continue}
                    # This fixed fixture's only 3D geometry is THE END.
                    # Nebulae, stars and score lettering must remain untouched.
                    if($x -lt 320 -or $x -ge 481 -or $y -lt 101 -or $y -ge 126) {
                        throw "$case changed background/UI pixel at $x,$y"
                    }
                    $modelChanges++
                }}
                Write-Output "${case}: $modelChanges model pixels changed; background/UI preserved"
            } finally {$image.Dispose()}
        }
    } finally {$baseline.Dispose()}
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys) {Set-Item "Env:$key" $saved[$key]}
}
