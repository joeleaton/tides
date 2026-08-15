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
**[Tides-1.1.zip](https://github.com/joeleaton/tides/releases/download/v1.1/Tides-1.1.zip)**

Unzip and read the bundled README for install instructions.

---

## What's new in 1.1

- **INIT is now truly a clean rack.** Hitting Init resets every voice
  group; only G1 stays audible with a neutral starting point.
- **MIDI channel choices persist across sessions.** Set MIDI CH 4 for
  G1 once and every future instance of Tides opens with G1 on channel 4.
- **Reworked morph UX.** Morph is a toggle button now — click to open
  the endpoint pickers + slider, click again to close. The slider
  position is remembered across toggles. While morph is active a
  translucent overlay covers the six edit panes and absorbs mouse
  clicks so knobs the morph is currently driving can't be dragged.
- **User presets can be morph endpoints**, alongside the factory
  library.

---

## Install (short version)

Unzip `Tides-1.1.zip` and drag from its `Plug-ins/` folder:

- `Tides.component` → `~/Library/Audio/Plug-Ins/Components/` — Logic, MainStage, GarageBand
- `Tides.vst3` → `~/Library/Audio/Plug-Ins/VST3/` — Cubase, Reaper, Live, Bitwig, Studio One, …
- `Tides.app` → `/Applications` — Standalone (needs no host)

First-run "unidentified developer" warning:
**System Settings → Privacy & Security → Open Anyway**.

---

## Credits

- DSP, UI, plug-in: Joel Eaton.
- Built with [JUCE](https://juce.com).
