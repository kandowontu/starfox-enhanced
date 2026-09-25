param(
    [Parameter(Mandatory)][string]$SdkDirectory,
    [Parameter(Mandatory)][string]$Adapter,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference='Stop'
$sdk=(Resolve-Path -LiteralPath $SdkDirectory).Path
$adapterPath=(Resolve-Path -LiteralPath $Adapter).Path
$binaries=Join-Path $sdk 'bin/x64'
# Production runtime only: no developer/debug DLLs or third-party add-ons.
$runtime=@('sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll')
foreach($file in $runtime) {
    $path=Join-Path $binaries $file
    $signature=Get-AuthenticodeSignature -LiteralPath $path
    if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'NVIDIA') {
        throw "Invalid NVIDIA production runtime signature: $file"
    }
}
foreach($file in @('license.txt','3rd-party-licenses.md','bin/x64/nvngx_dlss.license.txt')) {
    if(!(Test-Path -LiteralPath (Join-Path $sdk $file) -PathType Leaf)){throw "Missing runtime notice: $file"}
}
$target=Join-Path ([IO.Path]::GetFullPath($Destination)) 'dlss'
New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -LiteralPath $adapterPath -Destination (Join-Path $target 'starfox_dlss_native.dll')
foreach($file in $runtime){Copy-Item -LiteralPath (Join-Path $binaries $file) -Destination (Join-Path $target $file)}
Copy-Item -LiteralPath (Join-Path $sdk 'license.txt') -Destination (Join-Path $target 'Streamline-LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $sdk '3rd-party-licenses.md') -Destination (Join-Path $target 'Streamline-THIRD-PARTY.md')
Copy-Item -LiteralPath (Join-Path $binaries 'nvngx_dlss.license.txt') -Destination $target
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'package/DLSS-RUNTIME.txt') -Destination $target
# Record exactly what ships so support can distinguish missing/replaced DLLs
# from unsupported hardware. No machine paths or user information are included.
$files=@('starfox_dlss_native.dll')+$runtime
$manifest=@{
    schema=1
    architecture='x64'
    files=@($files | ForEach-Object {
        $installed=Get-Item -LiteralPath (Join-Path $target $_)
        @{
            name=$_
            bytes=$installed.Length
            sha256=(Get-FileHash -LiteralPath $installed.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            version=$installed.VersionInfo.FileVersion
        }
    })
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $target 'runtime-manifest.json') -Encoding utf8
& (Join-Path $PSScriptRoot 'verify_dlss_package.ps1') -Installation $Destination
Write-Output "Packaged optional Windows x64 DLSS runtime: $target"
