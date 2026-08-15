#pragma once
#include "dsp/MorphOscillator.h"
#include "dsp/NoiseSH.h"
#include "dsp/EnvelopeDADSR.h"
#include "state/ResolvedParams.h"

// Exciter = sound generation (VCO morph + noise/S&H, blended) shaped by the
// DADSR VCA and Level. Output is the raw mono excitation that feeds the
// resonator's Input Filter (the Input Filter itself lives in BBDResonator,
// matching the manual's signal flow).
namespace tides::dsp
{
    class Exciter
    {
    public:
        void prepare (double sr);

        void setVcoFrequency (float hz) noexcept { vco.setFrequency(hz); }

        void setExternalSample (float x) noexcept { vco.setExternalSample(x); }

        void noteOn  (bool legato, bool kbReset) noexcept;
        void noteOff (bool hold)   noexcept { env.noteOff(hold); }
        bool isActive() const noexcept { return env.isActive(); }

        // Pull current values from the patch (called per block).
        void updateFromState (const state::ResolvedParams& s) noexcept;

        float envValue() const noexcept { return env.value(); }

        inline float process() noexcept
        {
            const float w = vco.process();
            const float n = noise.process();
            const float mixed = w * (1.0f - waveNoiseMix) + n * waveNoiseMix;
            return mixed * env.process() * level;
        }

    private:
        MorphOscillator vco;
        NoiseSH         noise;
        EnvelopeDADSR   env;
        float waveNoiseMix { 0.0f };
        float level { 0.7f };
    };
}
