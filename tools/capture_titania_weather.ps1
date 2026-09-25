param(
    [string]$OutputDirectory='tmp/titania-weather',
    [ValidateSet('GPU','SOFTWARE')][string]$Renderer='GPU',
    [ValidateSet('native','sky','sky-ground')][string[]]$Variants=@('native','sky','sky-ground')
)
$ErrorActionPreference='Stop'
# Fly normally into TENKI_ON's 100-unit trigger: x=475, y=-109.
# Do not write EBYTE3, palettes, map pointers, weather flags or object state.
# Twenty contiguous three-frame Right presses, then three frames of Down.
$weatherPresses=((0..19 | ForEach-Object {"$($_*3):256"}) + '60:1024') -join ','
foreach($variant in $Variants) {
    $capture=@{
        Experience='ORIGINAL'; Levels=@('LEVEL2_3'); Ticks=@(2600)
        Frames=1100; Presses=$weatherPresses; PressFrames=3
        Aspects=@('16_9'); GodMode=$true; TimingMode='UNLOCKED'
        FinalTarget=$true; TraceObjects=$true; PpuSnapshot=$true
        CaptureInterval=30; Renderer=$Renderer; WaitSeconds=180
        EnhancedSky=($variant -ne 'native'); EnhancedGround=($variant -eq 'sky-ground')
        OutputDirectory=(Join-Path $OutputDirectory $variant)
    }
    & "$PSScriptRoot/capture_background_audit.ps1" @capture
    if(!$?) {throw "Titania weather capture failed: $variant"}
}
