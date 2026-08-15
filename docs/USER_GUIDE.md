# Tides — User Guide

A friendly tour of the instrument, from first sound to deep patching.

If you just want a parameter-by-parameter reference, see
[REFERENCE.md](REFERENCE.md).

---

## 1. What Tides Is

Tides is a polyphonic synthesiser built around a **BBD-style resonator**.
Instead of subtractive filters or wavetables, Tides excites a delay-line
resonator with a short tone, noise burst, or live input, and the
resonator sings the pitch you played. The result lives somewhere between
a Karplus-Strong pluck, a tuned reverb, and a self-oscillating filter
bank — articulate, glassy, organic.

The signal flow is:

```
       ┌───────────┐   ┌───────────┐   ┌───────────┐   ┌────────┐
MIDI → │  EXCITER  │ → │ RESONATOR │ → │  OUTPUT   │ → │ MASTER │ → out
       └───────────┘   └───────────┘   └───────────┘   └────────┘
              ▲              ▲                ▲
              └─── modulation matrix (9 sources × everything) ───┘
```

Everything else in the UI is in service of that path: envelopes shape
the exciter, LFOs and macros drive modulation, voice groups let you
layer or split, and presets save the whole arrangement.

---

## 2. Your First Five Minutes

1. **Load the plug-in.** In a host, instantiate Tides on a MIDI track.
   In Standalone, launch the app and pick your audio device.
2. **Play a note.** You'll hear the current preset (factory preset 0).
3. **Hit `Init`** in the top-right block. This wipes the focused group
   to a true blank slate — a sine exciter into a clean resonator with
   no modulation. Now you have a known starting point.
4. **Turn FEEDBACK up** on the Resonator pane. As you increase it the
   tone gains body and ring; near the top, the resonator self-oscillates.
5. **Turn DRY/WET to ~50%.** You'll hear the raw exciter blended with
   the resonator.
6. **Adjust the envelope** (Attack / Decay / Sustain / Release on the
   Envelope pane) until the shape feels right.
7. **Save it.** Hit `Save`, name it, press return. It's now in your
   USER preset library forever.

That's the whole loop. Everything else refines this.

---

## 3. The Interface

```
┌────────────────────────────────────────────────────────────┐ ◀ TOP BAR
│ TIDES │  ◀ PRESET NAME ▶  BROWSE  A/B  SAVE  INIT  NOTES  │
├────────────────────────────────────────────────────────────┤ ◀ GROUP BAR
│ [G1] [G2] [G3] [G4] [G5] [G6]   QUALITY  MORPH A▸B  • • • │
├──────────────┬───────────────┬──────────────┬──────────────┤
│   EXCITER    │  ENVELOPE     │   RESONATOR  │   OUTPUT     │ ◀ PANEL ROW
├──────────────┴───────────────┴──────────────┴──────────────┤
│                    QUAD LFO   /   MODULATION                │ ◀ MOD ROW
├────────────────────────────────────────────────────────────┤
│         SCOPE   ·   VOICES   ·   L/R/EXT METERS             │ ◀ VIZ STRIP
├────────────────────────────────────────────────────────────┤
│              PERF ROW (MPE · CC routing hints)              │
└────────────────────────────────────────────────────────────┘
```

### Top bar

- **TIDES** — brand block.
- **◀ Preset name ▶** — current preset. Arrows step prev/next through
  the **factory** library only (USER presets and INIT don't step).
- **BROWSE** — full menu of factory presets, grouped by category, with
  a **USER** submenu containing your saved sounds plus **Manage…** and
  **Reveal Folder** entries.
- **A/B** — compares your live edits against the saved baseline. First
  press swaps to the baseline (lit). Second press restores your edits.
  See §6.
- **SAVE** — writes the focused group as a user preset (overwrite
  confirmation if the name already exists).
- **INIT** — true blank patch. Loaded into the focused group only;
  other groups untouched.
- **NOTES** — pops up the preset's design notes (factory presets ship
  with these).

### Group bar

- **G1–G6** — six voice groups. Click to focus. Right-click to rename.
  All panes below act on the **focused group**. Other groups keep their
  state and continue to sound.
- **QUALITY** — Eco / Std / HQ. Trade CPU for clarity; see §7.
- **MORPH A▸B · slider** — preset morph; crossfades the focused group
  between two factory presets. See §5.

### The four panels

| Pane       | Purpose                                                |
|------------|--------------------------------------------------------|
| Exciter    | Pick the sound feeding the resonator (wave + noise mix, level, S/H rate). |
| Envelope   | Shape the exciter's amplitude (Onset, ADSR, hold/legato/loop). |
| Resonator  | The heart: input filter, feedback, feedback filter, fine tune, glide. |
| Output     | Dry/Wet, panning, voice level, Wild/Tame, master.       |

### Mod row

- **QUAD LFO** — four LFOs (LFO 1–4). Each has RATE, RANDOM, AMP,
  W↔RND mix, SMOOTH, a wave-shape selector, plus BIPOLAR and KB Rst
  switches.
- **MODULATION** — nine source tiles (LFO 1–4, ENV, VEL, KEY, AT, MW).
  Tap a source to **arm patch mode**, then click any knob to assign.

### Viz strip + perf row

- Live scope, per-voice activity grid, L/R/EXT peak meters.
- Perf row shows the MPE state and CC routing.

---

## 4. Sound Design Workflow

### Pick your exciter

The exciter is the input to the resonator. It determines articulation.

- **Sine (Wave = 0)** — clean, pluck-like. The classic "filter ping".
- **Saw / square** — bright, more harmonics for the resonator to grab.
- **Noise** — windy, breathy, percussive.
- **Wave/Noise mix** — knob between pure tone and noise. A small bit
  of noise gives every note a "breath".
- **S/H rate** — quantises the noise to a random-step signal.

Use the **envelope** to control how the exciter is applied:
- Short A/D, low S → percussive **pluck**.
- Long A, high S → sustained **bow**.
- Loop ON → exciter retriggers continuously while a key is held —
  great for tremolo or rhythmic textures.

### Resonator basics

- **Input filter** — pre-resonator tilt. Darken to keep the
  resonator from screaming on bright exciters.
- **Feedback** — the amount of resonance. Below ~70% you get a tuned
  reverb; near 100% the resonator self-oscillates (see SELF-OSC light).
- **Feedback filter** — taming the loop. Higher = brighter, longer.
- **Fine tune** — ±50 cents. For detuning against other groups.
- **Glide** — portamento between notes.

### Output

- **Dry/Wet** — exciter vs resonator. Dry = raw exciter; Wet =
  pure resonator. Useful for blending pluck attack with sustained ring.
- **Panning** — per-voice random spread is controlled via modulation,
  not here; this is the **group** pan.
- **Wild/Tame** — drives the resonator's saturator and noise floor.
  - Fully **tame** (left) = clean, ideal model.
  - **Wild** (right) = drift, satDrive, occasional self-osc.
  - High Wild may leave notes ringing — back off if they don't release.
- **Master** — group output gain. Final stage is post-effects.

---

## 5. Voice Groups & Preset Morph

Tides has **six voice groups**, each with its own preset, MIDI
channel, polyphony, and routing. The editor edits the *focused* group
— click G1…G6 to switch focus.

### Common uses

- **Layered patch** — Group 1 = pluck pad, Group 2 = drone. Set both
  to channel 1, both will play. Loaded as a LAYERED multi-preset
  the whole stack saves/loads together.
- **Split** — G1 on channel 1 for bass, G2 on channel 2 for lead.
- **Voice steal** — each group has its own pool; one fat patch can
  use all 16 voices, or split them 8/8 across two groups.

Right-click the chip to rename it (the name appears in patches).

### Preset Morph

The **MORPH A▸B** slider on the group bar crossfades the *focused
group* between two factory presets. Pick endpoints with the dropdowns
on either side; drag the slider to morph.

Morph is a **modulation target** — assign LFO/macro/etc to it for
automated morphing.

**Important behaviour:** while morph is active (slider > 0):
- Every continuous param (filters, ADSR, levels, etc.) crossfades.
- **LFO knobs visually freeze** — they show the live morphed value, so
  turning them has no immediate visible effect; their underlying preset
  value still updates. Return MORPH to 0 to edit cleanly.
- Mod **connections** are not morphed — your patching survives.
- **Group routing** (channel, poly, priority) is not morphed.
- **Discrete params** (wave types, switches) snap at MORPH = 0.5.

The MORPH slider tints **amber** when active so you always know.

---

## 6. A/B Compare & Init

- **A/B** — compares your *current* edits against the saved baseline
  (whatever was loaded most recently from disk). One click swaps you to
  the baseline; the second click restores your edits. The button lights
  when you're viewing the saved version.
- **INIT** — wipes the focused group to a true blank slate
  (sine exciter, default ADSR, clean resonator, neutral macros, no mod
  connections). The preset bar shows `INI · INIT` in amber when active.

Both operations affect only the focused group; the other five groups
are preserved.

---

## 7. Quality Mode

QUALITY selects the resonator's internal upsample factor:

| Mode | Cost  | Sound                                          |
|------|-------|------------------------------------------------|
| Eco  | 1×    | CPU-cheap, slight aliasing on high notes.      |
| Std  | 2×    | Default. Clean across the keyboard.            |
| HQ   | 4×    | Extra clarity in the top octave; ~2× CPU.      |

Switch live — the engine cross-fades over a few ms.

---

## 8. Modulation

Nine sources, every continuous param a destination, signed depth.

### Sources

| Tile  | What it is                                              |
|-------|---------------------------------------------------------|
| LFO 1–4 | Free-running or retriggered LFOs from the QUAD LFO pane. |
| ENV   | Per-voice ADSR envelope (the same one shaping the exciter). |
| VEL   | Key velocity, sampled at note-on.                        |
| KEY   | Note number (key-follow), centred at C3.                |
| AT    | Channel aftertouch (or MPE per-note pressure).          |
| MW    | Mod wheel (CC1).                                        |

### Macros

Each source has a **macro** — a master amount that scales everything
that source modulates. Default = 0 so a new source does nothing until
you bring its macro in. Useful for performance: assign LFO 1 to a
dozen knobs, then sweep one macro to swell the whole patch.

### Patch mode (click-to-assign)

1. Click a source tile (LFO 1 say). The tile lights amber, the outline
   on every modulatable knob lights, and a banner appears: "Click a
   knob to assign".
2. Click any knob. A new connection is created at +50% depth (or the
   existing one bumps by +50%).
3. **Bump cycling** — re-clicking the same knob bumps depth:
   0.5 → 1.0 → 0.5 → 0 → -0.5 → -1.0 → -0.5 → 0 → 0.5 …
   The triangle-wave makes both polarities reachable from one
   gesture.
4. Right-click anywhere outside (or the source tile again) to disarm.

### Live mod feedback

- Every modulated knob shows a **ribbon arc** indicating the
  modulation's current excursion, and an **animated dot** at the live
  modulated value. The dot updates at 30 fps and reflects the
  most-recently-started active voice.
- Hover or right-click a connection (on the source tile) to mute,
  invert, or remove it.

### "Random initial value"

When you re-press a key while a free-running LFO modulates something,
the initial value can look random — that's because the LFO is in
free-run mode and the new voice samples it at whatever phase it's in.
Set the LFO's **KB Rst** switch ON to reset phase on each note-on.

---

## 9. MPE

Tides supports MPE on **lower zone** (master channel = 1, member
channels 2–15). Per-note expression maps as:

| MPE input        | Tides destination |
|------------------|-------------------|
| Per-note pitch   | Voice pitch (default ±48 semi range) |
| Per-note pressure| AT source         |
| Per-note slide (CC74) | Mapped via Modulation |
| Per-note velocity | VEL source        |

Toggle MPE in the perf row. With MPE off, all expression is channel-
wide.

---

## 10. Presets

### Factory presets

33 designed presets across eight categories (PLUCKED, PADS, SELF-OSC,
BASS, BELL, STRINGS, RANDOM, LAYERED). Each ships with design notes
(NOTES button).

### User presets

- **SAVE** → name → return. Confirmation prompt if you'd overwrite.
- Stored at `~/Library/Application Support/Tides/Presets/` as `.tides`
  XML files.
- **BROWSE → USER** lists them, plus **Manage Presets…** (rename /
  delete / reveal individual files) and **Reveal Folder in Finder**.

User presets store the **focused group** only — they do not capture
the other five groups. This is intentional: a user preset is a
"voice" you can drop into any group of any project.

### A/B reload

The baseline is the *most recently loaded preset* (factory, user, or
init). Edits don't change the baseline until you save or load again.

---

## 11. Performance Tips

- **CPU** — QUALITY = Eco for live work, HQ for mixing.
- **Polyphony** — set per-group on the group bar. Lower = fewer voice
  steals + less CPU; default 16 is plenty for most layered patches.
- **Voice steal** — group-internal; you can play 16 of G1 plus 16 of
  G2 simultaneously.
- **High Wild + high Feedback** can leave notes ringing after release
  because the resonator self-oscillates. Pull either back if it
  bothers you.
- **LFOs while morphing** are display-frozen; return MORPH to 0 to
  edit.

---

## 12. Troubleshooting

- **Stuck notes after preset switch** — should not happen; the engine
  silences voices on every preset load. If you see it, file a bug.
- **Notes hang at high Wild** — the resonator is self-oscillating.
  Reduce FEEDBACK or pull Wild back.
- **My LFO knob "snaps back"** — you're in morph mode (slider > 0).
  Set MORPH to 0 to edit.
- **Saved preset doesn't load** — confirm it's in
  `~/Library/Application Support/Tides/Presets/`. Re-save if it
  predates a format change.
- **No sound** — check DRY/WET (if exciter is silent, only resonator
  rings will sound), MASTER, and that the channel matches your MIDI
  track.

---

## 13. Where Next

- **REFERENCE.md** — every parameter, every range, every default.
- **NOTES** button on any factory preset — designer commentary on why
  each patch is built the way it is.
- Right-click for context menus on most controls.
