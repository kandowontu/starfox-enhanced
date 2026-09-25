param([string]$Installation='build/current')
$ErrorActionPreference='Stop'
$verify=Join-Path $PSScriptRoot 'verify_dlss_package.ps1'
& $verify -Installation $Installation
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('starfox-dlss-package-'+[guid]::NewGuid())
[IO.Directory]::CreateDirectory($fixture) | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $Installation 'dlss') -Destination $fixture -Recurse
    $folder=Join-Path $fixture 'dlss'
    $path=Join-Path $folder 'runtime-manifest.json'
    $original=[IO.File]::ReadAllText($path)
    $rejected={param([string]$expected)
        $failure=$null
        try { & $verify -Installation $fixture } catch { $failure=$_.Exception.Message }
        if(!$failure -or !$failure.Contains($expected)) { throw "Expected '$expected', got '$failure'" }
    }
    $manifest=$original | ConvertFrom-Json
    $manifest.files[0].sha256='0'*64
    [IO.File]::WriteAllText($path,($manifest | ConvertTo-Json -Depth 4))
    & $rejected 'integrity failure'
    $manifest=$original | ConvertFrom-Json
    $manifest.files[0].name=$manifest.files[1].name
    [IO.File]::WriteAllText($path,($manifest | ConvertTo-Json -Depth 4))
    & $rejected 'manifest entry'
    [IO.File]::WriteAllText($path,$original)
    $extra=Join-Path $folder 'unexpected.dll'
    [IO.File]::WriteAllText($extra,'fixture')
    & $rejected 'Unexpected DLLs'
    [IO.File]::Delete($extra)
    $adapter=Join-Path $folder 'starfox_dlss_native.dll'
    $bytes=[IO.File]::ReadAllBytes($adapter)
    $offset=[BitConverter]::ToUInt32($bytes,0x3c)
    # Corrupt only a disposable copy; update its manifest so this specifically
    # exercises architecture rejection rather than hash rejection.
    $bytes[$offset+4]=0x4c;$bytes[$offset+5]=0x01
    [IO.File]::WriteAllBytes($adapter,$bytes)
    $manifest=$original | ConvertFrom-Json
    ($manifest.files | Where-Object name -eq 'starfox_dlss_native.dll').sha256=(Get-FileHash -LiteralPath $adapter).Hash
    [IO.File]::WriteAllText($path,($manifest | ConvertTo-Json -Depth 4))
    & $rejected 'not x64'
    Write-Output 'DLSS package negative checks passed: changed bytes, duplicate entries, unexpected DLL, wrong architecture.'
} finally {
    # Exact newly created fixture only; never the supplied installation.
    [IO.Directory]::Delete($fixture,$true)
}
