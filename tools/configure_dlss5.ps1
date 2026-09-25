[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Installation,
    [ValidateSet('OFF','ON')][string]$Mode = 'OFF'
)
$ErrorActionPreference = 'Stop'
$directory = (Resolve-Path -LiteralPath $Installation).Path
$executable = Join-Path $directory 'starfox_pc.exe'
foreach ($name in @('starfox_pc.exe','dxgi.dll','renodx-dlss5.addon64','nvngx_dlssnr.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $directory $name) -PathType Leaf)) {
        throw "Missing $name in the selected installation. This tool does not install or download native add-ons."
    }
}
foreach ($process in @(Get-Process -Name starfox_pc -ErrorAction SilentlyContinue)) {
    if (!$process.Path) { throw 'Cannot identify a running game installation; close the game first.' }
    if ([string]::Equals($process.Path,$executable,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Close this game installation before changing the startup neural setting.'
    }
}
$path = Join-Path $directory 'ReShade.ini'
$text = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllText($path) } else { '' }
$newline = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
$section = [regex]::new('(?m)^\[RenoDX\.DLSS5\][ \t]*\r?$')
$matches = $section.Matches($text)
if ($matches.Count -gt 1) { throw 'Duplicate RenoDX.DLSS5 sections: resolve the ambiguity before changing settings.' }
$value = if ($Mode -eq 'ON') { '1' } else { '0' }
if (!$matches.Count) {
    if ($text.Length -and !$text.EndsWith("`n")) { $text += $newline }
    $text += "[RenoDX.DLSS5]${newline}NeuralUplift=$value$newline"
} else {
    $start = $matches[0].Index + $matches[0].Length
    $next = [regex]::Match($text.Substring($start),'(?m)^\[')
    $end = if ($next.Success) { $start + $next.Index } else { $text.Length }
    $body = $text.Substring($start,$end-$start)
    $key = [regex]::new('(?m)^[ \t]*NeuralUplift[ \t]*=[^\r\n]*')
    if ($key.Matches($body).Count -gt 1) { throw 'Duplicate NeuralUplift keys: resolve the ambiguity before changing settings.' }
    if ($key.IsMatch($body)) { $body = $key.Replace($body,"NeuralUplift=$value") }
    else {
        if (!$body.EndsWith("`n")) { $body += $newline }
        $body += "NeuralUplift=$value$newline"
    }
    $text = $text.Substring(0,$start) + $body + $text.Substring($end)
}
# Keep a recoverable first copy; replace only the selected installation's ini.
if ((Test-Path -LiteralPath $path) -and !(Test-Path -LiteralPath "$path.before-starfox-dlss5")) {
    Copy-Item -LiteralPath $path -Destination "$path.before-starfox-dlss5"
}
[IO.File]::WriteAllText($path,$text,[Text.UTF8Encoding]::new($false))
Write-Output "Experimental neural reconstruction: $Mode on next launch. This does not change ordinary DLSS quality or prove hardware support."
