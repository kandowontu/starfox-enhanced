# Xbox UWP Developer Mode build

This is an x64 UWP package for Xbox One and Xbox Series consoles running in
Developer Mode. Upload the `.appx` and any files in `Dependencies` through the
Xbox Device Portal. It is not a retail Xbox package and cannot run outside
Developer Mode.

The app uses its UWP `LocalState` directory as internal writable storage. Put
`Starfox-Assets.BIN` there before first launch. `Starfox-MSU1.PAK` is optional
and may be put in the same directory. Both names are discovered without regard
to letter case. Settings, controller/keyboard choices, HUD layouts, and any
rebuilt asset companion are also saved in LocalState.

The included `.cer` contains only the public development certificate. If the
console asks for a certificate while deploying, upload that file with the
package. The private signing key is generated during the build and is never
placed in the distribution folder.

Build on Windows with Visual Studio 2022's Universal Windows Platform C++
tools and the Windows 11 SDK installed:

```powershell
.\tools\build_xbox_uwp.ps1
```
