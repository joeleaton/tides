#pragma once
#include <cmath>
#include <algorithm>

// Per-voice DADSR. Doubles as a modulation source (same contour the VCA
// sees), per manual. Onset-delay + log attack + exp decay/release here;
// HOLD / LEGATO / LOOP and the modulation tap are wired in later stages.
namespace tides::dsp
{
    class EnvelopeDADSR
    {
    public:
        enum class Stage { Idle, Delay, Attack, Decay, Sustain, Release };

        void prepare (double sr) { sampleRate = sr; }

        // Times in seconds (caller applies the short/med/long range curve).
        void setTimes (float delayS, float attackS, float decayS,
                       float sustainLvl, float releaseS) noexcept
        {
            delaySamps = secs(delayS);
            sustain    = std::clamp(sustainLvl, 0.0f, 1.0f);
            // One-pole (RC-style) segment coefficients: ~5 time constants
            // over the nominal stage time. Continuous at every transition,
            // so no envelope discontinuity for the resonator to click on.
            attackCoef  = coef(attackS);
            decayCoef   = coef(decayS);
            releaseCoef = coef(releaseS);
        }

        // LOOP: while held, cycle Delay->Attack->Decay instead of holding
        // Sustain (rhythmic/tremolo/evolving shapes). Implies legato.
        void setLoop (bool shouldLoop) noexcept { loopMode = shouldLoop; }

        void noteOn (bool legato) noexcept
        {
            if (! legato || stage == Stage::Idle) level = 0.0f;
            counter = 0;
            stage   = (delaySamps > 0 ? Stage::Delay : Stage::Attack);
        }
        void noteOff (bool hold) noexcept
        {
            if (hold) return;            // HOLD: ignore note-off
            stage = Stage::Release;      // continues from current level
        }

        bool  isActive() const noexcept { return stage != Stage::Idle; }
        float value()    const noexcept { return level; }

        inline float process() noexcept
        {
            // Aim slightly past the targets so the asymptotic one-pole
            // actually reaches them in finite, musical time (analog-like).
            constexpr float kOver = 1.02f;

            switch (stage)
            {
                case Stage::Idle:    level = 0.0f; break;
                case Stage::Delay:
                    if (++counter >= delaySamps) { counter = 0; stage = Stage::Attack; }
                    break;
                case Stage::Attack:
                    level += (kOver - level) * attackCoef;
                    if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                    break;
                case Stage::Decay:
                    level += (sustain - level) * decayCoef;
                    if (level - sustain < 1.0e-4f)
                    {
                        if (loopMode) { counter = 0; stage = Stage::Delay; }
                        else            stage = Stage::Sustain;
                    }
                    break;
                case Stage::Sustain: level = sustain; break;
                case Stage::Release:
                    level += (0.0f - level) * releaseCoef;
                    if (level < 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                    break;
            }
            return level;
        }

    private:
        int secs (float s) const noexcept
        { return (int) std::lround(s * sampleRate); }

        // ~5 time constants across the nominal stage time => coef in (0,1].
        float coef (float seconds) const noexcept
        {
            const float n = std::max(1.0f, (float) (seconds * sampleRate));
            return 1.0f - std::exp(-5.0f / n);
        }

        double sampleRate { 48000.0 };
        int    delaySamps { 0 }, counter { 0 };
        float  attackCoef { 0.5f }, decayCoef { 0.01f }, releaseCoef { 0.01f };
        float  sustain { 1.0f }, level { 0.0f };
        bool   loopMode { false };
        Stage  stage { Stage::Idle };
    };
}
