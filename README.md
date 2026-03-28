# NT Perform VST

There is a theory which states that if ever anyone discovers exactly what a Eurorack modular synthesizer is for and why it exists, it will instantly be replaced by something even more complex and harder to patch. There is another theory which states that this has already happened.

NT Perform exists somewhere in that space.

---

## What It Is

NT Perform is a VST3/AU plugin that bridges your DAW and the [Expert Sleepers Disting NT](https://www.expert-sleepers.co.uk/distingNT.html) Eurorack module. It gives you direct, automatable control over all 30 performance page parameters of the Disting NT — from within your DAW, using ordinary automation lanes, without once having to lean over and squint at a small knob in a dark studio.

This is generally considered an improvement.

---

## What It Does

- Exposes all **30 Disting NT performance page parameters** as DAW-automatable controls
- Displays **parameter names, ranges, and current values** pulled directly from the hardware
- Communicates over **MIDI SysEx** — either directly to a MIDI device or via the DAW's own MIDI routing
- Supports **configurable SysEx device IDs** for multi-module setups
- Shows **TX/RX activity indicators** so you know something is happening, even if you're not sure what
- Detects and displays the connected module's **firmware version**, which is occasionally useful and always reassuring
- Offers a **grid view** showing all 30 parameters at once, for those who prefer their complexity visible

---

## Installation

### macOS

Download the `.pkg` installer from the [Releases](../../releases) page and run it. The plugin will be installed to `~/Library/Audio/Plug-Ins/VST3/`.

### Windows

Download the `.exe` installer from the [Releases](../../releases) page and follow the prompts. Prompts will be involved.

### Linux

Download the `.tar.gz` from the [Releases](../../releases) page and extract the `.vst3` bundle to `~/.vst3/` or `/usr/lib/vst3/`.

---

## Building From Source

Requirements: CMake 3.22+, a C++17 compiler, and an internet connection (JUCE is fetched automatically).

```bash
cmake -B build
cmake --build build -- -j4
```

The plugin appears in `build/NTPerform_artefacts/`.

### Installing Locally (macOS)

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/NTPerform.vst3
cp -r build/NTPerform_artefacts/Debug/VST3/NTPerform.vst3 ~/Library/Audio/Plug-Ins/VST3/
```

### Code Signing

By default the build signs with an Apple Development certificate suitable for local use. For distribution builds that will run on other machines, see the `CLAUDE.md` for Developer ID signing and notarization instructions.

---

## Usage

1. Load NT Perform on an instrument or MIDI track in your DAW
2. Select the MIDI output connected to your Disting NT's MIDI input
3. Select the MIDI input connected to your Disting NT's MIDI output
4. Set the SysEx ID to match your module (default: 1)
5. The plugin will query the module and populate all parameter names and values
6. Adjust parameters in the plugin, or draw automation — the hardware follows along

If nothing appears to be happening, check your MIDI connections. If things are definitely connected and still nothing is happening, this is entirely normal and is a sign that MIDI is working as designed.

---

## Architecture, Briefly

The plugin speaks SysEx to the Disting NT using Expert Sleepers' documented protocol. A small state machine manages request/response timing so messages don't pile up. Parameter values are kept in sync in both directions — changes in the DAW go to the hardware, and changes on the hardware (via MIDI CC) come back to the DAW.

It is, in the end, a very thin and polite layer of software standing between you and some voltages.

---

## License

MIT. See [LICENSE](LICENSE).
