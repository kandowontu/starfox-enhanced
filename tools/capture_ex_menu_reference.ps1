param(
    [string]$Core='tmp/ppu-reference-snes9x/libretro/snes9x_libretro.dll',
    [string]$Rom='tmp/runtime-inputs/starfox-ex/SFES.SFC',
    [string]$OutputDirectory='tmp/ex-menu-reference',
    [string]$DllDirectory='C:/Strawberry/c/bin',
    [switch]$ObserveBg2,
    [switch]$BackgroundOnly,
    [int[]]$Choices=@(0..36)+@(99),
    [ValidateRange(0,600)][int]$ExtraFrames=0
)
$ErrorActionPreference='Stop'
$referencePath=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $referencePath | Out-Null
# Unmodified EX v1.11.03: title L+Select shortcut, page 3 -> page 2,
# then three Up edges to BACKGROUND. No ROM or RAM writes, no save state.
# Start pulses span more than one cartridge update; short video-frame pulses
# can fall between the source game's 20 Hz controller samples.
$menuPresses=@('start:600:630','start:1800:1830','l:3200:3260','select:3200:3260',
    'left:3700:3708','up:4000:4008','up:4040:4048','up:4080:4088')
$sequence=@(5..36)+@(99)+@(0..4)
foreach($choice in $Choices) {
    $steps=[Array]::IndexOf($sequence,$choice)
    if($steps -lt 0){throw "Invalid EX background choice: $choice"}
    $arguments=@('tools/check_reference_video.py',$Core,$Rom,(Join-Path $referencePath "choice-$choice.png"),
        '--frames',[string](4300+40*$steps+$ExtraFrames),'--dll-directory',$DllDirectory,
        '--watch','0x1c54','--watch','0x1ba9','--watch','0x1cb9')
    if($ObserveBg2){$arguments+='--observe-bg2'}
    if($BackgroundOnly){$arguments+='--background-only'}
    if($BackgroundOnly -and $choice -eq 99){$arguments+='--expect-blank'}
    foreach($press in $menuPresses){$arguments+=@('--press',$press)}
    for($step=0;$step -lt $steps;$step++) {
        $first=4200+40*$step
        $arguments+=@('--press',"right:${first}:$($first+8)")
    }
    $log=& python @arguments 2>&1
    if($LASTEXITCODE -ne 0){throw "Reference failed for choice ${choice}: $log"}
    $expected='WRAM: 01c54=02 01ba9=0f 01cb9='+('{0:x2}' -f $choice)
    if(!($log | Where-Object {"$_" -eq $expected})) {throw "Reference navigation mismatch for choice ${choice}: $log"}
    $log | Out-File -LiteralPath (Join-Path $referencePath "choice-$choice.log") -Encoding utf8
    Write-Output "Reference EX choice $choice verified (unmodified ROM/controller input only)"
}
