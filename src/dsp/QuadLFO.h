#pragma once
#include <cmath>
#include <cstdint>

// Four per-voice LFOs. Each = periodic shape (sine/tri/ramp/square) blended
// with a stepped S&H random generator (own rate, optional smoothing),
// scaled by amplitude, bipolar or unipolar. KB-reset restarts the phase on
// note-on. (MIDI-sync / single-shot / cross-mod deferred.)
namespace tides::dsp
{
    class QuadLFO
    {
    public:
        static constexpr int kNum = 4;

        void prepare (double sr) { sampleRate = sr; reset(); }

        void reset()
        {
            for (auto& l : lfo) { l = Lfo{}; l.rng = 0x9E3779B9u + (uint32_t) (&l - lfo); }
        }

        void setParams (int i, float rateHz, float randomHz, float amp,
                        float mix, int wave, bool bipolar,
                        float smoothAmount, bool kbReset) noexcept
        {
            auto& l = lfo[i];
            l.inc     = rateHz / (float) sampleRate;
            l.rndInc  = randomHz <= 0.001f ? 0.0f : randomHz / (float) sampleRate;
            l.amp     = amp;
            l.mix     = mix;
            l.wave    = wave;
            l.bipolar = bipolar;
            // Continuous 0..1 -> smoothing coefficient. 0 = instant step,
            // 1 = very slow slew (~200 ms). Exponential mapping makes the
            // mid-knob range musically useful.
            l.smoothAmount = smoothAmount;
            l.smoothCoef   = std::pow (0.005f, smoothAmount);
            l.kbReset = kbReset;
        }

        void noteOn() noexcept
        {
            for (auto& l : lfo)
                if (l.kbReset)
                {
                    l.phase = 0.0f;
                    l.heldTarget = white(l);
                    if (l.smoothCoef >= 0.99f) l.heldCur = l.heldTarget;
                }
        }

        inline void tick() noexcept
        {
            for (auto& l : lfo)
            {
                float pv;
                const float ph = l.phase;
                switch (l.wave)
                {
                    case 1:  pv = 1.0f - 4.0f * std::abs(ph - 0.5f); break;   // tri
                    case 2:  pv = 2.0f * ph - 1.0f;                  break;   // ramp
                    case 3:  pv = ph < 0.5f ? 1.0f : -1.0f;          break;   // sqr
                    default: pv = std::sin(6.2831853f * ph);         break;   // sin
                }
                l.phase += l.inc;
                if (l.phase >= 1.0f) l.phase -= 1.0f;

                if (l.rndInc > 0.0f)
                {
                    l.rndPhase += l.rndInc;
                    if (l.rndPhase >= 1.0f) { l.rndPhase -= 1.0f; l.heldTarget = white(l); }
                }
                // smoothCoef 0..1: 1 = instant, near-0 = slow slew.
                l.heldCur += (l.heldTarget - l.heldCur) * l.smoothCoef;

                const float mixed  = pv * l.mix + l.heldCur * (1.0f - l.mix);
                const float scaled = mixed * l.amp;
                l.out = l.bipolar ? scaled : (scaled * 0.5f + 0.5f);
            }
        }

        float value (int i) const noexcept { return lfo[i].out; }

    private:
        struct Lfo
        {
            float phase{0}, inc{0}, rndPhase{0}, rndInc{0};
            float heldCur{0}, heldTarget{0}, amp{1}, mix{1}, out{0};
            int   wave{0};
            bool  bipolar{true}, kbReset{false};
            float smoothAmount{0.0f}, smoothCoef{1.0f};   // 1 = instant
            uint32_t rng{0x12345u};
        };

        static inline float white (Lfo& l) noexcept
        {
            l.rng = l.rng * 1664525u + 1013904223u;
            return (float) ((int32_t) l.rng) * (1.0f / 2147483648.0f);
        }

        double sampleRate { 48000.0 };
        Lfo    lfo[kNum];
    };
}
