#include "engine/Voice.h"
#include "dsp/ParamMap.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace tides::engine
{
    void Voice::prepare (double sr, int voiceIndex)
    {
        sampleRate = sr;
        exciter.prepare(sr);
        resonator.prepare(sr);
        output.prepare(sr);
        lfos.prepare(sr);
        // Fix this voice's *direction* of analogue detune (the magnitude
        // is then scaled per block by the Wild/Tame macro).
        resonator.setVarianceSeed((uint32_t) (voiceIndex + 1));
        active = false;
    }

    void Voice::recomputePitch (const state::ResolvedParams& r) noexcept
    {
        using namespace tides::params;
        using namespace tides::dsp::map;

        // Unison detune offsets the whole voice (exciter + resonator stay
        // co-tuned; the stack of voices beats against each other). MPE
        // per-voice pitch bend adds on top so each finger can slide.
        const float n0     = (float) curNote + uniDetune + noteBendSemis;
        const float noteHz = midiNoteHz(n0);

        // Resonator clock always tracks the keyboard, then Note Offset
        // (quantised semitones) and Fine Tune (±3 st, modulatable).
        const int   off  = r.resNoteOffset;
        const float fineT= fineTuneSemis(r.get(P::ResFineTune));
        const float resHz= midiNoteHz(n0 + (float) off + fineT);
        resonator.setResonatorPitch(resHz);

        // VCO: key-tracked when Key Follow routed (INIT), else free of the
        // keyboard. Coarse (±oct) + Fine (±3 st) always offset it.
        const float oct  = coarseOctaves(r.get(P::ExCoarseFreq));
        const float fine = fineSemis(r.get(P::ExFineFreq));
        const float base = r.keyFollowVco ? noteHz : 220.0f;
        exciter.setVcoFrequency(base * std::pow(2.0f, oct + fine / 12.0f));
    }

    void Voice::resolve (const state::PatchState& s) noexcept
    {
        using namespace tides::dsp::map;

        // Push per-LFO parameters from the patch.
        for (int i = 0; i < dsp::QuadLFO::kNum; ++i)
        {
            const auto& L = s.lfo[(size_t) i];
            lfos.setParams(i,
                lfoRateHz   (L.rate.load(std::memory_order_relaxed)),
                lfoRandomHz (L.randomRate.load(std::memory_order_relaxed)),
                L.amplitude.load(std::memory_order_relaxed),
                L.mix.load(std::memory_order_relaxed),
                L.wave.load(std::memory_order_relaxed),
                L.bipolar.load(std::memory_order_relaxed),
                L.smooth.load(std::memory_order_relaxed),
                L.kbReset.load(std::memory_order_relaxed));
        }

        // Gather the nine modulation sources (indexed by mod::Source).
        // Aliased onto srcCache so the engine can publish these values into
        // the snapshot for animated mod ribbons in the UI.
        auto& src = srcCache;
        src.fill (0.0f);
        src[(size_t) mod::Source::Envelope]   = exciter.envValue();
        src[(size_t) mod::Source::Lfo1]       = lfos.value(0);
        src[(size_t) mod::Source::Lfo2]       = lfos.value(1);
        src[(size_t) mod::Source::Lfo3]       = lfos.value(2);
        src[(size_t) mod::Source::Lfo4]       = lfos.value(3);
        // MPE: per-voice timbre/aftertouch override the group-wide values
        // so each finger expresses independently. Non-MPE voices still pull
        // from the group atomics, preserving the original behaviour.
        src[(size_t) mod::Source::Modwheel]   = mpeActive
            ? noteTimbre
            : s.modwheel.load(std::memory_order_relaxed);
        src[(size_t) mod::Source::Velocity]   = velocity;
        src[(size_t) mod::Source::Aftertouch] = mpeActive
            ? noteAftertouch
            : s.aftertouch.load(std::memory_order_relaxed);

        const float fineT = fineTuneSemis(s.get(params::P::ResFineTune));
        const float eff   = (float) curNote
                          + (float) s.resNoteOffset.load(std::memory_order_relaxed)
                          + fineT;
        src[(size_t) mod::Source::KeyFollow] =
            std::clamp((eff - 12.0f) / 96.0f, 0.0f, 1.0f);  // C0..C8

        matrix.evaluate(s, src);
        resolved.build(s, matrix);
    }

    void Voice::updateFromState (const state::PatchState& s) noexcept
    {
        resolve(s);
        exciter.updateFromState(resolved);
        resonator.updateFromState(resolved);
        output.update(resolved, uniPan);
        if (active) recomputePitch(resolved);
    }

    void Voice::noteOn (int midiNote, float vel, const state::PatchState& s) noexcept
    {
        curNote  = midiNote;
        velocity = vel;
        active   = true;
        gated    = true;
        // Reset per-voice MPE expression to neutral on each new note.
        // (mpeActive + midiChannel are set by the engine via setMpe() so
        // they're already in place by the time this runs.)
        noteBendSemis  = 0.0f;
        noteAftertouch = 0.0f;
        noteTimbre     = 0.0f;
        resolve(s);
        recomputePitch(resolved);
        // LOOP implies LEGATO (manual).
        const bool legato = s.envLegato.load(std::memory_order_relaxed)
                          || s.envLoop.load(std::memory_order_relaxed);
        exciter.noteOn(legato, s.kbReset.load(std::memory_order_relaxed));
        lfos.noteOn();
    }

    void Voice::noteOff (const state::PatchState& s) noexcept
    {
        gated = false;                 // no longer counts vs poly; may ring on
        exciter.noteOff(s.envHold.load(std::memory_order_relaxed));
    }

    void Voice::kill() noexcept
    {
        gated = false;
        // De-energise the loop over ~6 ms instead of hard-zeroing a loud
        // self-oscillating buffer (that hard cut was the steal click). The
        // voice frees via the normal lifetime check once it has decayed,
        // or is immediately re-excited if this was a steal.
        resonator.requestSoftReset();
    }
}
