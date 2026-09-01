param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$BuildDirectory = "build/xbox-uwp-x64",
    [string]$InstallDirectory = "dist/StarFoxEnhanced-xbox-uwp-x64"
)

$ErrorActionPreference = 'Stop'
$source = [System.IO.Path]::GetFullPath($SourceRoot)
$build = [System.IO.Path]::GetFullPath((Join-Path $source $BuildDirectory))
$install = [System.IO.Path]::GetFullPath((Join-Path $source $InstallDirectory))
if (-not $build.StartsWith($source, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDirectory must remain inside SourceRoot"
}
if (-not $install.StartsWith($source, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "InstallDirectory must remain inside SourceRoot"
}

New-Item -ItemType Directory -Force -Path $build | Out-Null
New-Item -ItemType Directory -Force -Path $install | Out-Null
$certificatePath = Join-Path $build 'StarFoxEnhanced-UWP-Development.pfx'
$publicCertificatePath = Join-Path $build 'StarFoxEnhanced-UWP-Development.cer'
$passwordText = [Convert]::ToBase64String([Guid]::NewGuid().ToByteArray())
$securePassword = ConvertTo-SecureString $passwordText -AsPlainText -Force
$certificate = $null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Visual Studio 2022 with the MSVC x64 tools is required'
}
$visualStudio = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $visualStudio) { throw 'MSVC x64 tools were not found' }
$vcvars = Join-Path $visualStudio 'VC/Auxiliary/Build/vcvarsall.bat'
$environment = & cmd.exe /d /s /c "`"$vcvars`" x64 store >nul && set"
foreach ($line in $environment) {
    $separator = $line.IndexOf('=')
    if ($separator -gt 0) {
        [Environment]::SetEnvironmentVariable(
            $line.Substring(0, $separator),
            $line.Substring($separator + 1), 'Process')
    }
}
$makeAppx = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" `
    -Recurse -File -Filter makeappx.exe |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending | Select-Object -First 1
$signTool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" `
    -Recurse -File -Filter signtool.exe |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($null -eq $makeAppx -or $null -eq $signTool) {
    throw 'Windows SDK MakeAppx and SignTool are required'
}

try {
    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -Subject 'CN=StarFoxEnhanced' `
        -FriendlyName 'Star Fox Enhanced UWP Development' `
        -KeyUsage DigitalSignature `
        -KeyExportPolicy Exportable `
        -HashAlgorithm SHA256 `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -NotAfter (Get-Date).AddYears(2) `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3')
    Export-PfxCertificate -Cert $certificate -FilePath $certificatePath `
        -Password $securePassword | Out-Null
    Export-Certificate -Cert $certificate -FilePath $publicCertificatePath `
        -Type CERT | Out-Null

    & cmake -S $source -B $build -G Ninja `
        '-DCMAKE_SYSTEM_NAME=WindowsStore' `
        '-DCMAKE_SYSTEM_VERSION=10.0' `
        '-DCMAKE_BUILD_TYPE=Release' `
        '-DSTARFOX_BUILD_RUNTIME=ON' `
        '-DSTARFOX_BUILD_TESTS=OFF' `
        '-DSTARFOX_BUILD_TOOLS=OFF' `
        '-DSTARFOX_EMBED_RUNTIME_ASSETS=ON' `
        '-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF'
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP configure failed' }

    & cmake --build $build --target starfox_pc -j 6
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP build failed' }

    $staging = Join-Path $build 'AppX'
    $stagingAssets = Join-Path $staging 'Assets'
    New-Item -ItemType Directory -Force -Path $stagingAssets | Out-Null
    $legacyManifest = Join-Path $staging 'Package.appxmanifest'
    if (Test-Path -LiteralPath $legacyManifest -PathType Leaf) {
        Remove-Item -LiteralPath $legacyManifest -Force
    }
    Copy-Item -LiteralPath (Join-Path $build 'starfox_pc.exe') `
        -Destination $staging -Force
    Copy-Item -LiteralPath (Join-Path $build 'starfox_pc.winmd') `
        -Destination $staging -Force
    Copy-Item -LiteralPath (Join-Path $build 'Package.appxmanifest') `
        -Destination (Join-Path $staging 'AppxManifest.xml') -Force
    Copy-Item -Path (Join-Path $build 'uwp-assets/*.png') `
        -Destination $stagingAssets -Force

    $packageName = 'StarFoxEnhanced-0.0.2-xbox-uwp-x64.appx'
    $packagePath = Join-Path $install $packageName
    if (Test-Path -LiteralPath $packagePath -PathType Leaf) {
        Remove-Item -LiteralPath $packagePath -Force
    }
    & $makeAppx.FullName pack /o /d $staging /p $packagePath
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP package creation failed' }
    & $signTool.FullName sign /fd SHA256 /f $certificatePath `
        /p $passwordText $packagePath
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP package signing failed' }
    $verification = Join-Path $build 'AppX-verification'
    New-Item -ItemType Directory -Force -Path $verification | Out-Null
    & $makeAppx.FullName unpack /o /p $packagePath /d $verification
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP package verification failed' }
    foreach ($required in @(
        'AppxManifest.xml', 'AppxBlockMap.xml', 'AppxSignature.p7x',
        'starfox_pc.exe', 'starfox_pc.winmd')) {
        $verifiedFile = Join-Path $verification $required
        if (-not (Test-Path -LiteralPath $verifiedFile -PathType Leaf)) {
            throw "Xbox UWP package is missing $required"
        }
    }

    Copy-Item -LiteralPath $publicCertificatePath `
        -Destination (Join-Path $install `
            ([System.IO.Path]::GetFileName($publicCertificatePath))) -Force
    Copy-Item -LiteralPath (Join-Path $source 'platform/uwp/README.md') `
        -Destination (Join-Path $install 'README-XBOX-UWP.md') -Force
    Write-Host "Xbox UWP package: $packagePath"
}
finally {
    if ($null -ne $certificate) {
        Remove-Item -LiteralPath $certificate.PSPath -Force
    }
}
