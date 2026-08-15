#pragma once
#include <cstdint>

// Analogue-style transistor noise -> sample & hold. Final design: rate map
// tracks Coarse Freq over ~75% of range then steepens to continuous white
// (proposal §3.2). SKELETON: white + basic S&H; unused by INIT (mix = 0).
namespace tides::dsp
{
    class NoiseSH
    {
    public:
        void prepare (double sr) { sampleRate = sr; }
        void setRateHz (float hz) noexcept { stepInc = hz / (float) sampleRate; }

        inline float process() noexcept
        {
            phase += stepInc;
            if (phase >= 1.0f) { phase -= 1.0f; held = white(); }
            return held;
        }

    private:
        inline float white() noexcept
        {
            state = state * 1664525u + 1013904223u;
            return (float) ((int32_t) state) * (1.0f / 2147483648.0f);
        }
        double   sampleRate { 48000.0 };
        uint32_t state { 0x1234567u };
        float    phase { 0.0f }, stepInc { 1.0f }, held { 0.0f };
    };
}
