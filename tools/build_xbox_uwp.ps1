param(
    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$BuildDirectory = "build/xbox-uwp-x64",
    [string]$InstallDirectory = "dist/StarFoxEnhanced-xbox-uwp-x64",
    [ValidateRange(0, 65535)]
    [int]$BuildRevision = 0
)

$ErrorActionPreference = 'Stop'
$source = [System.IO.Path]::GetFullPath($SourceRoot)
$build = [System.IO.Path]::GetFullPath((Join-Path $source $BuildDirectory))
$install = [System.IO.Path]::GetFullPath((Join-Path $source $InstallDirectory))
$packageName = 'StarFoxEnhanced-0.0.4-xbox-uwp-x64.appx'
$sourcePrefix = $source.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $build.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "BuildDirectory must remain inside SourceRoot"
}
if (-not $install.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "InstallDirectory must remain inside SourceRoot"
}

New-Item -ItemType Directory -Force -Path $build | Out-Null
New-Item -ItemType Directory -Force -Path $install | Out-Null
# CMake refuses to reconfigure a build tree with a different generator.  The
# default output directory has been used by both Visual Studio and Ninja in
# older builds, so remove only the generator-owned CMake state when it is not
# already a Ninja tree.  Keep binaries and other user files in the directory.
$cachePath = Join-Path $build 'CMakeCache.txt'
if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
    $generatorEntry = Select-String -LiteralPath $cachePath `
        -Pattern '^CMAKE_GENERATOR:INTERNAL=' -SimpleMatch:$false |
        Select-Object -First 1
    if ($null -ne $generatorEntry -and $generatorEntry.Line -notmatch '=Ninja$') {
        foreach ($cmakeState in @(
            'CMakeCache.txt', 'CMakeFiles', 'build.ninja', '.ninja_deps',
            '.ninja_log', 'rules.ninja', 'cmake_install.cmake', 'Makefile')) {
            $statePath = Join-Path $build $cmakeState
            if (Test-Path -LiteralPath $statePath) {
                Remove-Item -LiteralPath $statePath -Recurse -Force
            }
        }
    }
}
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

# Native UWP binaries import the APP-container MSVC runtime. Consoles do not
# reliably have this framework preinstalled for sideloaded packages. Prefer an
# installed Extension SDK, then a previously verified workspace copy, and
# finally Microsoft's signed winget dependency archive.
$extensionSdkRoot =
    "${env:ProgramFiles(x86)}\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.VCLibs"
$vclibsPackage = Get-ChildItem -LiteralPath $extensionSdkRoot `
    -Recurse -File -Filter 'Microsoft.VCLibs.x64.14.00.appx' `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\Appx\\Retail\\x64\\' } |
    Sort-Object FullName -Descending | Select-Object -First 1
if ($null -eq $vclibsPackage) {
    $vclibsPackage = Get-ChildItem -LiteralPath (Join-Path $source 'build') `
        -Recurse -File -Filter 'Microsoft.VCLibs.140.00_*_x64.appx' `
        -ErrorAction SilentlyContinue |
        Where-Object {
            (Get-AuthenticodeSignature -LiteralPath $_.FullName).Status `
                -eq [System.Management.Automation.SignatureStatus]::Valid
        } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
}
if ($null -eq $vclibsPackage) {
    $dependencyArchive = Join-Path $build 'DesktopAppInstaller_Dependencies.zip'
    $dependencyArchiveHash =
        '906CAD3B2BE067D816B20EA4EB1DF541F8A23AC4A4AA9FED70CE675CD918E6A6'
    if (-not (Test-Path -LiteralPath $dependencyArchive -PathType Leaf) `
        -or (Get-FileHash -LiteralPath $dependencyArchive -Algorithm SHA256).Hash `
            -ne $dependencyArchiveHash) {
        Invoke-WebRequest -UseBasicParsing `
            -Uri 'https://github.com/microsoft/winget-cli/releases/download/v1.12.350/DesktopAppInstaller_Dependencies.zip' `
            -OutFile $dependencyArchive
    }
    if ((Get-FileHash -LiteralPath $dependencyArchive -Algorithm SHA256).Hash `
        -ne $dependencyArchiveHash) {
        throw 'Downloaded Microsoft VCLibs dependency archive failed SHA-256 verification'
    }
    $dependencyExtract = Join-Path $build 'Microsoft-VCLibs'
    if (Test-Path -LiteralPath $dependencyExtract) {
        Remove-Item -LiteralPath $dependencyExtract -Recurse -Force
    }
    Expand-Archive -LiteralPath $dependencyArchive `
        -DestinationPath $dependencyExtract -Force
    $vclibsPackage = Get-ChildItem -LiteralPath $dependencyExtract `
        -Recurse -File -Filter 'Microsoft.VCLibs.140.00_*_x64.appx' |
        Sort-Object FullName | Select-Object -First 1
}
if ($null -eq $vclibsPackage) {
    throw 'The official Microsoft.VCLibs x64 UWP framework package was not found'
}
$vclibsSignature = Get-AuthenticodeSignature -LiteralPath $vclibsPackage.FullName
if ($vclibsSignature.Status `
        -ne [System.Management.Automation.SignatureStatus]::Valid `
    -or $null -eq $vclibsSignature.SignerCertificate `
    -or $vclibsSignature.SignerCertificate.Subject `
        -notlike 'CN=Microsoft Corporation,*') {
    throw 'The selected VCLibs dependency is not validly signed by Microsoft'
}
$dependencyVerification = Join-Path $build 'VCLibs-verification'
if (Test-Path -LiteralPath $dependencyVerification) {
    Remove-Item -LiteralPath $dependencyVerification -Recurse -Force
}
& $makeAppx.FullName unpack /o /p $vclibsPackage.FullName `
    /d $dependencyVerification
if ($LASTEXITCODE -ne 0) { throw 'VCLibs UWP dependency verification failed' }
[xml]$dependencyManifest = Get-Content -LiteralPath `
    (Join-Path $dependencyVerification 'AppxManifest.xml') -Raw
$dependencyIdentity = $dependencyManifest.Package.Identity
if ($dependencyIdentity.Name -ne 'Microsoft.VCLibs.140.00' `
    -or $dependencyIdentity.ProcessorArchitecture -ne 'x64') {
    throw 'Unexpected VCLibs UWP dependency identity or architecture'
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
        '-DSTARFOX_PACKAGE_MSU1_MUSIC=OFF' `
        "-DSTARFOX_UWP_BUILD_REVISION=$BuildRevision"
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP configure failed' }

    & cmake --build $build --target starfox_pc -j 6
    if ($LASTEXITCODE -ne 0) { throw 'Xbox UWP build failed' }

    & (Join-Path $PSScriptRoot 'check_uwp_imports.ps1') `
        -Binary (Join-Path $build 'starfox_pc.exe') `
        -VclibsDirectory $dependencyVerification

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

    # Package in the build tree first. The public distribution is replaced
    # only after package, signature, contents and dependency all validate, so
    # a failed build can never leave a mismatched APPX/certificate pair.
    $packagePath = Join-Path $build $packageName
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
    [xml]$verifiedManifest = Get-Content -LiteralPath (Join-Path $verification 'AppxManifest.xml') -Raw
    if ($verifiedManifest.Package.Identity.ProcessorArchitecture -ne 'x64' `
        -or $verifiedManifest.Package.Identity.Version -ne "0.0.4.$BuildRevision") {
        throw 'Xbox UWP package has the wrong architecture or build revision'
    }
    if ((Get-FileHash -LiteralPath (Join-Path $verification 'starfox_pc.exe')).Hash `
        -ne (Get-FileHash -LiteralPath (Join-Path $build 'starfox_pc.exe')).Hash) {
        throw 'Xbox UWP package does not contain the validated executable'
    }
    foreach ($required in @(
        'AppxManifest.xml', 'AppxBlockMap.xml', 'AppxSignature.p7x',
        'starfox_pc.exe', 'starfox_pc.winmd')) {
        $verifiedFile = Join-Path $verification $required
        if (-not (Test-Path -LiteralPath $verifiedFile -PathType Leaf)) {
            throw "Xbox UWP package is missing $required"
        }
    }

    # Replace the public artifact set only after the complete staged build is
    # known-good. This also removes stale 0.0.2 packages from old checkouts.
    Get-ChildItem -LiteralPath $install -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Name -match '^StarFoxEnhanced-[0-9]+\.[0-9]+\.[0-9]+-xbox-uwp-x64\.appx$'
        } | ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
    $installedPackagePath = Join-Path $install $packageName
    Copy-Item -LiteralPath $packagePath -Destination $installedPackagePath -Force

    $dependencyDirectory = Join-Path $install 'Dependencies/x64'
    New-Item -ItemType Directory -Force -Path $dependencyDirectory | Out-Null
    Get-ChildItem -LiteralPath $dependencyDirectory -File `
        -Filter 'Microsoft.VCLibs*.appx' -ErrorAction SilentlyContinue |
        ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }
    Copy-Item -LiteralPath $vclibsPackage.FullName `
        -Destination (Join-Path $dependencyDirectory $vclibsPackage.Name) -Force

    Copy-Item -LiteralPath $publicCertificatePath `
        -Destination (Join-Path $install `
            ([System.IO.Path]::GetFileName($publicCertificatePath))) -Force
    Copy-Item -LiteralPath (Join-Path $source 'platform/uwp/README.md') `
        -Destination (Join-Path $install 'README-XBOX-UWP.md') -Force
    Write-Host "Xbox UWP package: $installedPackagePath"
}
finally {
    if ($null -ne $certificate) {
        Remove-Item -LiteralPath $certificate.PSPath -Force
    }
}
