$ErrorActionPreference = 'Stop'
$configure = Join-Path $PSScriptRoot 'configure_dlss5.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('starfox-dlss5-config-' + [guid]::NewGuid())
[IO.Directory]::CreateDirectory($fixture) | Out-Null
try {
    # Empty placeholders are never loaded/executed: exercise configuration only.
    foreach ($name in @('starfox_pc.exe','dxgi.dll','renodx-dlss5.addon64','nvngx_dlssnr.dll')) {
        [IO.File]::WriteAllText((Join-Path $fixture $name),'')
    }
    $path = Join-Path $fixture 'ReShade.ini'
    $original = "[GENERAL]`r`nKeep=unchanged`r`n[RenoDX.DLSS5]`r`nNRIntensity=0.7`r`nNeuralUplift=0`r`n[OTHER]`r`nNeuralUplift=42`r`n"
    [IO.File]::WriteAllText($path,$original)
    & $configure -Installation $fixture -Mode ON
    if ([IO.File]::ReadAllText($path) -cne $original.Replace('NeuralUplift=0','NeuralUplift=1')) { throw 'ON changed unrelated settings' }
    & $configure -Installation $fixture
    if ([IO.File]::ReadAllText($path) -cne $original) { throw 'Default OFF failed round trip' }
    if ([IO.File]::ReadAllText("$path.before-starfox-dlss5") -cne $original) { throw 'Original backup changed' }
    foreach ($source in @('[GENERAL]', "[RenoDX.DLSS5]`nNRIntensity=1`n[OTHER]`nKeep=yes")) {
        [IO.File]::WriteAllText($path,$source)
        & $configure -Installation $fixture -Mode ON
        $once = [IO.File]::ReadAllText($path)
        & $configure -Installation $fixture -Mode ON
        if ([IO.File]::ReadAllText($path) -cne $once) { throw 'Insertion is not idempotent' }
        if (!$once.Contains('NeuralUplift=1')) { throw 'Missing inserted key' }
    }
    foreach ($ambiguous in @("[RenoDX.DLSS5]`n[RenoDX.DLSS5]", "[RenoDX.DLSS5]`nNeuralUplift=0`nNeuralUplift=1")) {
        [IO.File]::WriteAllText($path,$ambiguous)
        $rejected = $false
        try { & $configure -Installation $fixture -Mode ON } catch { $rejected = $true }
        if (!$rejected -or [IO.File]::ReadAllText($path) -cne $ambiguous) { throw 'Ambiguous ini was not safely rejected' }
    }
    Write-Output 'DLSS5 configuration checks passed (no add-on loaded).'
} finally {
    # This exact, newly allocated fixture contains no user assets.
    [IO.Directory]::Delete($fixture,$true)
}
