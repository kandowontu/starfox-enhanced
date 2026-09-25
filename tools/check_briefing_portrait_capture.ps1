param(
    [Parameter(Mandatory=$true)][string]$Capture,
    [Parameter(Mandatory=$true)][string]$Reference
)
# Frame-360 EX PLANETSELECT, 400x224 logical viewport at 2x. Compare the
# portrait rectangles independently of the image preview and the lit planet.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$actual=[System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Capture).Path)
$expected=[System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Reference).Path)
try {
    if($actual.Width -ne 800 -or $actual.Height -ne 448 -or
       $expected.Width -ne 800 -or $expected.Height -ne 448) {
        throw 'Expected 800x448 EX briefing captures.'
    }
    foreach($rect in @(@(176,160,272,320),@(556,240,616,320))) {
        $ink=0
        for($y=$rect[1];$y -lt $rect[3];$y++) {
            for($x=$rect[0];$x -lt $rect[2];$x++) {
                $pixel=$actual.GetPixel($x,$y).ToArgb()
                if($pixel -ne $expected.GetPixel($x,$y).ToArgb()) {
                    throw "Portrait mismatch at $x,$y"
                }
                if(($pixel -band 0xffffff) -ne 0) {$ink++}
            }
        }
        if($ink -lt 1000) {throw "Portrait missing or mostly empty: $ink colored pixels"}
        Write-Output "Portrait at $($rect[0]),$($rect[1]): $ink colored pixels; reference exact."
    }
} finally {
    $actual.Dispose()
    $expected.Dispose()
}
