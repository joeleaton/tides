# Tides

A polyphonic synthesiser built around a BBD-style resonator. Karplus-Strong
pluck meets self-oscillating filter meets tuned reverb — articulate,
glassy, organic.

By Joel Eaton.

macOS Apple Silicon (M1/M2/M3/M4), macOS 11 or later.
Ships as **Audio Unit**, **VST3**, and **Standalone**.

---

## Download

Grab the latest build from **[Releases](https://github.com/joeleaton/tides/releases)**.

Direct link to the current version:
**[Tides-1.4.zip](https://github.com/joeleaton/tides/releases/download/v1.4/Tides-1.4.zip)**

Unzip and read the bundled README for install instructions.

---

## Documentation

- **[User Guide](docs/USER_GUIDE.md)** — quick start, the interface, sound-design
  workflow, voice groups & the routing bar, modulation, MPE, presets, troubleshooting.
- **[Reference](docs/REFERENCE.md)** — full parameter index, the modulation matrix,
  voice-group routing & priority modes, morph, quality mode, persistence, glossary.

---

## What's new in 1.4

- **Click the preset name to browse** — the separate Browse button is gone; the
  preset name in the header now opens the preset menu (marked with a ▾ caret,
  highlights on hover). ◀ ▶ still step through presets.

Earlier: **1.3** brought clearer UI typography, tidier knob rows, a Morph
return-to-A fix and a preset-name typo fix.
**1.2** gave LFOs genuine free-run (KB Rst off keeps them evolving
across notes) and shipped the docs below.

---

## Install (short version)

Unzip `Tides-1.4.zip` and drag from its `Plug-ins/` folder:

- `Tides.component` → `~/Library/Audio/Plug-Ins/Components/` — Logic, MainStage, GarageBand
- `Tides.vst3` → `~/Library/Audio/Plug-Ins/VST3/` — Cubase, Reaper, Live, Bitwig, Studio One, …
- `Tides.app` → `/Applications` — Standalone (needs no host)

### First-run gotcha (important)

Tides is ad-hoc-signed, so macOS marks it as quarantined. **The plug-in
bundles will silently fail to load in your host** until you strip the
quarantine flag. In Terminal, after copying the files into place:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Tides.component
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Tides.vst3
xattr -dr com.apple.quarantine /Applications/Tides.app
```

For the Standalone you can alternatively use
**System Settings → Privacy & Security → Open Anyway** after the first
launch attempt is refused. Bundle plug-ins have no such prompt — the
`xattr` command is the only fix for those.

---

## Credits

- DSP, UI, plug-in: Joel Eaton.
- Built with [JUCE](https://juce.com).
