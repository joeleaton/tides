#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

// Mip-mapped band-limited wavetables for the Exciter VCO morph path
// (sine -> triangle -> ramp -> 5% pulse -> square). One additive,
// harmonic-limited table per shape per octave band; the oscillator picks
// the band by fundamental frequency and crossfades adjacent shapes.
//
// Built once per sample rate (off the audio thread, in prepare).
namespace tides::dsp
{
    class WavetableSet
    {
    public:
        static constexpr int kNumShapes = 5;   // sine,tri,ramp,pulse5,square
        static constexpr int kSize      = 2048;
        static constexpr int kNumMips   = 11;   // ~16 Hz .. ~32 kHz fundamentals
        static constexpr float kBaseHz  = 16.3516f; // C0

        // Shared instance, rebuilt only when the sample rate changes.
        static const WavetableSet& shared (double sampleRate)
        {
            static WavetableSet inst;
            if (std::abs(inst.builtRate - sampleRate) > 1.0e-6) inst.build(sampleRate);
            return inst;
        }

        int mipForFreq (float hz) const noexcept
        {
            const int m = (int) std::floor(std::log2(std::max(hz, 1.0f) / kBaseHz));
            return std::clamp(m, 0, kNumMips - 1);
        }

        // Linear-interpolated table lookup. phase in [0,1).
        inline float sample (int shape, int mip, float phase) const noexcept
        {
            const float fp = phase * (float) kSize;
            const int   i0 = (int) fp;
            const float fr = fp - (float) i0;
            const auto& t  = tables[(size_t) shape][(size_t) mip];
            return t[(size_t) i0] + fr * (t[(size_t) i0 + 1] - t[(size_t) i0]);
        }

    private:
        double builtRate { 0.0 };
        std::array<std::array<std::vector<float>, kNumMips>, kNumShapes> tables;

        void build (double sr)
        {
            builtRate = sr;
            const float nyq = 0.45f * (float) sr;

            for (int s = 0; s < kNumShapes; ++s)
                for (int m = 0; m < kNumMips; ++m)
                {
                    const float topFundamental = kBaseHz * std::pow(2.0f, (float) (m + 1));
                    int maxH = (int) std::floor(nyq / topFundamental);
                    maxH = std::clamp(maxH, 1, 1024);
                    tables[(size_t) s][(size_t) m] = buildShape(s, maxH);
                }
        }

        // Additive build via per-harmonic phasor recurrence (cheap, no per
        // sample std::sin). Each table normalised to unit peak.
        static std::vector<float> buildShape (int shape, int maxH)
        {
            std::vector<float> t((size_t) kSize + 1, 0.0f);
            const float dPhi = 6.28318530718f / (float) kSize;

            for (int h = 1; h <= maxH; ++h)
            {
                float amp = 0.0f;
                switch (shape)
                {
                    case 0: amp = (h == 1) ? 1.0f : 0.0f; break;          // sine
                    case 1: // triangle: odd, 1/h^2, alternating
                        if (h & 1) amp = ((((h - 1) / 2) & 1) ? -1.0f : 1.0f)
                                          / (float) (h * h);
                        break;
                    case 2: amp = 1.0f / (float) h; break;                // ramp/saw
                    case 3: // 5% pulse
                        amp = 2.0f * std::sin(3.14159265f * (float) h * 0.05f)
                              / (3.14159265f * (float) h);
                        break;
                    case 4: amp = (h & 1) ? 1.0f / (float) h : 0.0f; break;// square
                    default: break;
                }
                if (std::abs(amp) < 1.0e-12f) continue;

                const float w = dPhi * (float) h;
                const float c = std::cos(w), sgen = std::sin(w);
                float cs = 1.0f, sn = 0.0f;          // cos/sin recurrence
                for (int i = 0; i <= kSize; ++i)
                {
                    t[(size_t) i] += amp * sn;       // sine-series basis
                    const float ncs = cs * c - sn * sgen;
                    sn = sn * c + cs * sgen;
                    cs = ncs;
                }
            }

            float peak = 1.0e-9f;
            for (int i = 0; i < kSize; ++i) peak = std::max(peak, std::abs(t[(size_t) i]));
            const float g = 1.0f / peak;
            for (auto& v : t) v *= g;
            t[(size_t) kSize] = t[0];                // wrap guard point
            return t;
        }
    };
}
