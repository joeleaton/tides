#include "state/FactoryPresets.h"
#include "state/ParameterIDs.h"
#include "mod/ModDefs.h"
#include <JuceHeader.h>

// ─── Factory preset library ─────────────────────────────────────────────
//
// Every builder function below resets the Preset to INIT first, then
// configures exactly one focused group (group 0) unless the preset is
// explicitly multi-group (LAYERED category).
//
// Param notes (normalised 0..1 unless stated):
//   ExWave:        0 sin · 0.25 sin↔tri · 0.5 tri · 0.7 tri↔ramp · 0.85 ramp↔pul · 1 sqr
//   ExCoarseFreq:  0 = −4 oct · 0.5 = 0 oct · 1 = +4 oct
//   ExFineFreq:    0 = −3 st  · 0.5 = 0 st  · 1 = +3 st
//   ExWaveNoiseMix:0 = full wave · 0.5 = mid · 1 = full noise
//   EnvAttack/D/R: log scaled by envRange (Short/Med/Long); EnvSustain is a level
//   ResFeedback:   0 muted · 0.5 resonant · 0.85+ self-oscillates
//   ResInputFilter / ResFeedbackFilter: log freq, 0 dark · 1 open
//   OutDryWet:     0 dry exciter · 1 full resonator
//   WildTame:      0 wild · 0.5 neutral · 1 tame
//
namespace tides::state::factory
{
    using namespace tides::params;
    using namespace tides::mod;
    using P = tides::params::P;

    // ── shorthand helpers ───────────────────────────────────────────────
    static inline void p (PatchState& g, P pp, float v) { g.set (pp, v); }

    static inline void env (PatchState& g, EnvRange range,
                            float onset, float a, float d, float s, float r,
                            bool hold = false, bool legato = false, bool loop = false)
    {
        g.envRange.store ((int) range);
        p (g, P::EnvOnsetDelay, onset);
        p (g, P::EnvAttack,     a);
        p (g, P::EnvDecay,      d);
        p (g, P::EnvSustain,    s);
        p (g, P::EnvRelease,    r);
        g.envHold  .store (hold);
        g.envLegato.store (legato || loop);
        g.envLoop  .store (loop);
    }

    static inline void exc (PatchState& g, float wave, float coarse, float fine,
                             float wnMix, float shRate, float level)
    {
        p (g, P::ExWave,         wave);
        p (g, P::ExCoarseFreq,   coarse);
        p (g, P::ExFineFreq,     fine);
        p (g, P::ExWaveNoiseMix, wnMix);
        p (g, P::ExNoiseSHRate,  shRate);
        p (g, P::ExLevel,        level);
    }

    static inline void res (PatchState& g, float inFilt, float fbFilt, float fb,
                             float fineTune = 0.5f, float glide = 0.0f, bool invert = false)
    {
        p (g, P::ResInputFilter,    inFilt);
        p (g, P::ResFeedbackFilter, fbFilt);
        p (g, P::ResFeedback,       fb);
        p (g, P::ResFineTune,       fineTune);
        p (g, P::ResGlide,          glide);
        g.resInvert.store (invert);
    }

    static inline void out (PatchState& g, float dryWet, float pan, float level,
                             float wildTame = 0.5f, float master = 0.8f)
    {
        p (g, P::OutDryWet,  dryWet);
        p (g, P::OutPanning, pan);
        p (g, P::OutLevel,   level);
        p (g, P::WildTame,   wildTame);
        p (g, P::Master,     master);
    }

    static inline void uni (PatchState& g, int voices, float detune, float spread)
    {
        g.unisonVoices.store (voices);
        g.unisonDetune.store (detune);
        g.unisonSpread.store (spread);
    }

    static inline void setLfo (PatchState& g, int i,
                                float rate, float amp, float mix,
                                int wave, bool bipolar = true,
                                float randomRate = 0.0f, float smooth = 0.0f,
                                bool kbReset = false)
    {
        auto& L = g.lfo[(size_t) i];
        L.rate      .store (rate);
        L.amplitude .store (amp);
        L.mix       .store (mix);
        L.wave      .store (wave);
        L.bipolar   .store (bipolar);
        L.randomRate.store (randomRate);
        L.smooth    .store (smooth);
        L.kbReset   .store (kbReset);
    }

    static inline void route (PatchState& g, Source s, Dest d, float depth)
    {
        const int n = g.numConnections.load();
        if (n >= mod::kMaxConnections) return;
        auto& c = g.connections[(size_t) n];
        c.source  = s;
        c.dest    = d;
        c.depth   = depth;
        c.enabled = true;
        c.muted   = false;
        g.numConnections.store (n + 1);
    }

    // Some sources benefit from a baseline source-macro attenuverter
    // (Mod-wheel etc. arrive at 0 from MIDI, so they only modulate when
    // the player moves them; LFOs are full-amplitude by default).
    static inline void macro (PatchState& g, Source s, float v)
    {
        g.sourceMacro[(size_t) s].store (v);
    }

    // Per-group display name (UI thread only). Overrides the default
    // GROUPS_INIT placeholders installed by `initRouting()`.
    static inline void name (Preset& pr, int g, const char* n)
    {
        if (g >= 0 && g < kMaxGroups) pr.groupNames[g] = juce::String::fromUTF8 (n);
    }

    // ── reset helpers ───────────────────────────────────────────────────
    static void resetAll (Preset& pr)
    {
        pr.initRouting();
        for (auto& g : pr.group) g.resetToInit();
        // Only group 0 audible by default; the multi-group builders enable
        // the others explicitly.
        pr.groupPoly[0].store (16);
        for (int i = 1; i < kMaxGroups; ++i) pr.groupPoly[i].store (0);
        pr.focused.store (0);

        // Clear lingering names so each preset starts from a known baseline.
        // The builders below set group 0's name and any extra groups they use.
        // `G·N` literal has a UTF-8 mid-dot — must go through fromUTF8.
        for (int i = 0; i < kMaxGroups; ++i)
            pr.groupNames[i] = juce::String::fromUTF8 ("G·") + juce::String (i + 1);
    }

    // ─── PLUCKED ────────────────────────────────────────────────────────
    static void buildKotoMallet (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Koto Mallet");
        auto& g = pr.group[0];
        exc (g, 0.1068f, 0.6f, 0.5f, 0.0f, 0.5f, 0.8f);
        env (g, EnvRange::Medium, 0.0f, 0.0405f, 0.6723f, 0.348f, 1.0f);
        res (g, 0.75f, 0.55f, 0.5632f);
        out (g, 0.78f, 0.5f, 0.75f, 0.4f);
        setLfo (g, 0, 0.3346f, 0.4f, 1.0f, 0, true);
        route (g, Source::Velocity, Dest::ExciterLevel, 0.35f);
    }

    static void buildKalimba (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Kalimba");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.4f, 0.5f, 0.0202f, 0.55f, 0.9f);
        env (g, EnvRange::Medium, 0.0f, 0.01f, 0.45f, 0.1121f, 0.9144f);
        res (g, 0.8f, 0.4f, 0.65f);
        out (g, 0.82f, 0.5f, 0.78f, 0.35f);
        uni (g, 1, 0.3f, 0.5f);
        route (g, Source::Velocity, Dest::ResFeedback, 0.25f);
        route (g, Source::Velocity, Dest::ExciterLevel, 0.4f);
    }



    static void buildPluckedGlass (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Plucked Glass");
        auto& g = pr.group[0];
        exc (g, 0.85f, 0.70f, 0.50f, 0.05f, 0.50f, 0.65f);  // square-ish, bright
        env (g, EnvRange::Short, 0.0f, 0.02f, 0.40f, 0.0f, 0.25f);
        res (g, 0.90f, 0.78f, 0.72f);
        out (g, 0.85f, 0.55f, 0.70f, 0.55f);
        uni (g, 2, 0.10f, 0.45f);
    }

    static void buildPizzCello (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Pizz Cello");
        auto& g = pr.group[0];
        exc (g, 0.3f, 0.35f, 0.5f, 0.1f, 0.55f, 0.85f);
        env (g, EnvRange::Medium, 0.0f, 0.0878f, 0.6703f, 0.0115f, 0.3f);
        res (g, 0.55f, 0.4f, 0.55f, 0.5f, 0.05f);
        out (g, 0.8f, 0.45f, 1.0f, 0.4f);
        uni (g, 2, 0.18f, 0.35f);
        route (g, Source::Velocity, Dest::EnvAttack, -0.3f);
    }


    static void buildMbira (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Mbira");
        auto& g = pr.group[0];
        exc (g, 0.12f, 0.5f, 0.5f, 0.18f, 0.6f, 0.8f);
        env (g, EnvRange::Medium, 0.0f, 0.0f, 0.5f, 0.0561f, 1.0f);
        res (g, 0.65f, 0.45f, 0.6f);
        out (g, 0.82f, 0.5f, 0.75f, 0.35f);
        uni (g, 2, 0.12f, 0.3f);
    }


    // ─── PADS ───────────────────────────────────────────────────────────
    static void buildAtrium (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Atrium");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.50f, 0.50f, 0.05f, 0.55f, 0.70f);
        env (g, EnvRange::Long, 0.0f, 0.55f, 0.40f, 0.85f, 0.65f, /*hold*/ false);
        res (g, 0.65f, 0.40f, 0.55f, 0.50f, 0.05f);
        out (g, 0.88f, 0.50f, 0.70f, 0.45f);
        uni (g, 2, 0.20f, 0.60f);
        setLfo (g, 0, 0.3151f, 0.45f, 1.0f, 0, true, 0.0f, 0.30f);
        route (g, Source::Lfo1, Dest::ResFineTune,    0.25f);
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.20f);
    }

    static void buildSlowCoast (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Slow Coast");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.40f, 0.50f, 0.10f, 0.45f, 0.65f);
        env (g, EnvRange::Long, 0.0f, 0.75f, 0.50f, 0.90f, 0.80f);
        res (g, 0.55f, 0.30f, 0.60f);
        out (g, 0.90f, 0.50f, 0.65f, 0.40f);
        uni (g, 3, 0.25f, 0.70f);
        setLfo (g, 0, 0.2694f, 0.5f, 1.0f, 0, true, 0.0f, 0.50f);   // ~slow drift
        setLfo (g, 1, 0.3607f, 0.3f, 1.0f, 0, true, 0.0f, 0.10f);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.35f);
        route (g, Source::Lfo2, Dest::ExciterFrequency, 0.10f);
    }

    static void buildGlassCathedral (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Glass Cathedral");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.65f, 0.50f, 0.04f, 0.55f, 0.65f);
        env (g, EnvRange::Long, 0.0f, 0.55f, 0.35f, 0.95f, 0.80f);
        res (g, 0.85f, 0.70f, 0.62f);
        out (g, 0.92f, 0.50f, 0.65f, 0.55f);
        uni (g, 2, 0.15f, 0.55f);
        setLfo (g, 0, 0.3477f, 0.4f, 1.0f, 1, true, 0.0f, 0.40f);   // triangle
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.30f);
    }

    static void buildBowedGlass (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Bowed Glass");
        auto& g = pr.group[0];
        exc (g, 0.20f, 0.55f, 0.50f, 0.05f, 0.55f, 0.75f);
        env (g, EnvRange::Medium, 0.0f, 0.55f, 0.35f, 0.95f, 0.50f);
        res (g, 0.75f, 0.55f, 0.68f);
        out (g, 0.88f, 0.50f, 0.72f, 0.50f);
        uni (g, 2, 0.18f, 0.50f);
        setLfo (g, 0, 0.3346f, 0.5f, 1.0f, 0, true, 0.0f, 0.30f);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.35f);
        route (g, Source::Aftertouch, Dest::ResFeedback, 0.30f);
    }

    static void buildSoftWind (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Soft Wind");
        auto& g = pr.group[0];
        exc (g, 0.50f, 0.60f, 0.50f, 0.65f, 0.30f, 0.55f);   // noisy
        env (g, EnvRange::Long, 0.0f, 0.65f, 0.30f, 0.85f, 0.75f);
        res (g, 0.55f, 0.45f, 0.40f);
        out (g, 0.85f, 0.50f, 0.62f, 0.35f);
        uni (g, 2, 0.12f, 0.55f);
        setLfo (g, 0, 0.2825f, 0.5f, 0.7f, 0, true, 0.20f, 0.50f);
        route (g, Source::Lfo1, Dest::ExciterWaveNoiseMix, 0.25f);
    }

    // ─── SELF-OSC ───────────────────────────────────────────────────────
    static void buildPureTone (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Pure Tone");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.50f, 0.50f, 0.04f, 0.50f, 0.05f);    // exciter near-silent
        env (g, EnvRange::Medium, 0.0f, 0.10f, 0.10f, 1.0f, 0.30f, /*hold*/ true);
        res (g, 0.50f, 0.45f, 0.98f);                          // self-osc
        out (g, 1.0f, 0.50f, 0.70f, 0.65f);
        uni (g, 1, 0.0f, 0.0f);
    }

    static void buildFmSqueal (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "FM Squeal");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.50f, 0.50f, 0.04f, 0.50f, 0.10f);
        env (g, EnvRange::Medium, 0.0f, 0.05f, 0.20f, 1.0f, 0.30f, true);
        res (g, 0.55f, 0.55f, 0.97f);
        out (g, 1.0f, 0.50f, 0.65f, 0.35f);                    // wild flavour
        setLfo (g, 0, 0.5144f, 0.7f, 1.0f, 0, true);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.45f);     // FM-like wobble
    }

    static void buildResonantHowl (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Resonant Howl");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.55f, 0.50f, 0.04f, 0.50f, 0.08f);
        env (g, EnvRange::Long, 0.0f, 0.55f, 0.20f, 1.0f, 0.65f, true);
        res (g, 0.45f, 0.40f, 0.96f);
        out (g, 1.0f, 0.50f, 0.65f, 0.30f);
        setLfo (g, 0, 0.2825f, 0.6f, 1.0f, 0, true, 0.0f, 0.45f);
        setLfo (g, 1, 0.2499f, 0.4f, 1.0f, 1, true, 0.0f, 0.30f);
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.45f);
        route (g, Source::Lfo2, Dest::ResFeedback,       0.10f);
    }

    static void buildWhistle (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Whistle");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.75f, 0.50f, 0.04f, 0.50f, 0.10f);     // high, near-silent
        env (g, EnvRange::Medium, 0.0f, 0.20f, 0.15f, 0.95f, 0.45f);
        res (g, 0.75f, 0.65f, 0.95f);
        out (g, 1.0f, 0.55f, 0.55f, 0.65f);
        setLfo (g, 0, 0.4129f, 0.5f, 1.0f, 0, true);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.20f);     // breathy waver
    }

    // ─── BASS ───────────────────────────────────────────────────────────
    static void buildSubLantern (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Sub Lantern");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.30f, 0.50f, 0.05f, 0.50f, 0.85f);      // pure sine bass
        env (g, EnvRange::Short, 0.0f, 0.02f, 0.45f, 0.40f, 0.25f);
        res (g, 0.50f, 0.30f, 0.25f);                           // light feedback warmth
        out (g, 0.55f, 0.50f, 0.75f, 0.55f);
    }

    static void buildResonatorBass (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Resonator Bass");
        auto& g = pr.group[0];
        exc (g, 0.30f, 0.35f, 0.50f, 0.05f, 0.50f, 0.80f);
        env (g, EnvRange::Short, 0.0f, 0.01f, 0.40f, 0.30f, 0.25f);
        res (g, 0.65f, 0.50f, 0.62f);
        out (g, 0.78f, 0.50f, 0.78f, 0.45f);
        route (g, Source::Velocity, Dest::ResFeedback, 0.20f);
    }

    static void buildAcidDrip (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Acid Drip");
        auto& g = pr.group[0];
        exc (g, 0.85f, 0.35f, 0.50f, 0.05f, 0.50f, 0.85f);     // sq-ish bass
        env (g, EnvRange::Medium, 0.0f, 0.01f, 0.40f, 0.10f, 0.20f);
        res (g, 0.55f, 0.40f, 0.78f);
        out (g, 0.78f, 0.50f, 0.78f, 0.50f);
        setLfo (g, 0, 0.4129f, 0.6f, 1.0f, 2, true);             // ramp wobble
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.50f);
        route (g, Source::Velocity, Dest::ResFeedbackFilter, 0.30f);
    }

    // ─── BELL / METALLIC ────────────────────────────────────────────────
    static void buildBellTower (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Bell Tower");
        auto& g = pr.group[0];
        exc (g, 0.05f, 0.60f, 0.50f, 0.08f, 0.55f, 0.75f);
        env (g, EnvRange::Medium, 0.0f, 0.01f, 0.80f, 0.0f, 0.85f);
        res (g, 0.85f, 0.70f, 0.78f);
        out (g, 0.85f, 0.50f, 0.70f, 0.50f);
    }

    static void buildCrystalTine (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Crystal Tine");
        auto& g = pr.group[0];
        exc (g, 0.10f, 0.70f, 0.50f, 0.05f, 0.55f, 0.65f);
        env (g, EnvRange::Short, 0.0f, 0.01f, 0.70f, 0.0f, 0.55f);
        res (g, 0.92f, 0.82f, 0.72f);
        out (g, 0.88f, 0.55f, 0.68f, 0.55f);
        uni (g, 2, 0.05f, 0.50f);
    }

    static void buildAnvil (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Anvil");
        auto& g = pr.group[0];
        exc (g, 0.60f, 0.45f, 0.50f, 0.35f, 0.45f, 0.95f);     // tri+noise hammer
        env (g, EnvRange::Short, 0.0f, 0.0f, 0.55f, 0.0f, 0.30f);
        res (g, 0.80f, 0.55f, 0.70f);
        out (g, 0.82f, 0.50f, 0.78f, 0.35f);
    }

    static void buildWineGlass (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Wine Glass");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.75f, 0.50f, 0.04f, 0.55f, 0.55f);
        env (g, EnvRange::Long, 0.0f, 0.45f, 0.30f, 0.90f, 0.85f);
        res (g, 0.92f, 0.78f, 0.85f);
        out (g, 0.92f, 0.50f, 0.60f, 0.60f);
        setLfo (g, 0, 0.3477f, 0.4f, 1.0f, 0, true, 0.0f, 0.40f);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.20f);
    }

    // ─── STRINGS ────────────────────────────────────────────────────────
    static void buildBowedString (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Bowed String");
        auto& g = pr.group[0];
        exc (g, 0.65f, 0.50f, 0.50f, 0.10f, 0.55f, 0.78f);     // ramp-ish for bow harmonics
        env (g, EnvRange::Medium, 0.0f, 0.30f, 0.30f, 0.85f, 0.45f);
        res (g, 0.70f, 0.55f, 0.55f);
        out (g, 0.78f, 0.50f, 0.75f, 0.45f);
        uni (g, 3, 0.20f, 0.50f);
        setLfo (g, 0, 0.3607f, 0.5f, 1.0f, 0, true);             // vibrato source
        route (g, Source::Aftertouch, Dest::ResFineTune, 0.20f); // bow pressure
    }

    static void buildMosaicStrings (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Mosaic Strings");
        auto& g = pr.group[0];
        exc (g, 0.55f, 0.50f, 0.50f, 0.08f, 0.55f, 0.75f);
        env (g, EnvRange::Medium, 0.0f, 0.35f, 0.35f, 0.85f, 0.55f);
        res (g, 0.65f, 0.45f, 0.55f);
        out (g, 0.80f, 0.50f, 0.72f, 0.45f);
        uni (g, 4, 0.30f, 0.75f);
        setLfo (g, 0, 0.3216f, 0.5f, 1.0f, 0, true, 0.0f, 0.30f);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.20f);
    }

    static void buildTapeLattice (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Tape Lattice");
        auto& g = pr.group[0];
        exc (g, 0.55f, 0.50f, 0.50f, 0.15f, 0.50f, 0.78f);
        env (g, EnvRange::Long, 0.0f, 0.55f, 0.40f, 0.85f, 0.65f);
        res (g, 0.65f, 0.48f, 0.70f);
        out (g, 0.85f, 0.50f, 0.72f, 0.25f);                   // pushed wild
        uni (g, 3, 0.28f, 0.65f);
        setLfo (g, 0, 0.2955f, 0.45f, 1.0f, 0, true, 0.0f, 0.45f);
        route (g, Source::Lfo1, Dest::ResFineTune, 0.30f);
    }

    // ─── RANDOM / GLITCH ────────────────────────────────────────────────
    static void buildStaticDrift (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Static Drift");
        auto& g = pr.group[0];
        exc (g, 0.0f, 0.55f, 0.50f, 0.55f, 0.40f, 0.70f);
        env (g, EnvRange::Long, 0.0f, 0.55f, 0.30f, 0.85f, 0.60f);
        res (g, 0.65f, 0.55f, 0.55f);
        out (g, 0.82f, 0.50f, 0.62f, 0.20f);
        setLfo (g, 0, 0.3477f, 0.6f, 0.10f, 0, true, 0.40f, 0.20f);
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.45f);
        route (g, Source::Lfo1, Dest::ResFineTune,        0.20f);
    }

    static void buildStutterSparks (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Stutter Sparks");
        auto& g = pr.group[0];
        exc (g, 0.95f, 0.55f, 0.50f, 0.45f, 0.55f, 0.75f);
        env (g, EnvRange::Short, 0.0f, 0.02f, 0.50f, 0.20f, 0.30f);
        res (g, 0.75f, 0.60f, 0.65f);
        out (g, 0.85f, 0.50f, 0.70f, 0.30f);
        setLfo (g, 0, 0.4455f, 1.0f, 0.0f, 3, true, 0.55f);      // pure random
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.50f);
        route (g, Source::Lfo1, Dest::ResFineTune,       0.30f);
    }

    static void buildQuantumFoam (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Quantum Foam");
        auto& g = pr.group[0];
        exc (g, 0.55f, 0.50f, 0.50f, 0.45f, 0.55f, 0.70f);
        env (g, EnvRange::Long, 0.0f, 0.60f, 0.40f, 0.80f, 0.65f);
        res (g, 0.65f, 0.55f, 0.72f);
        out (g, 0.88f, 0.50f, 0.65f, 0.15f);                   // very wild
        setLfo (g, 0, 0.3346f, 0.7f, 0.20f, 0, true, 0.45f, 0.30f);
        setLfo (g, 1, 0.2825f, 0.5f, 0.0f, 3, true, 0.60f, 0.10f);
        route (g, Source::Lfo1, Dest::ExciterFrequency,   0.20f);
        route (g, Source::Lfo2, Dest::ResFeedbackFilter,  0.40f);
        route (g, Source::Lfo2, Dest::ResFineTune,        0.15f);
    }

    // ─── LAYERED multi-group ────────────────────────────────────────────
    static void buildOctaveMirror (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Sub Mirror");
        name (pr, 1, "Octave Mirror");
        // Group 0: clean sub
        {
            auto& g = pr.group[0];
            exc (g, 0.0f, 0.30f, 0.50f, 0.05f, 0.50f, 0.80f);
            env (g, EnvRange::Medium, 0.0f, 0.25f, 0.35f, 0.85f, 0.50f);
            res (g, 0.55f, 0.40f, 0.45f);
            out (g, 0.75f, 0.45f, 0.70f, 0.50f);
        }
        // Group 1: upper octave, lighter
        {
            auto& g = pr.group[1];
            exc (g, 0.15f, 0.625f, 0.50f, 0.05f, 0.50f, 0.65f); // ~+1 oct
            env (g, EnvRange::Medium, 0.0f, 0.30f, 0.35f, 0.80f, 0.50f);
            res (g, 0.75f, 0.55f, 0.55f);
            out (g, 0.85f, 0.55f, 0.55f, 0.55f);
        }
        pr.groupPoly[0].store (10);
        pr.groupPoly[1].store (8);
        pr.groupChannel[0].store (1);
        pr.groupChannel[1].store (1);   // same channel -> doubled
    }

    static void buildChoir (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Soprano");
        name (pr, 1, "Alto");
        name (pr, 2, "Tenor");
        name (pr, 3, "Bass");
        for (int gi = 0; gi < 4; ++gi)
        {
            auto& g = pr.group[gi];
            exc (g, 0.25f, 0.50f, 0.50f + (gi - 1.5f) * 0.015f,   // small detune per group
                  0.10f, 0.50f, 0.55f);
            env (g, EnvRange::Long, 0.0f, 0.60f, 0.45f, 0.90f, 0.70f);
            res (g, 0.65f, 0.45f, 0.55f);
            out (g, 0.85f,
                 0.5f + (gi - 1.5f) * 0.15f,   // pan spread
                 0.20f, 0.45f);                // level: 20% (multi-group sum was hot)
            uni (g, 2, 0.15f, 0.45f);
            setLfo (g, 0, 0.2825f + gi * 0.02f, 0.45f, 1.0f, 0, true, 0.0f, 0.40f);
            route (g, Source::Lfo1, Dest::ResFineTune, 0.20f);
        }
        for (int i = 0; i < 4; ++i)
        {
            pr.groupPoly[i].store (6);
            pr.groupChannel[i].store (1);
        }
        pr.presetUnison.store (true);
    }

    static void buildStackLead (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Body");
        name (pr, 1, "Shimmer");
        name (pr, 2, "Sub");
        // Group 0: body — bright pluck
        {
            auto& g = pr.group[0];
            exc (g, 0.75f, 0.55f, 0.50f, 0.05f, 0.55f, 0.80f);
            env (g, EnvRange::Short, 0.0f, 0.02f, 0.50f, 0.20f, 0.35f);
            res (g, 0.78f, 0.60f, 0.62f);
            out (g, 0.80f, 0.45f, 0.72f, 0.45f);
        }
        // Group 1: shimmer — high tine
        {
            auto& g = pr.group[1];
            exc (g, 0.0f, 0.78f, 0.50f, 0.05f, 0.50f, 0.55f);
            env (g, EnvRange::Short, 0.0f, 0.02f, 0.60f, 0.0f, 0.55f);
            res (g, 0.90f, 0.75f, 0.70f);
            out (g, 0.85f, 0.55f, 0.50f, 0.50f);
        }
        // Group 2: sub support
        {
            auto& g = pr.group[2];
            exc (g, 0.0f, 0.30f, 0.50f, 0.05f, 0.50f, 0.80f);
            env (g, EnvRange::Short, 0.0f, 0.02f, 0.50f, 0.10f, 0.30f);
            res (g, 0.55f, 0.30f, 0.30f);
            out (g, 0.60f, 0.50f, 0.65f, 0.55f);
        }
        for (int i = 0; i < 3; ++i)
        {
            pr.groupPoly[i].store (6);
            pr.groupChannel[i].store (1);
        }
        pr.presetUnison.store (true);
    }

    static void buildSlowerFocus (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "Slower Focus");
        auto& g = pr.group[0];
        exc (g, 0.3141f, 0.5047f, 0.5f, 0.2423f, 0.5971f, 0.65f);
        env (g, EnvRange::Long, 0.0f, 0.9521f, 0.2095f, 0.0608f, 0.9044f);
        res (g, 0.427f, 0.78f, 0.72f, 0.5f, 0.5983f);
        out (g, 0.85f, 0.55f, 0.5224f, 0.4522f);
        uni (g, 2, 0.0978f, 0.7826f);
        setLfo (g, 0, 0.3088f, 1.0f, 1.0f, 0, true);
        setLfo (g, 3, 0.2779f, 1.0f, 1.0f, 0, true);
        macro (g, Source::Lfo1, 0.23f);
        macro (g, Source::Lfo2, 0.134f);
        macro (g, Source::Lfo3, 0.247f);
        macro (g, Source::Lfo4, 0.176f);
        route (g, Source::Lfo1, Dest::ResFeedbackFilter, 0.5f);
        route (g, Source::Lfo2, Dest::ResInputFilter, 0.5f);
        route (g, Source::Lfo3, Dest::ExciterWave, 0.5f);
        route (g, Source::Lfo4, Dest::ResGlide, 0.5f);
        route (g, Source::Lfo1, Dest::Lfo1Rate, 0.5f);
    }

    static void buildRythmCrunch (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "RYTHM CRUNCH");
        auto& g = pr.group[0];
        exc (g, 0.4642f, 0.2491f, 0.5f, 0.4544f, 1.0f, 1.0f);
        env (g, EnvRange::Long, 0.0f, 0.0f, 0.5f, 0.1237f, 0.6788f, false, false, true);
        res (g, 0.7014f, 0.2915f, 0.5836f);
        out (g, 0.8697f, 0.5f, 0.65f, 0.4f);
        uni (g, 2, 0.25f, 0.7f);
        setLfo (g, 0, 0.2608f, 0.6762f, 1.0f, 1, true, 0.3714f, 1.0f, false);
        setLfo (g, 1, 0.5018f, 1.0f, 1.0f, 0, true, 0.0f, 0.1f, false);
        macro (g, Source::Lfo1, 0.149f);
        macro (g, Source::Lfo2, 0.383f);
        route (g, Source::Lfo1, Dest::EnvDecay, 0.5f);
        route (g, Source::Lfo2, Dest::ResPanning, 0.5f);
    }

    static void buildHoldMe (Preset& pr)
    {
        resetAll (pr);
        name (pr, 0, "HOLD ME");
        auto& g = pr.group[0];
        exc (g, 0.4347f, 0.5f, 0.4873f, 0.3638f, 0.7987f, 1.0f);
        env (g, EnvRange::Medium, 0.0f, 0.0f, 0.6309f, 0.1204f, 1.0f, false, false, true);
        res (g, 0.6576f, 0.6937f, 0.8821f);
        out (g, 0.9466f, 0.5f, 0.7f, 0.0f);
        uni (g, 2, 0.0543f, 0.5f);
        macro (g, Source::Lfo1, 0.337f);
        macro (g, Source::Lfo2, 0.419f);
        route (g, Source::Lfo1, Dest::EnvDecay, 0.5f);
        route (g, Source::Lfo2, Dest::ResPanning, 0.5f);
    }

    // ─── Library registration ───────────────────────────────────────────
    static const std::vector<PresetInfo>& libImpl()
    {
        static const std::vector<PresetInfo> data = {
            // PLUCKED — single-group
            { "PLUCKED",  "Koto Mallet",   "pluck · short · resonant",        &buildKotoMallet,     false },
            { "PLUCKED",  "Kalimba",       "pluck · soft · wood",             &buildKalimba,        false },
            { "PLUCKED",  "Plucked Glass", "pluck · bright · glassy",         &buildPluckedGlass,   false },
            { "PLUCKED",  "Pizz Cello",    "pluck · bowed · short",           &buildPizzCello,      false },
            { "PLUCKED",  "Mbira",         "pluck · wood · detuned",          &buildMbira,          false },
            // PADS — single-group
            { "PADS",     "Atrium",        "pad · ambient · drift",           &buildAtrium,         false },
            { "PADS",     "Slow Coast",    "pad · long · evolving",           &buildSlowCoast,      false },
            { "PADS",     "Glass Cathedral","pad · glass · bright",           &buildGlassCathedral, false },
            { "PADS",     "Bowed Glass",   "pad · bowed · sustain",           &buildBowedGlass,     false },
            { "PADS",     "Soft Wind",     "pad · noisy · breath",            &buildSoftWind,       false },
            { "PADS",     "Slower Focus",  "pad · slow · focused",            &buildSlowerFocus,    false },
            { "PADS",     "Hold Me",       "pad · sustained · still",         &buildHoldMe,         false },
            // SELF-OSC — single-group
            { "SELF-OSC", "Pure Tone",     "self-osc · clean · sine",         &buildPureTone,       false },
            { "SELF-OSC", "FM Squeal",     "self-osc · wobble · FM",          &buildFmSqueal,       false },
            { "SELF-OSC", "Resonant Howl", "self-osc · howl · slow",          &buildResonantHowl,   false },
            { "SELF-OSC", "Whistle",       "self-osc · high · breath",        &buildWhistle,        false },
            // BASS — single-group
            { "BASS",     "Sub Lantern",   "bass · clean · deep",             &buildSubLantern,     false },
            { "BASS",     "Resonator Bass","bass · resonant · velocity",      &buildResonatorBass,  false },
            { "BASS",     "Acid Drip",     "bass · sq · wobble",              &buildAcidDrip,       false },
            // BELL / METALLIC — single-group
            { "BELL",     "Bell Tower",    "bell · long · bright",            &buildBellTower,      false },
            { "BELL",     "Crystal Tine",  "bell · high · glass",             &buildCrystalTine,    false },
            { "BELL",     "Anvil",         "bell · metal · hammer",           &buildAnvil,          false },
            { "BELL",     "Wine Glass",    "bell · high · ring",              &buildWineGlass,      false },
            // STRINGS — single-group
            { "STRINGS",  "Bowed String",  "string · bowed · unison",         &buildBowedString,    false },
            { "STRINGS",  "Mosaic Strings","string · ensemble · drift",       &buildMosaicStrings,  false },
            { "STRINGS",  "Tape Lattice",  "string · wild · tape",            &buildTapeLattice,    false },
            // RANDOM / GLITCH — single-group
            { "RANDOM",   "Static Drift",  "noise · S&H · drift",             &buildStaticDrift,    false },
            { "RANDOM",   "Stutter Sparks","random · square · fast",          &buildStutterSparks,  false },
            { "RANDOM",   "Quantum Foam",  "random · chaotic · wild",         &buildQuantumFoam,    false },
            { "RANDOM",   "Rythm Crunch",  "random · rhythmic · crunch",      &buildRythmCrunch,    false },
            // LAYERED — multi-group (rewrite the whole preset)
            { "LAYERED",  "Octave Mirror", "stack · 2-group · octave",        &buildOctaveMirror,   true  },
            { "LAYERED",  "Choir",         "stack · 4-group · choir",         &buildChoir,          true  },
            { "LAYERED",  "Stack Lead",    "stack · 3-group · lead",          &buildStackLead,      true  },
        };
        return data;
    }

    const std::vector<PresetInfo>& library() { return libImpl(); }

    int count() { return (int) libImpl().size(); }

    const PresetInfo& info (int index)
    {
        const auto& l = libImpl();
        const int n = (int) l.size();
        return l[(size_t) juce::jlimit (0, n - 1, index)];
    }

    void loadIntoPreset (int index, Preset& dst)
    {
        const auto& l = libImpl();
        if (l.empty()) return;
        const int n = (int) l.size();
        const auto& p = l[(size_t) juce::jlimit (0, n - 1, index)];
        if (p.build) p.build (dst);
    }

    // Copy every UI-visible field of one PatchState into another. The audio
    // engine reads these atomics so writes happen under memory_order_relaxed —
    // any visibility race just shows up as one block of stale params.
    void copyPatchState (const PatchState& src, PatchState& dst)
    {
        using namespace tides::params;
        for (int i = 0; i < kNumParams; ++i)
            dst.set (static_cast<P> (i), src.get (static_cast<P> (i)));

        dst.envRange    .store (src.envRange    .load());
        dst.envHold     .store (src.envHold     .load());
        dst.envLegato   .store (src.envLegato   .load());
        dst.envLoop     .store (src.envLoop     .load());
        dst.kbReset     .store (src.kbReset     .load());
        dst.resInvert   .store (src.resInvert   .load());
        dst.resNoteOffset.store(src.resNoteOffset.load());
        dst.keyFollowVco.store (src.keyFollowVco.load());
        dst.extIn       .store (src.extIn       .load());
        dst.unisonVoices.store (src.unisonVoices.load());
        dst.unisonDetune.store (src.unisonDetune.load());
        dst.unisonSpread.store (src.unisonSpread.load());

        for (int i = 0; i < (int) src.lfo.size(); ++i)
        {
            dst.lfo[(size_t) i].rate      .store (src.lfo[(size_t) i].rate      .load());
            dst.lfo[(size_t) i].randomRate.store (src.lfo[(size_t) i].randomRate.load());
            dst.lfo[(size_t) i].amplitude .store (src.lfo[(size_t) i].amplitude .load());
            dst.lfo[(size_t) i].mix       .store (src.lfo[(size_t) i].mix       .load());
            dst.lfo[(size_t) i].smooth    .store (src.lfo[(size_t) i].smooth    .load());
            dst.lfo[(size_t) i].wave      .store (src.lfo[(size_t) i].wave      .load());
            dst.lfo[(size_t) i].bipolar   .store (src.lfo[(size_t) i].bipolar   .load());
            dst.lfo[(size_t) i].kbReset   .store (src.lfo[(size_t) i].kbReset   .load());
        }

        // Mod-matrix connections (compact prefix).
        const int n = src.numConnections.load();
        for (int i = 0; i < n; ++i) dst.connections[(size_t) i] = src.connections[(size_t) i];
        for (int i = n; i < (int) src.connections.size(); ++i) dst.connections[(size_t) i] = {};
        dst.numConnections.store (n);

        // Source macros / detached flags.
        for (int i = 0; i < (int) src.sourceMacro.size(); ++i)
        {
            dst.sourceMacro   [(size_t) i].store (src.sourceMacro   [(size_t) i].load());
            dst.sourceDetached[(size_t) i].store (src.sourceDetached[(size_t) i].load());
        }
    }

    void loadIntoFocusedGroup (int index, Preset& dst, int dstGroup)
    {
        const auto& l = libImpl();
        if (l.empty()) return;
        const int  n   = (int) l.size();
        const auto& p  = l[(size_t) juce::jlimit (0, n - 1, index)];
        const int  g   = juce::jlimit (0, kMaxGroups - 1, dstGroup);

        // Build the preset into a sacrificial temp Preset, then copy only its
        // group 0 (and that group's routing) into dst at dstGroup.
        Preset temp;
        if (p.build) p.build (temp);

        copyPatchState (temp.group[0], dst.group[(size_t) g]);
        dst.groupChannel[g] .store (temp.groupChannel[0] .load());
        dst.groupPoly[g]    .store (temp.groupPoly[0]    .load());
        dst.groupPriority[g].store (temp.groupPriority[0].load());
        dst.groupNames[g]   = temp.groupNames[0];
    }
}
