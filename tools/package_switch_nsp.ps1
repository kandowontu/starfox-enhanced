[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$NroPath = ".\switch\StarFoxEnhanced\StarFoxEnhanced.nro",

    [Parameter(Mandatory = $false)]
    [string]$SdmcPath = "/switch/StarFoxEnhanced/StarFoxEnhanced.nro"
)

$ErrorActionPreference = 'Stop'

$resolvedNro = (Resolve-Path -LiteralPath $NroPath).Path
if ([System.IO.Path]::GetExtension($resolvedNro) -ine '.nro') {
    throw "NroPath must identify a .nro file: $resolvedNro"
}

$nton = Get-Command nton -ErrorAction SilentlyContinue
if ($null -eq $nton) {
    throw "NTON was not found. Install it with: py -m pip install nton"
}

$keysPath = Join-Path ([Environment]::GetFolderPath('UserProfile')) '.switch\prod.keys'
if (-not (Test-Path -LiteralPath $keysPath -PathType Leaf)) {
    throw "Your own dumped prod.keys is required at $keysPath"
}

if (-not $SdmcPath.StartsWith('/')) {
    throw "SdmcPath must be absolute from the SD-card root (start it with /)."
}

Write-Host "Creating a local NSP forwarder for $resolvedNro"
Write-Host "The forwarder will launch sdmc:$SdmcPath"
& $nton.Source build $resolvedNro `
    --sdmc $SdmcPath `
    --name 'Star Fox Enhanced' `
    --publisher 'Star Fox Enhanced team' `
    --version '0.0.4'
if ($LASTEXITCODE -ne 0) {
    throw "NTON failed with exit code $LASTEXITCODE"
}

Write-Host 'NSP creation complete. NTON places output in Desktop\NTON.'
