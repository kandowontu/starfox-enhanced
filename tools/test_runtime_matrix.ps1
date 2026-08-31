param(
    [string]$Executable = "build/current/starfox_pc.exe",
    [string]$Map = "LEVEL1_1",
    [double]$SampleSeconds = 0.6,
    [double]$MinimumRatio = 0.70
)

$ErrorActionPreference = "Stop"
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$rates = @(20, 30, 60, 90, 120, 240, 360, 480)
$displays = 0..4
$paces = @("UNLOCKED", "ORIGINAL")
$experiences = @("ORIGINAL", "EX")
$results = @()

foreach ($experience in $experiences) {
  foreach ($display in $displays) {
    foreach ($rate in $rates) {
      foreach ($pace in $paces) {
            $frames = [Math]::Max(12, [Math]::Ceiling($rate * $SampleSeconds))
            $start = [System.Diagnostics.ProcessStartInfo]::new()
            $start.FileName = $resolvedExecutable
            $start.ArgumentList.Add($Map)
            $start.UseShellExecute = $false
            $start.RedirectStandardError = $true
            $start.CreateNoWindow = $true
            $start.Environment["STARFOX_TEST_FRAMES"] = "$frames"
            $start.Environment["STARFOX_TEST_SKIP_PREROLL"] = "1"
            $start.Environment["STARFOX_TEST_PRESENTATION_FPS"] = "$rate"
            $start.Environment["STARFOX_TEST_DISPLAY_MODE"] = "$display"
            $start.Environment["STARFOX_TEST_TIMING_MODE"] = $pace
            $start.Environment["STARFOX_TEST_EXPERIENCE"] = $experience
            $start.Environment["STARFOX_TEST_RENDERER"] = "GPU"
            $start.Environment["STARFOX_TEST_ANTI_ALIASING"] = "OFF"
            $start.Environment["STARFOX_TEST_ENHANCED"] = "0"
            $start.Environment["STARFOX_TEST_SMOOTH_POLYS"] = "0"
            $start.Environment["STARFOX_TEST_RTX_LIGHTING"] = "0"
            $start.Environment["STARFOX_TRACE_FPS"] = "1"
            $process = [System.Diagnostics.Process]::Start($start)
            $errorText = $process.StandardError.ReadToEnd()
            $process.WaitForExit()
            if ($process.ExitCode -ne 0) {
                throw "Matrix run failed for display=$display fps=$rate pace=$pace`n$errorText"
            }
            $match = [regex]::Match($errorText, "measured=(\d+)")
            if (-not $match.Success) {
                throw "Matrix run did not report FPS for display=$display fps=$rate pace=$pace`n$errorText"
            }
            $measured = [int]$match.Groups[1].Value
            $ratio = $measured / [double]$rate
            $results += [pscustomobject]@{
                Display = $display
                FPS = $rate
                Pace = $pace
                Experience = $experience
                Measured = $measured
                Ratio = [Math]::Round($ratio, 3)
                Pass = $measured -gt 0 -and $ratio -ge $MinimumRatio
            }
      }
    }
  }
}

$results | Format-Table -AutoSize
$failures = @($results | Where-Object { -not $_.Pass })
if ($failures.Count -ne 0) {
    throw "$($failures.Count) runtime timing combinations were stuck or unexpectedly low"
}
Write-Output "Validated $($results.Count) resolution/FPS/pace combinations."
