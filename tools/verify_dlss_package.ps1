[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$Installation)
$ErrorActionPreference='Stop'
$folder=Join-Path (Resolve-Path -LiteralPath $Installation).Path 'dlss'
$expected=@('starfox_dlss_native.dll','sl.interposer.dll','sl.common.dll','sl.dlss.dll','nvngx_dlss.dll')
$manifest=Get-Content -LiteralPath (Join-Path $folder 'runtime-manifest.json') -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or $manifest.architecture -cne 'x64') { throw 'Unsupported DLSS manifest format/architecture' }
$entries=@($manifest.files)
if ($entries.Count -ne $expected.Count) { throw 'DLSS manifest must describe exactly five runtime DLLs' }
foreach($name in $expected) {
    $entry=@($entries | Where-Object { $_.name -ceq $name })
    if ($entry.Count -ne 1) { throw "Missing/duplicate DLSS manifest entry: $name" }
    $file=Get-Item -LiteralPath (Join-Path $folder $name)
    if($file.PSIsContainer -or $file.Length -ne $entry[0].bytes -or
        (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ine $entry[0].sha256) {
        throw "DLSS package integrity failure: $name"
    }
    # Validate PE headers without loading any code; mixed x86/x64 packages
    # otherwise misleadingly appear as unsupported GPUs at startup.
    $stream=[IO.File]::OpenRead($file.FullName)
    $reader=[IO.BinaryReader]::new($stream)
    try {
        if($stream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5a4d) { throw "Not a PE DLL: $name" }
        $stream.Position=0x3c
        $offset=$reader.ReadUInt32()
        if($offset -lt 64 -or $offset+26 -gt $stream.Length) { throw "Invalid PE header: $name" }
        $stream.Position=$offset
        if($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw "DLSS DLL is not x64: $name" }
        $stream.Position=$offset+22
        if(($reader.ReadUInt16() -band 0x2000) -eq 0 -or $reader.ReadUInt16() -ne 0x20b) { throw "Invalid x64 DLL header: $name" }
    } finally { $reader.Dispose(); $stream.Dispose() }
    if($name -ne 'starfox_dlss_native.dll') {
        $signature=Get-AuthenticodeSignature -LiteralPath $file.FullName
        if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'NVIDIA') {
            throw "Invalid NVIDIA signature: $name"
        }
    }
}
$dlls=@(Get-ChildItem -LiteralPath $folder -Filter '*.dll' -File)
if($dlls.Count -ne $expected.Count -or @($dlls | Where-Object { $_.Name -notin $expected }).Count) {
    throw 'Unexpected DLLs in production DLSS folder; use a clean staging directory'
}
foreach($notice in @('Streamline-LICENSE.txt','Streamline-THIRD-PARTY.md','nvngx_dlss.license.txt','DLSS-RUNTIME.txt')) {
    $file=Get-Item -LiteralPath (Join-Path $folder $notice)
    if($file.PSIsContainer -or !$file.Length) { throw "Missing/empty notice: $notice" }
}
Write-Output 'DLSS package verified: x64 architecture, exact file set, hashes, NVIDIA signatures and notices. Redistribution approval is a separate requirement.'
