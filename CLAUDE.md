# NT Perform VST — Build Instructions

## Build

```bash
cmake -B build
cmake --build build -- -j4
```

## Install (macOS)

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/NTPerform.vst3
cp -r build/NTPerform_artefacts/Debug/VST3/NTPerform.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

## Code Signing

**Default (Apple Development — local testing):** no override needed. The CMakeLists.txt defaults to SHA1 `6EDA7EFA231C691B7862F1B7AAA134B31920710C`.

**Distribution (Developer ID + notarization required on macOS 26+):**

```bash
cmake -B build -DCODE_SIGN_IDENTITY=4C9BC6484BFCD52CBB79C2C84583C40051808DA1
cmake --build build -- -j4
# Then notarize before installing:
# xcrun notarytool submit ... --wait
# xcrun stapler staple build/NTPerform_artefacts/Debug/VST3/NTPerform.vst3
```

> **macOS 26 note:** Developer ID plugins must be notarized. Without notarization the host process is killed with `CODESIGNING / Invalid Page` at load time. Apple Development cert works for local development without notarization.

## Certs (Team KN424RZG26)

| Purpose | SHA1 |
|---|---|
| Apple Development (local) | `6EDA7EFA231C691B7862F1B7AAA134B31920710C` |
| Developer ID Application (distribution) | `4C9BC6484BFCD52CBB79C2C84583C40051808DA1` |
