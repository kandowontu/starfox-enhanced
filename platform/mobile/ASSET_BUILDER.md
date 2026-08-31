# Preparing mobile runtime data on a PC

`starfox_asset_builder` creates the same version-bound `Starfox-Assets.BIN`
that the game normally creates on first launch. No ROM data is included in the
builder or in this repository.

On Windows, drag a supported unmodified `.sfc`/`.smc` ROM onto
`starfox_asset_builder.exe`, or run:

```text
starfox_asset_builder.exe "C:\path\to\Star Fox.sfc"
```

The output is `Starfox-Assets.BIN` in the current folder. Transfer that BIN to
the device and choose it from Star Fox Enhanced's first-launch file picker.
The app validates the BIN against its embedded patch/symbol manifest before
copying it into private application storage.

Supported inputs are Star Fox Japan 1.0/1.1, USA 1.0/1.1/1.2, Starwing Europe
1.0/1.1, and Starwing Germany 1.0. A 512-byte copier header is accepted.
