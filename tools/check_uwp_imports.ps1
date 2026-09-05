param(
    [Parameter(Mandatory=$true)][string]$Binary,
    [Parameter(Mandatory=$true)][string]$VclibsDirectory,
    [string]$Dumpbin = 'dumpbin.exe'
)
$ErrorActionPreference = 'Stop'

# A Windows desktop can accidentally satisfy non-UWP CRT imports from System32.
# Xbox cannot. Check the actual PE, not just CMake settings or an APPX signature.
$imports = & $Dumpbin /imports $Binary
if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $Binary" }
$modules = @{}
$module = $null
foreach ($line in $imports) {
    if ($line -match '^\s+([\w.\-]+\.dll)\s*$') {
        $module = $Matches[1].ToLowerInvariant()
        if (-not $modules.ContainsKey($module)) { $modules[$module] = @() }
    } elseif ($null -ne $module -and $line -match '^\s+[0-9A-F]+\s+(\S+)\s*$') {
        $modules[$module] += $Matches[1]
    }
}
foreach ($module in $modules.Keys) {
    if ($module -match '^(kernel32|user32|msvcp\d+(?:_\w+)?|vcruntime\d+(?:_\d+)?|vccorlib\d+)\.dll$' `
        -and $module -notmatch '_app\.dll$') {
        throw "Xbox-incompatible desktop import: $module. Link using the MSVC Store libraries."
    }
}
foreach ($required in @('msvcp140_app.dll', 'vcruntime140_app.dll', 'vccorlib140_app.dll')) {
    if (-not $modules.ContainsKey($required)) { throw "Missing required UWP runtime import: $required" }
}
foreach ($module in $modules.Keys | Where-Object { $_ -match '_app\.dll$' }) {
    $runtime = Join-Path $VclibsDirectory $module
    if (-not (Test-Path -LiteralPath $runtime -PathType Leaf)) {
        throw "Bundled VCLibs dependency does not supply $module"
    }
    $exports = & $Dumpbin /exports $runtime
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $runtime" }
    $names = @{}
    foreach ($line in $exports) {
        if ($line -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)') {
            $names[$Matches[1]] = $true
        }
    }
    foreach ($symbol in $modules[$module]) {
        if (-not $names.ContainsKey($symbol)) {
            throw "Bundled $module does not export $symbol; the UWP framework is too old for this build."
        }
    }
}
Write-Host 'UWP import check passed: no desktop CRT; all runtime symbols supplied by bundled VCLibs.'
