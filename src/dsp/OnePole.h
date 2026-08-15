#pragma once
#include <cmath>

// TPT one-pole low-pass (zero-delay-feedback). Stable under per-sample
// cutoff modulation, which the resonator's in-loop filter requires.
namespace tides::dsp
{
    class OnePole
    {
    public:
        void prepare (double sr) { sampleRate = sr; reset(); }
        void reset() { z = 0.0f; }

        void setCutoff (float hz) noexcept
        {
            hz = std::fmin(hz, (float) sampleRate * 0.49f);
            const float g = std::tan(3.14159265f * hz / (float) sampleRate);
            G = g / (1.0f + g);
        }

        inline float processLP (float x) noexcept
        {
            const float v = (x - z) * G;
            const float y = v + z;
            z = y + v;
            return y;
        }

    private:
        double sampleRate { 48000.0 };
        float  G { 0.5f };
        float  z { 0.0f };
    };
}
