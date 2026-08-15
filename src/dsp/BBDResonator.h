#pragma once
#include <vector>
#include <cstdint>
#include "dsp/OnePole.h"
#include "dsp/Saturator.h"
#include "state/ResolvedParams.h"

// Tuned positive-feedback BBD resonator.
//
// Physically-modelled core (proposal §1/§3.1): fractional-delay feedback
// loop, in-loop 6 dB/oct Feedback Filter, sign INVERT, self-limiting
// saturation, noise seed. Now also:
//   * 2x oversampling of the whole loop (saturator/aliasing headroom)
//   * 4-point Lagrange delay interpolation
//   * in-loop BBD HF-loss / reconstruction-droop filter (fixed per-pass
//     loss -> emergent darkening of long low resonances)
//   * group-delay pitch compensation (loop locks accurately to the key)
//   * deliberate per-voice tuning offset + slow drift ("alive/imperfect")
//
// Still later: true BBD aliasing path, companding, calibrated curves.
namespace tides::dsp
{
    class BBDResonator
    {
    public:
        static constexpr int kOS = 2;             // oversample factor

        void prepare (double sampleRate);
        void reset();
        // De-energise the loop over a few ms then clear (no click), used on
        // voice steal instead of hard-zeroing a loud self-oscillating buffer.
        void requestSoftReset() noexcept;

        // Pitch (and its filter-group-delay compensation) only change per
        // note/glide, so cache the compensation here instead of per sample.
        void setResonatorPitch (float hz) noexcept
        {
            targetHz = hz;
            cachedPhaseDelay = phaseDelaySamples(hz);
        }

        // Per-voice seed sets the direction of the static detune (a stable
        // [-1,1] random unique to this voice). Magnitude is then driven by
        // the Wild/Tame macro per block.
        void setVarianceSeed (uint32_t seed) noexcept;

        void updateFromState (const state::ResolvedParams& s) noexcept;

        float process (float exciterIn) noexcept;   // host-rate in/out

        // Smoothed |output| follower. The voice uses this to stay alive
        // while the loop is still ringing / self-oscillating, independent
        // of the amplitude envelope (manual: the loop sustains itself).
        float energy() const noexcept { return outEnv; }

    private:
        inline float innerLoop (float x) noexcept;  // one oversampled step
        float phaseDelaySamples (float f0) const noexcept; // filter comp

        double sampleRate { 48000.0 };
        double osRate { 96000.0 };

        std::vector<float> delayBuf;
        int   writePos { 0 }, mask { 0 };
        float targetHz { 110.0f };
        float curDelay { 256.0f };
        float glideCoeff { 1.0f };
        float cachedPhaseDelay { 0.0f };

        OnePole inputLP1, inputLP2;   // 12 dB/oct input filter (osr)
        OnePole feedbackLP;           // 6 dB/oct in-loop filter (osr)
        OnePole bbdLossLP;            // per-pass BBD HF loss (osr)
        float   fbCutHz { 8000.0f }, lossCutHz { 7000.0f };

        // 2nd-order Butterworth decimation (TDF2), runs at osr.
        struct Biquad
        {
            float b0{}, b1{}, b2{}, a1{}, a2{}, z1{}, z2{};
            void design (double fc, double sr);
            inline float process (float x) noexcept
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
        } decim;

        float feedbackGain { 0.0f };
        float dryWet { 0.0f };
        bool  invert { false };

        float prevExc { 0.0f };
        float outEnv  { 0.0f };
        float envRelCoeff { 0.9999f };   // ~300 ms follower release

        int   softLen { 1 }, softCtr { 0 };
        float curSoftGain { 1.0f };

        // Per-voice analogue variance + Wild/Tame character.
        float staticSeedNorm { 0.0f }; // [-1,1] fixed per voice (direction)
        float varStaticMul   { 1.0f }; // static detune (delay multiplier)
        float varDriftDepth  { 0.0f }; // cents
        float noiseSeedScale { 1.0e-5f };
        float satDrive       { 1.0f };
        float driftCur { 0.0f }, driftTarget { 0.0f };
        int   driftCounter { 0 }, driftPeriod { 1024 };

        uint32_t rng { 0x2545F491u };
        inline float white() noexcept
        {
            rng = rng * 1664525u + 1013904223u;
            return (float) ((int32_t) rng) * (1.0f / 2147483648.0f);
        }
    };
}
