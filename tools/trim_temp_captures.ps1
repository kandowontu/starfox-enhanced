param([switch]$Apply)
$ErrorActionPreference='Stop'
$workspace=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$captureRoot=[IO.Path]::GetFullPath((Join-Path $workspace 'tmp'))
$prefix=$captureRoot+[IO.Path]::DirectorySeparatorChar
# Explicit old diagnostic sequences only. Never touch toolchains, downloads,
# source assets, saves, executable builds, or unrecognized file names.
$folders=@(
 'gpu-wipe-no-bloom-sequence-240','gpu-wipe-no-lighting-sequence-240',
 'gpu-wipe-full-sequence-240','gpu-wipe-sequence-residual-fixed',
 'state-ex-death-long-continuation','state-original-msu-scheduled-death',
 'state-fresh-ex-scheduled-revival-late','gpu-wipe-ex-full-sequence-240',
 'gpu-wipe-ex-no-bloom-sequence-240','gpu-ex-tunnel-sequence',
 'gpu-wipe-ex-sequence-residual-fixed','state-fresh-ex-scheduled-revival',
 'revival-established-late','sbs-wipe-current-original','sbs-wipe-current-ex',
 'sbs-wipe-finalmask-ex','sbs-wipe-complete-fixed-240','sbs-wipe-finalmask-original',
 'sbs-wipe-traced-ex','sbs-wipe-traced-original','sbs-wipe-regression','sbs-wipe-complete-240')
$notes=(Get-ChildItem -LiteralPath (Join-Path $workspace 'docs') -File -Recurse |
 ForEach-Object {Get-Content -LiteralPath $_.FullName -Raw}) -join "`n"
$notes=$notes.Replace('\','/')
$candidates=@(foreach($folder in $folders) {
 $target=[IO.Path]::GetFullPath((Join-Path $captureRoot $folder))
 if(!$target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {throw 'Unsafe capture path'}
 if(!(Test-Path -LiteralPath $target)) {continue}
 if((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Refusing linked capture directory'}
 if(Get-ChildItem -LiteralPath $target -Directory -Recurse | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}) {throw 'Refusing capture directory containing links'}
 Get-ChildItem -LiteralPath $target -File -Recurse | ForEach-Object {
  if($_.Name -match '^(.*\.frame-)(\d+)\.bmp$' -or $_.Name -match '^()(\d+)\.bmp$') {
   [PSCustomObject]@{Path=$_.FullName;Name=$_.Name;Sequence=$_.DirectoryName+'|'+$Matches[1];Frame=[int]$Matches[2];Bytes=$_.Length}
  }
 }
})
$remove=@(foreach($group in ($candidates | Group-Object Sequence)) {
 $ordered=@($group.Group | Sort-Object Frame)
 if($ordered.Count -le 30) {continue}
 $first=$ordered[2].Frame; $last=$ordered[-3].Frame
 foreach($file in $ordered) {
  $relative=$file.Path.Substring($workspace.Length+1).Replace('\','/')
  if($file.Frame -le $first -or $file.Frame -ge $last -or $file.Frame%60 -eq 0 -or
      $notes.Contains($relative) -or $notes.Contains($file.Name)) {continue}
  $file
 }
})
$bytes=($remove | Measure-Object Bytes -Sum).Sum
[PSCustomObject]@{Apply=[bool]$Apply;Files=$remove.Count;ReclaimGiB=[math]::Round($bytes/1GB,3)} | Format-List
if(!$Apply) {return}
# Manifest records exactly what was discarded; generated captures can be rerun,
# but deleted files are not sent to the recycle bin (the point is disk space).
$manifest=Join-Path $captureRoot ('capture-cleanup-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.csv')
$remove | Select-Object Path,Bytes | Export-Csv -LiteralPath $manifest -NoTypeInformation
foreach($file in $remove) {
 $resolved=[IO.Path]::GetFullPath($file.Path)
 if(!$resolved.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase) -or [IO.Path]::GetExtension($resolved) -ne '.bmp') {throw 'Unsafe file target'}
 Remove-Item -LiteralPath $resolved
}
Write-Output "Removed $($remove.Count) generated frames; reclaimed $([math]::Round($bytes/1GB,3)) GiB. Manifest: $manifest"
