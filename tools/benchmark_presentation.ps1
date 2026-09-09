param(
    [Parameter(Mandatory=$true)][string]$Baseline,
    [Parameter(Mandatory=$true)][string]$Candidate,
    [string]$OutputDirectory = 'tmp/optimization-006',
    [int]$Frames = 600,
    [int]$Repeats = 3,
    [ValidateRange(0,3)][int]$Chromatic = 0,
    [ValidateRange(0,3)][int]$Bloom = 0,
    [ValidateRange(0,3)][int]$Bloom2D = 0,
    [switch]$Shadows,
    [int[]]$Displays = @(0,4),
    [int[]]$Scales = @(1,2,4)
)
$ErrorActionPreference = 'Stop'
if ((Split-Path (Resolve-Path $Baseline).Path) -ne (Split-Path (Resolve-Path $Candidate).Path)) {
    throw 'Place both executables in the same folder so portable settings are identical.'
}
foreach ($item in @(Get-ChildItem Env: | Where-Object { $_.Name -match '^STARFOX_(TEST|CAPTURE)_' })) {
    [Environment]::SetEnvironmentVariable($item.Name, $null, 'Process')
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$outputRoot = (Resolve-Path $OutputDirectory).Path
$env:SDL_VIDEODRIVER = 'dummy'
$env:SDL_AUDIODRIVER = 'dummy'
$env:STARFOX_TEST_FRAMES = "$Frames"
$env:STARFOX_TEST_UNPACED = '1'
$env:STARFOX_TEST_PRESENTATION_FPS = '240'
$env:STARFOX_TEST_PREROLL_TICKS = '1000'
$env:STARFOX_TEST_EXPERIENCE = 'ORIGINAL'
$env:STARFOX_TRACE_PROFILE = '1'
$env:STARFOX_TRACE_PROFILE_DISTRIBUTION = '1'
foreach ($option in 'BLOOM','BLOOM_2D','EFFECT','WORLD_EFFECT','2D_FILTER',
    'MODEL_SMOOTHING','RTX_LIGHTING','CHROMATIC_ABERRATION','HDR_EFFECT','ENHANCED_SHADOWS') {
    [Environment]::SetEnvironmentVariable("STARFOX_TEST_$option", '0', 'Process')
}
$env:STARFOX_TEST_CHROMATIC_ABERRATION = "$Chromatic"
$env:STARFOX_TEST_BLOOM = "$Bloom"
$env:STARFOX_TEST_BLOOM_2D = "$Bloom2D"
$env:STARFOX_TEST_ENHANCED_SHADOWS = if ($Shadows) { '1' } else { '0' }
foreach ($display in $Displays) {
    $env:STARFOX_TEST_DISPLAY_MODE = "$display"
    foreach ($scale in $Scales) {
        $env:STARFOX_TEST_RENDER_SCALE = "$scale"
        for ($run = 0; $run -lt $Repeats; ++$run) {
            # Alternate order to reduce warm-up/order bias. Never run together.
            $order = if ($run % 2) { 'candidate','baseline' } else { 'baseline','candidate' }
            foreach ($kind in $order) {
                $exe = if ($kind -eq 'baseline') { $Baseline } else { $Candidate }
                $name = "$kind-d$display-s$scale-r$run"
                $env:STARFOX_CAPTURE_PATH = Join-Path $outputRoot "$name.bmp"
                $log = Join-Path $outputRoot "$name.log"
                $process = Start-Process -FilePath (Resolve-Path $exe).Path -WindowStyle Hidden -Wait -PassThru `
                    -ArgumentList 'upstream-ultrastarfox/SF.SFC','upstream-ultrastarfox/SYMBOLS.TXT','LEVEL1_1' `
                    -RedirectStandardError $log
                if ($process.ExitCode -ne 0) { throw "$name failed: $($process.ExitCode)" }
                Write-Output "$name $(Get-Content $log | Select-String 'render-(profile|distribution)-us')"
            }
            $baseHash = (Get-FileHash (Join-Path $outputRoot "baseline-d$display-s$scale-r$run.bmp")).Hash
            $newHash = (Get-FileHash (Join-Path $outputRoot "candidate-d$display-s$scale-r$run.bmp")).Hash
            if ($baseHash -ne $newHash) { throw "Capture parity failed: display=$display scale=$scale run=$run" }
        }
    }
}
