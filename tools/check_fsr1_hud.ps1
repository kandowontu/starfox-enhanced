param([string]$OutputDirectory='tmp/fsr1-hud-proof')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
foreach($mode in 0..4) {
    & "$PSScriptRoot/capture_presentation.ps1" -OutputDirectory "$OutputDirectory/$mode" -FinalTarget -Fsr1Mode $mode -Frames 12
}
$baseline=[Drawing.Bitmap]::new([IO.Path]::GetFullPath("$OutputDirectory/0/presentation.bmp"))
try {
    foreach($mode in 1..4) {
        $actual=[Drawing.Bitmap]::new([IO.Path]::GetFullPath("$OutputDirectory/$mode/presentation.bmp"))
        try {
            $checked=0
            # Bright HUD ink only: background behind transparent HUD must change
            # under spatial reconstruction. Include shield, bombs and boost.
            foreach($rect in @(@(48,366,130,406),@(672,364,756,403))) {
                for($y=$rect[1];$y -lt $rect[3];$y++) {for($x=$rect[0];$x -lt $rect[2];$x++) {
                    $expected=$baseline.GetPixel($x,$y)
                    if($expected.R -le 120 -and $expected.B -le 130){continue}
                    $checked++
                    if($actual.GetPixel($x,$y).ToArgb() -ne $expected.ToArgb()) {
                        throw "FSR mode $mode changed HUD ink at $x,$y"
                    }
                }}
            }
            if($checked -lt 300){throw 'HUD fixture contains too little ink'}
            Write-Output "FSR mode ${mode}: $checked exact HUD pixels"
        } finally {$actual.Dispose()}
    }
} finally {$baseline.Dispose()}
