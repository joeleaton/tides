# Tides — UI Design Hand-Off

A specification for the tactile UI that will replace the current debug panel.
This document is self-contained: a designer does not need to read the
codebase to work from it.

The engine is feature-complete and stable. The UI is the gate to a
shippable product. The current debug panel is functional but not
shippable.

---

## 1. What Tides is

Tides is a software emulation of an analogue **BBD-resonator polyphonic
synthesizer** (modelled on the Morphor ECHON 6). It is **not** a
subtractive synth with a delay effect. Each voice is a **tuned
positive-feedback resonator** (a fractional-delay feedback loop with an
in-loop filter and saturation) **excited by a small subtractive section**.
Think Karplus–Strong / modal resonance, not Juno.

The instrument is multitimbral: a Preset contains up to 6 independent
**voice groups**, each with a full sound + its own MIDI channel. Voices
are drawn dynamically from a shared 16-voice pool (better than the
hardware's fixed 6, per design choice).

### Sonic identity

- The resonator is the pitch element; it keytracks. The exciter only
  keytracks if Key Follow is routed.
- Self-oscillation is a first-class mode: with no exciter, sufficient
  Feedback ignites the loop from the analog noise seed.
- The "Wild ↔ Tame" macro controls the analog-instability stack (noise
  seed, drift, detune, saturator drive) in a single knob.
- Six (or more) voices in unison create lush, organically detuned beating
  through per-voice drift.

---

## 2. Layout zones

The hardware panel is organised in 6 zones plus a master row. The UI does
**not** have to copy this layout — but the same parameter *groupings*
should be preserved so users moving between hardware and software find
the same things in the same place. Suggested groupings:

```
┌──────────────────────────────────────────────────────────────────┐
│  Preset / Voice Group bar  (preset name • A/B • focused group)   │
├────────────────┬────────────────────────────────┬────────────────┤
│  Exciter       │  BBD Resonator                 │  Output        │
│  (VCO morph,   │  (Input Filter, Feedback,      │  (Dry/Wet,     │
│   Noise+S&H,   │   Feedback Filter, Invert,     │   Pan, Level,  │
│   DADSR)       │   Glide, Fine Tune, Note Off)  │   Wild/Tame,   │
│                │                                │   Master)      │
├────────────────┴────────────────────────────────┴────────────────┤
│  Quad LFO  (4 LFOs, per-LFO controls + LFO selector)             │
├──────────────────────────────────────────────────────────────────┤
│  Modulation Matrix  (9 sources × 32 destinations, source macros) │
├──────────────────────────────────────────────────────────────────┤
│  Performance row  (unison, MIDI channel, polyphony, priority)    │
├──────────────────────────────────────────────────────────────────┤
│  Visual feedback strip  (scope, voice activity, peak meters)     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Parameter inventory

### 3.1 Host-automatable parameters (APVTS)

All entries below are exposed to the DAW as automation parameters. They
target the **currently focused voice group** (see §5). Value range is
0–1 normalised unless stated. CC# is the MIDI Continuous Controller
number when sent on a voice group's MIDI channel.

| Section | Parameter | ID | Range | Default | CC# |
|---|---|---|---|---|---|
| Exciter | Wave (sine→tri→ramp→pulse→square morph) | `ex_wave` | 0–1 | 0.0 | 15 |
| Exciter | Coarse Freq (±4 oct) | `ex_coarse` | 0–1 | 0.5 | 17 |
| Exciter | Fine Freq (±3 st) | `ex_fine` | 0–1 | 0.5 | 18 |
| Exciter | Noise S&H Rate (stepped→white) | `ex_sh_rate` | 0–1 | 1.0 | 19 |
| Exciter | Wave/Noise Mix | `ex_wave_noise` | 0–1 | 0.0 | 20 |
| Exciter | Exciter Level | `ex_level` | 0–1 | 0.7 | 21 |
| Envelope | Onset Delay (0–2 s) | `env_onset` | 0–1 | 0.0 | 22 |
| Envelope | Attack | `env_attack` | 0–1 | 0.02 | 23 |
| Envelope | Decay | `env_decay` | 0–1 | 0.3 | 24 |
| Envelope | Sustain (level) | `env_sustain` | 0–1 | 1.0 | 25 |
| Envelope | Release | `env_release` | 0–1 | 0.15 | 26 |
| Resonator | Input Filter (12 dB/oct LPF) | `res_in_filter` | 0–1 | 1.0 | 31 |
| Resonator | Feedback Filter (6 dB/oct LPF, in loop) | `res_fb_filter` | 0–1 | 0.7 | 40 |
| Resonator | Feedback | `res_feedback` | 0–1 | 0.0 | 43 |
| Resonator | Fine Tune (±3 st) | `res_fine_tune` | 0–1 | 0.5 | 46 |
| Resonator | Glide (0–1 s) | `res_glide` | 0–1 | 0.0 | 5 |
| Output | Dry/Wet | `out_drywet` | 0–1 | 0.0 | 48 |
| Output | Panning | `out_pan` | 0–1 | 0.5 | 10 |
| Output | Voice-group Level | `out_level` | 0–1 | 0.8 | 7 |
| Output | Wild ↔ Tame (analog character macro) | `wild_tame` | 0–1 | 0.4 | — |
| Output | Master | `master` | 0–1 | 0.8 | — |
| Modulation | Source macro × 9 (attenuverters) | `macro_0…8` | **−1…+1** | 0 | 102–110 |
| Routing | Focused Voice Group | `focused_group` | 1–6 | 1 | — |

**Macro source order (matches manual & engine):**
`0` Envelope · `1` LFO1 · `2` LFO2 · `3` LFO3 · `4` LFO4 ·
`5` Modwheel · `6` Velocity · `7` Aftertouch · `8` Key Follow

> Macros are bipolar attenuverters. **12 o'clock = no modulation passed.**
> The UI must show this — a clicked-detent at centre and bipolar polarity
> (e.g. red right / blue left, matching destination LED colours).

### 3.2 Non-automated state (still persisted)

These live in PatchState (per voice group) and are saved with the preset,
but are not currently exposed as host-automation params. The UI must
present controls for them. They could be promoted to APVTS later.

| Section | Control | Type | Notes |
|---|---|---|---|
| Exciter | KB Reset (VCO phase reset on note-on) | toggle | Default off (free-run, organic) |
| Exciter | Ext In | toggle | When on, WAVE CCW outputs the external audio bus |
| Envelope | Range | choice (Short/Medium/Long) | Scales A/D/R time bounds |
| Envelope | Hold | toggle | Envelope ignores note-off (drones) |
| Envelope | Legato | toggle | New notes continue current level |
| Envelope | Loop | toggle | Cycle Delay→Attack→Decay while held (implies Legato) |
| Resonator | Invert | toggle | Polarity flip in feedback loop |
| Resonator | Note Offset | int (−12…+12) | Quantised semitone transpose |
| Resonator | Key Follow VCO | toggle | INIT default on (VCO tracks keyboard) |
| Unison | Voices | int 1–6 | Per-voice-group |
| Unison | Detune | 0–1 | ±1 semitone at full |
| Unison | Spread | 0–1 | Per-voice pan width |
| Quad LFO | Per-LFO: Wave (Sin/Tri/Ramp/Sqr), Rate, Random Rate, Amplitude, Random/Wave Mix, Bipolar, Smooth, KB Reset | mixed | 4 LFOs per voice |
| Mod Matrix | Connections | list | Up to 9×32; each = {source, dest, depth, enabled, muted} |
| Mod Matrix | Source detach flags | 9 bools | When set, macro acts as static manual value |
| Group routing | Per-group: MIDI Channel (1–16), Polyphony (0–16), Priority (5 modes) | mixed | One row per group |
| Preset routing | Preset MIDI Channel, Preset Unison, Preset Priority (5 modes) | mixed | Master/layering control |

### 3.3 Voice-allocation priority modes

Used at both the voice-group level (which voice within a group) and the
preset level (which group, when notes arrive on the preset channel with
Preset Unison off).

1. **First Available + Steal** — first free voice; else steal least-recently-used in group.
2. **First Available NoSteal** — first free; else ignore the note.
3. **Round-Robin + Steal** — rotate through voices; else steal LRU.
4. **Least Recently Used** — always the oldest available.
5. **Reuse Same Note** — retrigger a voice that's already playing that pitch; else fall back to mode 1.

---

## 4. Real-time visualisation data

The plugin exposes a lock-free engine snapshot the UI can read on a timer
(~30 fps recommended). All of the below are already wired and available.

| Datum | Type | Notes |
|---|---|---|
| Master output scope ring | 4096 mono samples | UI reads "last N" for waveform / spectrum |
| Master output peak | float (smoothed) | Decaying envelope |
| Ext In peak | float (smoothed) | Decaying envelope |
| Per-voice state × 16 | enum {Idle, Held, Ringing} | Drives a per-voice activity indicator |
| Per-voice group × 16 | int 0–5 | Which group owns the voice |
| Per-voice note × 16 | int (MIDI 0–127, −1 if idle) | Optional display |
| Per-voice resonator energy × 16 | float (smoothed |output|) | Brightness for ringing tails |
| Per-voice age × 16 | uint64 timestamp | For LRU visualisation if desired |

Suggested visual treatments:

- **Scope strip**: master waveform, full width, ~80 px. With high feedback
  the comb peaks and self-oscillation are immediately visible.
- **Voice activity row**: 16 dots/LEDs, coloured by state (e.g. blue=held,
  purple=ringing, dim=idle), brightness ∝ resonator energy.
- **Peak meters**: master and Ext In, vertical bars.
- **Per-destination LED ring** on each modulatable control (see §5.3).
- **Mod-matrix overview** showing all currently active routings (heatmap
  of sources × destinations with depth/polarity colour).

---

## 5. Hardware → screen gesture translations

### 5.1 Focused-group editing (preserve)

The hardware's "front panel edits the selected voice group" model is the
single most important workflow concept. The UI must mirror it:

- A persistent **Focused Group** selector (1–6) sits at the top.
- Every control on the panel edits whichever group is focused. Switching
  the selector retargets the entire panel.
- Group-routing controls (MIDI channel, poly, priority) follow the
  focused group; preset-level controls (preset channel, unison, prio)
  sit apart.

This is already implemented and host-automatable.

### 5.2 Source macros + mod-matrix routing (preserve, modernise)

Hardware flow: hold a source's SELECTION key, turn any destination knob
to dial in modulation depth.

Suggested screen flow:

- A **Modulation panel** with 9 source tiles (one per source) showing the
  macro attenuverter, an "active" highlight, and a detach toggle.
- "Add routing" mode: click a source tile → it lights up → click/drag on
  any destination control's **mod ring** to set depth/polarity. Click an
  empty area or the source tile again to exit.
- Each destination control has a small **mod ring** around it indicating
  total assigned modulation (sum-of-depths or per-source ribbon). Click
  the ring to open a per-destination routing list.
- A separate compact **routing list view** with rows of
  {source, dest, depth, enable, mute, delete} for power users.

> Macros default 0 (12 o'clock). The UI should make this obvious — the
> source tile should be visibly dim/inactive at 0 and clearly bright at
> full. Forgetting to raise the macro is the #1 confusion of the manual.

### 5.3 Original-value indicator (preserve, simplify)

Hardware: when adjusting a control, the source LEDs form a bar showing
distance to the saved value. On screen:

- Render a faint tick on each control at the saved position.
- Optional: a small "compare to saved" badge that lights when any control
  differs from saved.

Not yet wired in the engine; a "baselineSnapshot" capture on preset load
is a small addition.

### 5.4 A/B compare via RELOAD (preserve)

Hardware: RELOAD restores saved values; pressing again toggles back.
Screen: a clear **A/B** toggle in the preset bar.

### 5.5 Things that do *not* translate (replace)

| Hardware affordance | Reason it doesn't translate | Screen replacement |
|---|---|---|
| Number keypad as MIDI keyboard | Screen has no tactile pad | Use host MIDI; an on-screen mini-keyboard is optional |
| 6×6×6 bank navigation (three numeric presses) | No numeric keypad | Standard preset browser with folders/tags/search |
| Modal long-press patterns | Unintuitive on screen | Visible toggles / right-click menus |
| INIT button hold/blink | No tactile feedback | "Initialize" menu item |
| Front Panel Sync | No physical knobs to desync | Not applicable |
| Catch-up | Host automation already handles | Not applicable |

---

## 6. Must-haves vs. nice-to-haves

### Must-have for v1

- Every parameter in §3.1 and §3.2 reachable from the UI.
- Focused-group selector + visible group routing for the focused group.
- Preset master row (preset channel, unison, priority).
- Mod-matrix routing UI (source tiles + destination mod rings + routing list).
- Visual feedback strip (scope, voice activity, peak meters).
- Preset browser with save/recall, A/B compare.
- The bipolar attenuverter UI for source macros (centre detent at 0).
- Quad LFO controls with an LFO selector (1/2/3/4 + visible state).

### Nice-to-have for v1

- Original-value indicator.
- Mod-matrix overview heatmap.
- Microtuning import (Scala/MTS-ESP) — engine support deferred.
- Quality switch (Eco/Std/HQ) — engine support deferred.
- MPE indicator.
- Preset morph slider — engine support deferred.

### Out of scope for v1 (engine doesn't yet support)

- LFO MIDI-clock sync UI (controls exist in state but engine ignores).
- LFO single-shot mode UI.
- LFO cross-modulation patching.
- Microtuning UI.
- Quality / oversampling selector.
- Preset morph slider.

These are flagged in the roadmap. Reserve panel space if useful but
disable controls.

---

## 7. Identity & branding

- Plugin product name: **Tides**.
- Manufacturer: **Joel Eaton**.
- Bundle ids: `com.joeleaton.tides` (AU bundle) and the equivalent VST3.
- AU four-char codes: type `aumu`, subtype `Tds1`, manufacturer `Joel`.
- Plugin formats: AU + VST3 + Standalone (macOS arm64 currently;
  cross-platform later).

---

## 8. Implementation handshake

The UI will be a JUCE `AudioProcessorEditor` consuming:

- `juce::AudioProcessorValueTreeState` (`proc.apvts`) for all
  host-automatable controls — use `SliderAttachment`, `ButtonAttachment`,
  `ComboBoxAttachment`.
- `proc.patch()` (returns the focused group's `PatchState`) for the
  non-automated controls in §3.2. These are `std::atomic` members; the UI
  reads/writes them directly.
- `proc.preset()` for routing/structure (channels, poly, priority).
- `proc.snapshot()` for the lock-free visualisation data.
- `proc.stateEpoch` — bumps on focus switch / preset restore; the editor
  should call its sync function when this changes to refresh non-APVTS
  controls.

Focused-group switching is automatic: setting the `focused_group` APVTS
param triggers a message-thread push of that group's values into APVTS
so all attached controls update. The UI doesn't have to do anything
special on focus switch other than refresh the non-APVTS controls.

---

## 9. Open questions for the designer

- Visual language: dark UI (matches hardware photos) or light/neutral?
- Density: hardware is dense and tactile; screen has more room. Is it
  one big screen or paged sections?
- Mod-matrix overview: heatmap vs. ribbon-per-destination vs. list-only?
- Should the visualisation strip be persistent or hideable?
- Where to put microtuning / quality switch when they land — reserve
  space now?

End of hand-off.
