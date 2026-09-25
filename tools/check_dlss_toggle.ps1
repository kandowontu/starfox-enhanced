param([string]$Executable='build/current/starfox_pc.exe',
    [string]$OutputDirectory='tmp/dlss-toggle-check')
$ErrorActionPreference='Stop'
$proof=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $proof | Out-Null
$exe=[IO.Path]::GetFullPath($Executable)
if(!(Test-Path -LiteralPath (Join-Path (Split-Path $exe) 'dlss/starfox_dlss_native.dll'))){
    throw 'This check requires the installed optional DLSS runtime beside the executable'
}
$saved=@{}
Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
    $saved[$_.Name]=$_.Value
    Remove-Item -LiteralPath "Env:$($_.Name)"
}
try {
    $settings=@{
        SDL_AUDIODRIVER='dummy';STARFOX_TEST_HIDDEN='1';STARFOX_TEST_FRAMES='24'
        STARFOX_TEST_RENDERER='GPU';STARFOX_TEST_EXPERIENCE='ORIGINAL'
        STARFOX_TEST_SKIP_PREROLL='1';STARFOX_TEST_UNPACED='1'
        STARFOX_TEST_RENDER_SCALE='1';STARFOX_TEST_VSYNC='0'
        STARFOX_TEST_DLSS_SELECTION='0';STARFOX_TEST_DLSS_TOGGLE='1'
        STARFOX_TEST_RAY_TRACING='0';STARFOX_TEST_REFLECTIVE_SURFACES='0'
    }
    foreach($key in $settings.Keys){Set-Item -LiteralPath "Env:$key" -Value $settings[$key]}
    $log=Join-Path $proof 'toggle.log'
    $run=Start-Process $exe -WindowStyle Hidden -PassThru `
        -ArgumentList 'upstream-ultrastarfox/SF.SFC upstream-ultrastarfox/SYMBOLS.TXT LEVEL1_1' `
        -RedirectStandardError $log
    $handle=$run.Handle
    if(!$run.WaitForExit(60000)){throw "DLSS toggle check timed out (PID $($run.Id))"}
    if($run.ExitCode -ne 0){throw "DLSS toggle check failed ($($run.ExitCode)): $log"}
    $lines=Get-Content -LiteralPath $log
    $upgraded=@($lines | Where-Object {$_ -match '^dlss-presentation: upgraded$'}).Count
    $restored=@($lines | Where-Object {$_ -match '^dlss-presentation: restored$'}).Count
    $restarted=@($lines | Where-Object {$_ -match '^dlss-lifecycle: restarted before renderer creation$'}).Count
    $evaluated=@($lines | Where-Object {$_ -match '^dlss-gameplay: evaluated'}).Count
    $failures=@($lines | Where-Object {$_ -match '^dlss-(gameplay|lifecycle|presentation): .*failed'}).Count
    $last_off=(Get-Content -LiteralPath $log -Raw) -split 'dlss-lifecycle: restarted before renderer creation' | Select-Object -Last 1
    if($upgraded -lt 1 -or $restored -lt 1 -or $restarted -lt 2 -or $evaluated -lt 1 -or $failures -gt 0){
        throw "OFF/ON/OFF lifecycle incomplete: upgraded=$upgraded restored=$restored restarted=$restarted evaluated=$evaluated; $log"
    }
    if($last_off -match 'dlss-presentation: upgraded' -or $last_off -notmatch 'current renderer is not native D3D12') {
        throw "Final OFF renderer did not return to native Vulkan presentation: $log"
    }
    "DLSS OFF/ON/OFF: upgraded=$upgraded restored=$restored restarted=$restarted evaluated=$evaluated"
} finally {
    Get-ChildItem Env: | Where-Object {$_.Name -match '^(STARFOX_|SDL_AUDIODRIVER$|SDL_GPU_DRIVER$)'} | ForEach-Object {
        Remove-Item -LiteralPath "Env:$($_.Name)"
    }
    foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key],'Process')}
}
