# iOS alpha package

This archive contains an unsigned arm64 device application. It intentionally
contains no retail ROM. Before installing it on an iPhone or iPad, sign the
application with your own Apple development identity or through your preferred
sideloading tool.

On first launch, the system file picker accepts either a `Starfox-Assets.BIN`
prepared on a PC or an unmodified supported Star Fox/Starwing ROM. The selected
file is copied into the app sandbox before it is validated. The resulting
runtime bundle is stored privately in the app sandbox.

The published 0.0.6.7 IPA predates the native iOS picker and reports that the
SDL file-dialog operation is unsupported. For that package, add
`Starfox-Assets.BIN` to the app's Documents folder using iTunes File Sharing:
select the connected device, open File Sharing, select Star Fox Enhanced, then
use Add File. The Files app's “On My iPhone” → “Star Fox Enhanced” folder is
another way to place the same file when that folder is visible. Relaunch the
app after copying it. The file must be at the top of the app's Documents folder,
not in a subfolder.

This alpha is not an App Store package and has not been notarized or submitted
to App Review. The published 0.0.6.7 package has launched on one physical
iPhone after File Sharing import; that is not a full device compatibility or
4× upscale stability validation.
