#pragma once
#include "dsp/Wavetable.h"

// Exciter VCO: band-limited continuous morph through
// sine -> triangle -> ramp -> 5% pulse -> square (manual WAVE control).
// When Ext-In is enabled the sine slot is replaced by the external signal
// and the morph blends it with the internal shapes (manual EXT IN).
namespace tides::dsp
{
    class MorphOscillator
    {
    public:
        void prepare (double sr)
        {
            sampleRate = sr;
            bank = &WavetableSet::shared(sr);
            reset();
        }
        void reset (bool resetPhase = true) { if (resetPhase) phase = 0.0f; }

        void setFrequency (float hz) noexcept
        {
            inc = hz / (float) sampleRate;
            mip = bank->mipForFreq(hz);
        }
        void setMorph (float w) noexcept { morph = w; }

        void setExternalEnabled (bool e) noexcept { extEnabled = e; } // per block
        void setExternalSample  (float x) noexcept { extIn = x; }     // per sample

        inline float process() noexcept
        {
            const float seg = morph * 4.0f;          // 4 crossfade segments
            int   a = (int) seg;
            if (a > 3) a = 3;
            const int   b = a + 1;
            const float f = seg - (float) a;

            const float out = shapeSample(a) * (1.0f - f) + shapeSample(b) * f;

            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;
            return out;
        }

    private:
        inline float shapeSample (int shape) const noexcept
        {
            if (shape == 0 && extEnabled) return extIn; // ext-in replaces sine
            return bank->sample(shape, mip, phase);
        }

        const WavetableSet* bank { nullptr };
        double sampleRate { 48000.0 };
        float  phase { 0.0f }, inc { 0.0f }, morph { 0.0f };
        int    mip { 0 };
        float  extIn { 0.0f };
        bool   extEnabled { false };
    };
}
