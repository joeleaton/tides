# Tides — Reference

Parameter-by-parameter reference. For a friendly tour, read
[USER_GUIDE.md](USER_GUIDE.md) first.

All values are normalised 0..1 internally and exposed to host
automation that way. Display strings are calculated by per-parameter
formatters.

---

## Signal Flow

```
MIDI ┌─────────┐ pre-resonator    ┌─────────┐  feedback      ┌────────┐
─────┤ EXCITER ├──────────────────┤RESONATOR├──── loop ──────┤ OUTPUT ├──► out
     └─────────┘                  └─────────┘                └────────┘
          ▲                           ▲                          ▲
          │                  Quad LFO · Env · Vel · Key · AT · MW
          └──────── nine sources × nine macros × matrix ─────────┘
```

The exciter feeds the resonator. The exciter is also blended directly
into the output (Dry/Wet). The resonator's feedback loop is what
defines the instrument's character.

---

## Parameter Index

| Pane      | ID              | Display          | Default | CC | Notes |
|-----------|-----------------|------------------|---------|----|-------|
| Exciter   | `ex_wave`       | Wave             | 0.00    | 15 | Morph: sine → tri → ramp → pulse → square → ext-in |
| Exciter   | `ex_coarse`     | Coarse Freq      | 0.50    | 17 | ±24 semitones offset from MIDI note |
| Exciter   | `ex_fine`       | Fine Freq        | 0.50    | 18 | ±100 cents detune |
| Exciter   | `ex_sh_rate`    | Noise S&H Rate   | 1.00    | 19 | Free-running quantiser; 1.0 = full bandwidth |
| Exciter   | `ex_wave_noise` | Wave/Noise Mix   | 0.00    | 20 | 0 = pure tone, 1 = pure noise |
| Exciter   | `ex_level`      | Exciter Level    | 0.70    | 21 | Pre-resonator drive |
| Envelope  | `env_onset`     | Onset Delay      | 0.00    | 22 | 0..1 s, delays note-on |
| Envelope  | `env_attack`    | Attack           | 0.02    | 23 | Exponential, 1 ms..6 s |
| Envelope  | `env_decay`     | Decay            | 0.30    | 24 | Exponential, 1 ms..6 s |
| Envelope  | `env_sustain`   | Sustain          | 1.00    | 25 | Linear level |
| Envelope  | `env_release`   | Release          | 0.15    | 26 | Exponential, 1 ms..10 s |
| Resonator | `res_in_filter` | Input Filter     | 1.00    | 31 | Pre-loop low-pass; 1.0 = open |
| Resonator | `res_fb_filter` | Feedback Filter  | 0.70    | 40 | In-loop tilt; higher = brighter & longer |
| Resonator | `res_feedback`  | Feedback         | 0.00    | 43 | Resonance amount; near 1.0 self-oscillates |
| Resonator | `res_fine_tune` | Fine Tune        | 0.50    | 46 | ±50 cents detune from MIDI note |
| Resonator | `res_glide`     | Glide            | 0.00    | 5  | Portamento time |
| Output    | `out_drywet`    | Dry/Wet          | 0.00    | 48 | 0 = dry (raw exciter), 1 = wet (resonator only) |
| Output    | `out_pan`       | Panning          | 0.50    | 10 | Group pan (per-voice spread via mod) |
| Output    | `out_level`     | Level            | 0.80    | 7  | Group output |
| Output    | `wild_tame`     | Wild/Tame        | 0.40    | —  | 0 = tame/clean, 1 = wild/drifty |
| Master    | `master`        | Master           | 0.80    | —  | Post-effects final gain |

Total: **20 continuous params** + 9 macros + Focused Group.

---

## Exciter

The exciter is the voice's energy source — what gets fed into the
resonator.

### Wave (`ex_wave`)
Continuous morph through five waveforms plus an Ext-In tap:

| Value | Wave                |
|-------|---------------------|
| 0.00  | Sine                |
| 0.20  | Triangle            |
| 0.40  | Ramp                |
| 0.60  | Pulse (25% duty)    |
| 0.80  | Square              |
| 1.00  | External input bus  |

In-between values crossfade. Ext-In replaces the internal oscillator
with whatever's on the plug-in's external sidechain input.

### Coarse Freq (`ex_coarse`)
Bipolar ±24 semitones offset from the MIDI note. Snaps to integer
semitones at click-to-bump steps; otherwise continuous.

### Fine Freq (`ex_fine`)
Bipolar ±100 cents detune. Useful in layered patches for chorus-like
beating.

### Noise S&H Rate (`ex_sh_rate`)
Sample-and-hold rate for the noise source. Lower values quantise the
noise into discrete random steps (chiptune-like); higher values let it
breathe.

### Wave/Noise Mix (`ex_wave_noise`)
Crossfades between pure oscillator (0) and pure noise (1). The midpoint
gives a breathy hybrid.

### Exciter Level (`ex_level`)
Drive into the resonator. Higher → more saturation and louder rings.

---

## Envelope (Exciter ADSR)

ADSR shaped envelope with onset delay. Triggered on note-on, gated by
note-off. Default to plucky values.

### Onset Delay (`env_onset`)
0–1 s delay before the envelope starts. Useful for layered patches
where one group should attack late.

### Attack (`env_attack`)
Time to peak. Exponential curve.

### Decay (`env_decay`)
Time from peak to sustain level.

### Sustain (`env_sustain`)
Level held while the key is down. Linear normalised 0..1.

### Release (`env_release`)
Time from sustain to zero after note-off.

### Hold (switch)
Latches the envelope on the sustain stage indefinitely after note-off
— useful for organ-style infinite holds.

### Legato (switch)
On: envelope restarts only when going from silence to first held note;
new notes during held notes don't retrigger.

### Loop (switch)
On: after release, envelope loops back to attack while the key is
still held. Makes the exciter pulse, useful for tremolo.

---

## Resonator (BBD)

Bucket-brigade-style delay-line resonator. The note is pitched by the
loop length; feedback determines resonance.

### Input Filter (`res_in_filter`)
Low-pass on the signal entering the loop. 1.0 = fully open. Pull down
to keep bright exciters from over-driving the resonator.

### Feedback Filter (`res_fb_filter`)
Low-pass in the feedback path. Higher → more high-frequency content
preserved per pass → longer, brighter ring.

### Feedback (`res_feedback`)
The resonance amount.

- **0.00** — single-pass delay, no resonance.
- **~0.50** — tuned reverb / Karplus-Strong pluck.
- **~0.85** — long sustained ring.
- **~1.00** — self-oscillates indefinitely (without an input).

### Fine Tune (`res_fine_tune`)
±50 cent detune of the resonator vs MIDI note. For chorus when two
groups play in unison.

### Glide (`res_glide`)
Portamento between successive notes (mono-style; in poly mode it
applies per-voice from the previous-pitch reference).

### Self-osc indicator
The lamp on the resonator pane lights when the resonator is producing
output without an active exciter excitement — typically at Feedback ≥
~0.90 or high Wild settings.

---

## Output

Per-group output stage with one-pole 1 ms smoothing on level/pan to
prevent zipper noise under fast modulation.

### Dry/Wet (`out_drywet`)
Blends the raw exciter (0) with the resonator output (1). At 0.5 you
hear both, giving the pluck attack with the ringing tail.

### Panning (`out_pan`)
Group L/R balance, equal-power.

### Level (`out_level`)
Group output gain pre-master.

### Wild/Tame (`wild_tame`)
Macro for resonator character. Internally drives:

- **Noise floor** — small uncorrelated noise in the loop.
- **Drift** — slow random pitch wander.
- **Static detune** — per-voice fixed pitch offset (seeded by voice
  index, so the same voice is always the same offset).
- **Saturator drive** — soft-clip drive in the feedback loop. Higher
  drive can push the loop into self-oscillation at lower Feedback.

| Value | Display          | Character                      |
|-------|------------------|--------------------------------|
| 0.00  | TAME             | Clean, precise, model-accurate |
| 0.50  | WILD 50%         | Subtle drift + saturation      |
| 1.00  | WILD             | Drifty, hairy, occasional self-osc |

At very high Wild + Feedback the resonator can leave residual ring
after note-off. Pull either down to recover.

### Master (`master`)
Final post-effects gain.

---

## Quad LFO

Four LFOs (LFO 1–4) on the QUAD LFO pane. Each has five knobs, a wave
selector, and two switches.

| Knob       | Range                                       | Default |
|------------|---------------------------------------------|---------|
| **RATE**   | 0.005 Hz – 50 Hz, piecewise log¹            | 1 Hz    |
| **RANDOM** | S&H random clock rate (0 = stopped)         | 0       |
| **AMP**    | 0..1 output amplitude                       | 1.0     |
| **W↔RND**  | Crossfade between periodic wave and S&H     | 1.0 (full wave) |
| **SMOOTH** | 0 = stepped, 1 = max slew on S&H            | 0       |

¹ The RATE curve is piecewise: knob 0..0.5 covers 0.005 Hz to 1 Hz
(slow drifts down to a ~200-second cycle), knob 0.5..1.0 covers
1 Hz to 50 Hz (vibrato/tremolo speed). This gives lots of resolution
for slow drifts without burning the upper half on rates you never use.

Wave selector: **Sin / Tri / Ramp / Square**.

Switches:
- **BIPOLAR** — output is signed −1..+1. When off, output is
  unipolar 0..1 (useful as an envelope-style modulator).
- **KB Rst** — phase resets to 0 on each note-on. When off, the LFO
  is free-running and new voices sample it at whatever phase it
  happens to be in (which is why a re-pressed key can yield an
  apparently random initial mod value).

LFOs feed the modulation matrix via the **LFO 1–4** source tiles.

> Host-tempo sync, single-shot mode, and cross-mod are deferred to a
> future release.

---

## Modulation Matrix

### Sources (9)

| Source ID | Tile name | Per-voice? | Range            |
|-----------|-----------|------------|------------------|
| 0         | LFO 1     | no         | bipolar -1..+1   |
| 1         | LFO 2     | no         | bipolar -1..+1   |
| 2         | LFO 3     | no         | bipolar -1..+1   |
| 3         | LFO 4     | no         | bipolar -1..+1   |
| 4         | ENV       | yes        | unipolar 0..1    |
| 5         | VEL       | yes        | unipolar 0..1    |
| 6         | KEY       | yes        | bipolar (centred at MIDI 60) |
| 7         | AT        | yes (MPE)  | unipolar 0..1    |
| 8         | MW        | no         | unipolar 0..1    |

### Macros (9)

One per source. Default = 0 (i.e. nothing routes until you bring it
in). The macro multiplies every connection from that source — perfect
for performance macros.

### Destinations (34)

All 20 continuous patch params, plus per-source macros and the **MORPH**
crossfade. Discrete params (wave switches, envelope switches) are not
modulation targets.

### Connection rules

- Each connection has `source × destination × depth`.
- Depth is signed −1..+1; the formula is `dest += src * macro * depth`.
- Max **kMaxConnections = 34** total per group.
- Existing connections are bumped (re-clicking the same knob) rather
  than duplicated.

### Bump cycle

Triangle-wave step for click-to-bump:
`0 → +0.5 → +1.0 → +0.5 → 0 → -0.5 → -1.0 → -0.5 → 0 → +0.5 …`

Both polarities reachable from a single source-target pair.

### Mute / invert / remove

Each routing exposed inline on its source tile has small buttons:

- **M** — mute (keeps the connection but bypasses it)
- **±** — invert depth sign
- **×** — remove

---

## Voice Groups

Six independently-routed voice groups (G1..G6). Each owns:

- A full PatchState (20 params + LFOs + mod matrix + group name).
- A MIDI channel (1..16, or "Omni"). Multiple groups on the same
  channel layer.
- A polyphony count (1..16).
- A priority (for voice-steal ordering).

The editor edits the **focused group** — the highlighted chip. APVTS
parameters mirror the focused group, so host automation tracks the
focus.

### Group routing rules

- A group with channel == incoming MIDI channel plays the note.
- "Omni" routes everything.
- Within a group, polyphony is enforced via voice steal (oldest first
  by default).
- Inter-group voice steal does **not** happen.

### Group names

Right-click a chip → Rename. Names persist with the preset and show
in the modulation overview.

---

## Preset Morph

Crossfades the focused group between two factory preset endpoints
(A and B), controlled by the **MORPH** slider in the group bar.

### What morphs

- All 20 continuous params (linear normalised crossfade).
- All four LFO rate/depth/phase/smooth floats.
- All nine source macros.
- Discrete switches (wave type, hold/legato/loop, sync/retrig) **snap
  at 0.5**.

### What does NOT morph

- **Mod connections** — your patching is preserved.
- **Group routing** (channel, poly, priority, name).
- **MIDI CC mappings**.

### Visual behaviour

- The MORPH slider lights **amber** when active (> 0).
- LFO knobs visually freeze on their morphed value while morph is
  active — turning them appears to have no effect, because the live
  display reflects the crossfade output. Return MORPH to 0 to edit
  cleanly.

### Morph as a modulation target

The slider's value itself can be modulated — assign LFO/macro/etc to
the **Morph** destination (index 33). The slider's position becomes
`morph_param + sum(connections to Morph)` clamped to 0..1.

---

## Quality Mode

QUALITY selects the resonator's internal oversampling.

| Mode | Upsample | CPU (rel) | Top-octave clarity |
|------|----------|-----------|--------------------|
| Eco  | 1×       | 1.0       | Audible aliasing on highest notes |
| Std  | 2×       | 1.4       | Clean across the keyboard          |
| HQ   | 4×       | 2.0       | Extra clarity, smoother resonance  |

Switching is live with a few-ms cross-fade.

---

## MPE

Lower-zone routing (master ch 1, member ch 2–15).

| MPE message            | Routed to                |
|------------------------|--------------------------|
| Note-on                | Voice trigger            |
| Per-note pitchbend     | Voice pitch (range from host) |
| Per-note pressure (CC) | AT source                |
| Per-note CC74 (slide)  | Available as a mod source via the matrix |
| Velocity (note-on)     | VEL source               |
| Release velocity       | (ignored)                |

With MPE off, channel pressure and pitchbend apply globally to all
playing voices on that channel.

---

## CC Routing

Tides routes a fixed set of MIDI CCs to its 20 continuous params,
mirroring the manual's CC implementation chart. CCs are mapped at the
audio thread, write into both PatchState and APVTS so host automation
catches them.

See the table at the top of this document — the `CC` column is the
authoritative list. CCs marked `—` are local-only (not specified in
the manual).

---

## Persistence

### Plug-in state (host save)

`getStateInformation`/`setStateInformation` serialise the whole Preset
(all six groups, focus index, group routing, MIDI mappings, mod matrix)
as a `TidesState` ValueTree → XML.

### User presets (file)

Top-level wrapper `<TidesUserPreset version="1" name=… groupName=…
groupChannel=… groupPoly=… groupPriority=…>` containing a single
`<TidesState>` child = the focused group's PatchState. Stored as
`~/Library/Application Support/Tides/Presets/<name>.tides`.

### Factory presets

Compiled into the binary; defined in `src/state/FactoryPresets.cpp`.
Each has `name`, `category`, `tags`, optional `multiGroup` flag, and
a `description` shown in the NOTES popup.

---

## File Layout

```
src/
├── PluginProcessor.{h,cpp}    — APVTS, mod matrix, persistence
├── PluginEditor.{h,cpp}       — top-level layout
├── dsp/                       — Exciter, Envelope, BBDResonator, OutputStage
├── engine/                    — SynthEngine, Voice, EngineSnapshot
├── mod/                       — ModDefs, ModMatrix
├── state/                     — PatchState, Preset, FactoryPresets
├── ui/                        — TopBar, GroupBar, panels/, Knob, Theme
└── params/                    — ParameterIDs
```

```
docs/
├── USER_GUIDE.md              — friendly tour
└── REFERENCE.md               — this file
```

---

## Glossary

- **APVTS** — JUCE's AudioProcessorValueTreeState. Holds the host-
  automatable param surface (the 20 continuous + 9 macros + focus).
- **BBD** — Bucket-Brigade Device. An analogue delay technique; we
  emulate its characteristics in DSP.
- **Focused group** — the one the editor currently controls; one of
  G1..G6.
- **Macro** — per-source amount multiplier applied to every connection
  from that source.
- **Patch mode** — armed click-to-assign state in the modulation
  matrix.
- **Preset** — a six-group state. **User preset** — a single focused
  group only.
- **Voice group** — one of six independent synth "layers" with its own
  PatchState, MIDI channel, polyphony, priority.

---

## Known Behaviours (read this!)

1. **LFO knobs freeze visually during morph (slider > 0).** The
   displayed value reflects the live morph crossfade. To edit the
   underlying preset, set MORPH back to 0.
2. **Random-looking initial value on re-press.** Free-running LFOs
   (KB Rst OFF) sample at whatever phase they're at when the voice
   starts. Set KB Rst ON for deterministic note starts.
3. **High Wild + high Feedback can leave residual ring.** The
   resonator is self-oscillating; pull either back to recover.
4. **A/B baseline is the most recently loaded preset.** It does not
   include your edits until you save or reload.
5. **User presets only store the focused group.** They're a "voice",
   not a full project state. Use host save/load for the full preset.
6. **The INIT button is destructive but local.** It wipes the focused
   group only — your other five groups are preserved.
7. **MORPH is a mod target.** Assign any source to Morph (dest 33) for
   automated crossfading.
8. **Inter-group voice steal does not happen.** Each group's poly
   pool is independent.
9. **Discrete params snap at MORPH = 0.5.** Wave switches, envelope
   modes etc. flip half-way through the morph.
10. **Mod connections survive morph.** Only continuous values, LFO
    floats, macros, and switches participate.
