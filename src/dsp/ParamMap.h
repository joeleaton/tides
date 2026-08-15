#pragma once
#include <cmath>
#include <algorithm>

// Normalised (0..1) -> engineering-unit mappings. Ranges taken from the
// ECHON 6 manual. Kept centralised so the DSP and a future UI agree on curves.

namespace tides::dsp::map
{
    inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
    inline float clamp01 (float x) { return std::clamp(x, 0.0f, 1.0f); }

    // Logarithmic frequency map.
    inline float logHz (float n, float lo, float hi)
    {
        return lo * std::pow(hi / lo, clamp01(n));
    }

    // Input filter: 32.7 Hz .. 21 kHz (manual).
    inline float inputFilterHz (float n)    { return logHz(n, 32.7f, 21000.0f); }
    // Feedback filter: gentle in-loop LPF (range provisional pending audio ref).
    inline float feedbackFilterHz (float n) { return logHz(n, 40.0f, 18000.0f); }

    // DADSR stage time. Manual: short ~0..250 ms, long ~0..25 s; medium between.
    // Exponential so small knob moves are usable at the short end.
    inline float stageSeconds (float n, int range /*0 short,1 med,2 long*/)
    {
        const float maxS = (range == 0 ? 0.25f : range == 1 ? 2.5f : 25.0f);
        const float minS = 0.0005f;
        return minS * std::pow(maxS / minS, clamp01(n));
    }

    inline float onsetDelaySeconds (float n) { return clamp01(n) * 2.0f; }   // 0..2 s
    inline float glideSeconds      (float n) { return clamp01(n) * 1.0f; }   // 0..1 s
    inline float fineTuneSemis     (float n) { return lerp(-3.0f, 3.0f, n); }// ±3 st

    // Exciter pitch: COARSE ~ ±4 octaves, FINE ±3 st (summed onto note).
    inline float coarseOctaves (float n) { return lerp(-4.0f, 4.0f, n); }
    inline float fineSemis     (float n) { return lerp(-3.0f, 3.0f, n); }

    // Noise S&H clock. Manual: stepped/segmented at low settings, similar
    // range to Coarse over most of the travel, steepening to continuous
    // white in the final quarter (rate >> Fs => new value every sample).
    inline float noiseShRateHz (float n)
    {
        n = clamp01(n);
        if (n < 0.75f)
            return 2.0f * std::pow(3000.0f / 2.0f, n / 0.75f);       // ~2 Hz..3 kHz
        return 3000.0f * std::pow(60000.0f / 3000.0f, (n - 0.75f) / 0.25f); // ->white
    }

    // LFO periodic rate. Piecewise log so the lower half of the knob
    // covers slow drifts (0.005 Hz = ≈200 s/cycle .. 1 Hz) while the
    // upper half covers tremolo/vibrato territory (1 Hz .. 50 Hz). A
    // single logHz over the full 4-decade range cramps the useful slow
    // region into the first ~10% of the knob, which is why the bottom
    // end felt unreachable.
    inline float lfoRateHz (float n)
    {
        n = clamp01(n);
        return n < 0.5f ? logHz (n * 2.0f,         0.005f, 1.0f)
                        : logHz ((n - 0.5f) * 2.0f, 1.0f, 50.0f);
    }
    // S&H random clock: fully CCW stops it (held value).
    inline float lfoRandomHz (float n)
    {
        return clamp01(n) < 0.002f ? 0.0f : logHz(n, 0.1f, 60.0f);
    }

    inline float midiNoteHz (float midiNote)
    {
        return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
    }
}
