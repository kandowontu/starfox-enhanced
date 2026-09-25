param([string]$OutputDirectory='tmp/manipulations-spatial-proof',
    [ValidateSet('ORIGINAL','EX')][string]$Experience='ORIGINAL')
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value; Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy'; SDL_GPU_DRIVER='direct3d12'
        STARFOX_TEST_HIDDEN='1'; STARFOX_TEST_FRAMES='1'; STARFOX_TEST_SKIP_PREROLL='1'
        STARFOX_TEST_PREROLL_TICKS='1000'; STARFOX_TEST_UNPACED='1'; STARFOX_TEST_VSYNC='0'
        STARFOX_TEST_DISPLAY_MODE='16_9'; STARFOX_TEST_RENDER_SCALE='1'
        STARFOX_TEST_PRESENTATION_FPS='60'; STARFOX_TEST_TIMING_MODE='ORIGINAL'
        STARFOX_TEST_MSU1='0'; STARFOX_TEST_STEREO_OUTPUT='0'; STARFOX_TEST_LANGUAGE='0'
        STARFOX_TEST_DLSS_SELECTION='0'; STARFOX_TEST_FSR1_SELECTION='0'
        STARFOX_TEST_RAY_TRACING='0'; STARFOX_TEST_SOFTWARE_SHADOWS='0'
        STARFOX_TEST_REFLECTIVE_SURFACES='0'; STARFOX_TEST_RTX_LIGHTING='0'
        STARFOX_TEST_BLOOM='0'; STARFOX_TEST_BLOOM_2D='0'; STARFOX_TEST_2D_FILTER='0'
        STARFOX_TEST_HDR_EFFECT='0'; STARFOX_TEST_CHROMATIC_ABERRATION='0'
        STARFOX_TEST_MODEL_SMOOTHING='0'; STARFOX_TEST_ANTI_ALIASING='0'
        STARFOX_TEST_SEPARATED_MODELS='0'; STARFOX_TEST_GOD_MODE='1'
    }
    foreach($key in $settings.Keys){Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    $env:STARFOX_TEST_EXPERIENCE=$Experience
    $arguments=if($Experience -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC assets/symbols/starfox-ex.txt LEVEL1_1'}
        else {'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1'}
    foreach($renderer in @('SOFTWARE','GPU')) {
        $env:STARFOX_TEST_RENDERER=$renderer
        foreach($effect in @(0,48,49,50,51,52,53,58,59,60)) {
            $env:STARFOX_TEST_EFFECT='1'; $env:STARFOX_TEST_WORLD_EFFECT='0'
            $env:STARFOX_TEST_MATERIAL='57';$env:STARFOX_TEST_MANIPULATION="$effect"
            $stem="$Experience-$renderer-$effect"
            $env:STARFOX_CAPTURE_PRESENTATION_PATH=Join-Path $proof "$stem.bmp"
            $run=Start-Process build/current/starfox_pc.exe -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardError "$proof/$stem.log"
            $handle=$run.Handle
            if(!$run.WaitForExit(45000)){throw "Capture still running: PID $($run.Id), $stem; inspect before restarting"}
            if($run.ExitCode -ne 0 -or !(Test-Path -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH)) {throw "Capture failed: $stem"}
            $hash=(Get-FileHash -LiteralPath $env:STARFOX_CAPTURE_PRESENTATION_PATH).Hash
            if($effect -eq 0){$baseline=$hash} elseif($hash -eq $baseline){throw "Effect has no visible change: $stem (check saved intensity)"}
            Write-Output "Captured $stem"
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {Remove-Item -LiteralPath "Env:$($_.Name)"}
    foreach($key in $saved.Keys){Set-Item -LiteralPath "Env:$key" -Value $saved[$key]}
}
