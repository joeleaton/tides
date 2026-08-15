#pragma once
#include "dsp/Exciter.h"
#include "dsp/BBDResonator.h"
#include "dsp/OutputStage.h"
#include "dsp/QuadLFO.h"
#include "mod/ModMatrix.h"
#include "state/PatchState.h"
#include "state/ResolvedParams.h"

// One of the six hardware voices: Exciter -> BBD Resonator -> Output.
// Owns its per-voice modulators. Resonator pitch always key-tracks; the
// VCO tracks only when Key Follow is routed (INIT routes it -> keyFollowVco).
namespace tides::engine
{
    class Voice
    {
    public:
        void prepare (double sampleRate, int voiceIndex);

        // Per-voice unison offset (set before noteOn). detune in semitones
        // applied to exciter + resonator together; panOff added to base pan.
        void setUnison (float detuneSemis, float panOff) noexcept
        { uniDetune = detuneSemis; uniPan = panOff; }

        void noteOn  (int midiNote, float velocity, const state::PatchState&) noexcept;
        void noteOff (const state::PatchState&) noexcept;
        void kill() noexcept;

        bool isActive()  const noexcept { return active; }
        bool isHeld()    const noexcept { return active && gated; }   // counts vs poly
        bool isRinging() const noexcept { return active && ! gated; } // tail, reclaimable
        float resonatorEnergy() const noexcept { return resonator.energy(); }
        int  note()      const noexcept { return curNote; }
        uint64_t age()   const noexcept { return startStamp; }
        void setExternalInput (float x) noexcept { exciter.setExternalSample(x); }
        void setGroup (int g) noexcept { groupIndex = g; }
        int  group()     const noexcept { return groupIndex; }

        // MPE — per-voice expression. When `mpeActive` is set at noteOn the
        // voice ignores the group-wide modwheel / aftertouch and uses its own
        // stored values instead. `noteBendSemis` is added to the pitch on
        // every block.
        void setMpe (bool on, int channel) noexcept
        { mpeActive = on; midiChannel = channel; }
        bool isMpe() const noexcept { return mpeActive; }
        int  channel() const noexcept { return midiChannel; }
        void setNoteBendSemis  (float s)  noexcept { noteBendSemis  = s; }
        void setNoteAftertouch (float v)  noexcept { noteAftertouch = v; }
        void setNoteTimbre     (float v)  noexcept { noteTimbre     = v; }

        // Refresh cached parameters from the patch (per processing block).
        void updateFromState (const state::PatchState&) noexcept;

        inline void renderSample ([[maybe_unused]] const state::PatchState& s,
                                  float& l, float& r) noexcept
        {
            if (! active) { l = r = 0.0f; return; }

            lfos.tick();

            const float ex  = exciter.process();
            const float mono= resonator.process(ex);
            output.process(mono, l, r);

            // The voice lives until the exciter is done AND the resonator
            // has actually decayed - so plucks ring out and self-oscillation
            // sustains independent of the amplitude envelope.
            if (! exciter.isActive() && resonator.energy() < 1.0e-4f)
                active = false;
        }

        void setStartStamp (uint64_t s) noexcept { startStamp = s; }

    private:
        void recomputePitch (const state::ResolvedParams&) noexcept;
        // Push LFO params, gather the 9 sources, evaluate the matrix and
        // build the resolved (base + modulation) parameter view.
        void resolve (const state::PatchState&) noexcept;

        dsp::Exciter      exciter;
        dsp::BBDResonator resonator;
        dsp::OutputStage  output;
        dsp::QuadLFO      lfos;
        dsp::ModMatrix    matrix;
        state::ResolvedParams resolved;

        double sampleRate { 48000.0 };
        bool   active   { false };
        bool   gated    { false };   // true while the note is held
        int    curNote  { 60 };
        float  velocity { 1.0f };
        float  uniDetune { 0.0f };   // semitones
        float  uniPan    { 0.0f };   // added to base pan
        int    groupIndex { 0 };
        uint64_t startStamp { 0 };

        // MPE per-voice state — captured at noteOn, updated by the engine
        // when MIDI arrives on this voice's member channel.
        bool   mpeActive      { false };
        int    midiChannel    { 0 };
        float  noteBendSemis  { 0.0f };
        float  noteAftertouch { 0.0f };
        float  noteTimbre     { 0.0f };

    public:
        // Snapshot of the 9 mod-source values from the most recent resolve().
        // Read by the engine to publish into EngineSnapshot::liveSources so
        // the UI can animate mod ribbons. Order matches mod::Source.
        const std::array<float, 9>& lastSources() const noexcept { return srcCache; }
    private:
        std::array<float, 9> srcCache {};
    };
}
