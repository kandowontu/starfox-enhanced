param([string]$OutputDirectory='tmp/colony-source-camera-check')
$ErrorActionPreference='Stop'
# Natural controller replay: no object, boss-health or map-pointer overrides.
# God Mode preserves the route; SourceFrame selects completed presentation state.
$presses=(0..64 | ForEach-Object {
    $frame=$_*100
    $buttons=if($frame -lt 3000){16448}else{16512}
    '{0}:{1}' -f $frame,$buttons
}) -join ','
& "$PSScriptRoot/capture_background_audit.ps1" -OutputDirectory $OutputDirectory `
    -Levels LEVEL2_6 -Ticks 0 -Aspects 4_3 -Frames 6401 -PresentationFps 20 `
    -GodMode -Presses $presses -PressFrames 80 -SourceFrame -ModelLayers -FinalTarget
$log=Get-Content -LiteralPath (Join-Path $OutputDirectory 'ORIGINAL-LEVEL2_6-0-4_3.log')
if(!($log | Where-Object {$_ -match 'render-state .*bg=b1 map=dca9d wait=b71'})) {
    throw 'Colony replay did not reach the expected tunnel source state'
}
$door=@($log | Where-Object {$_ -match '^final-model-pose: header=50239 '})
if($door.Count -ne 1 -or $door[0] -notmatch 'xyz=([^,]+),([^,]+),([^ ]+).* frame=9 .* depth=([^ ]+)') {
    throw 'Expected completed HALF_D frame 9 pose was not captured'
}
$culture=[Globalization.CultureInfo]::InvariantCulture
$presented=[double]::Parse($Matches[3],$culture)
$source=[double]::Parse($Matches[4],$culture)
if([Math]::Abs($presented-$source) -gt 1e-9) {
    throw "Unmodified camera changed source depth: $presented versus $source"
}
Write-Output "Completed Colony door camera preserves source depth $source. This does not prove independent SNES full-frame parity."
