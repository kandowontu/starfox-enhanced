param(
    [ValidateSet('ORIGINAL','EX','BOTH')][string]$Experience='BOTH',
    [ValidateRange(1,10000)][int]$Ticks=600,
    [string]$Executable='build/vr-dev/starfox_vr_runtime_check.exe',
    [switch]$VerifyGeometryCache,
    [switch]$RayAudit,
    [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
if($RayAudit -and $VerifyGeometryCache) {throw 'RayAudit and VerifyGeometryCache are separate diagnostic modes'}
if($OutputDirectory) {New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null}
$passed=0
$failed=@()
foreach($variant in @('ORIGINAL','EX')) {
    if($Experience -ne 'BOTH' -and $Experience -ne $variant) {continue}
    $rom=if($variant -eq 'EX') {'tmp/runtime-inputs/starfox-ex/SFES.SFC'} else {'upstream-ultrastarfox/SF.SFC'}
    $symbols=if($variant -eq 'EX') {'assets/symbols/starfox-ex.txt'} else {'upstream-ultrastarfox/SYMBOLS.TXT'}
    $levels=@(Get-Content -LiteralPath $symbols | ForEach-Object {
        if($_ -match '^(LEVEL[1-7]_[1-9])\s') {$Matches[1]}
    } | Sort-Object -Unique)
    if(!$levels.Count) {throw "No numbered stages found for $variant"}
    foreach($level in $levels) {
        $arguments=@('--preflight',$rom,$symbols,$level,$Ticks)
        if($VerifyGeometryCache) {$arguments+='--verify-geometry-cache'}
        if($RayAudit) {$arguments+=@('--ray-audit','--preflight-invulnerable')}
        $output=& $Executable @arguments 2>&1
        $code=$LASTEXITCODE
        if($OutputDirectory) {$output | Out-File -LiteralPath (Join-Path $OutputDirectory "$variant-$level.log") -Encoding utf8}
        if($RayAudit -and $code -eq 0 -and (($output -join "`n") -notmatch 'Ray input audit: compute accepted=\d+ rejected=0 legacy ordinary=0')) {
            $code=1
            Write-Output "Incomplete/rejected GPU model migration: $variant/$level"
        }
        if($code -ne 0) {
            $failed+="$variant/$level"
            Write-Output "FAIL $variant/$level (exit $code)"
            Write-Output $output
        } else {
            ++$passed
            Write-Output "PASS $variant/$level ($Ticks source ticks, source assembly/input preparation only)"
        }
    }
}
Write-Output "World preflight totals: $passed passed; $($failed.Count) failed. No GPU or headset verification."
if($failed.Count) {throw "Failed stages: $($failed -join ', ')"}
