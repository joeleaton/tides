#pragma once
#include <cmath>

// Smooth bounded nonlinearity. In the resonator it serves double duty:
// self-limits the feedback loop (safe at extreme settings) and provides the
// "BBD pushed into distortion above 3 o'clock" colour (proposal §3.1).
// The exact curve will be fitted to hardware audio later.
namespace tides::dsp
{
    struct Saturator
    {
        // Padé tanh approximation: smooth, monotonic, saturates ~±1, and
        // ~an order of magnitude cheaper than std::tanh (called twice per
        // sample per voice in the resonator loop). The exact curve is a
        // calibration target for reference audio anyway.
        static inline float process (float x, float drive = 1.0f) noexcept
        {
            x *= drive;
            if (x < -3.0f) return -1.0f;
            if (x >  3.0f) return  1.0f;
            const float x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        }
    };
}
