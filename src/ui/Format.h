#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "dsp/ParamMap.h"

// Formatters that turn the engine's 0..1 normalised values (or -1..+1 for
// bipolar attenuverters) into the typographically-tight readouts the design
// uses. Mirrors design_handoff_tides/tides-state.jsx::Fmt.
namespace tides::ui::fmt
{
    inline juce::String pct (float v)
    {
        return juce::String (juce::roundToInt (v * 100.0f)) + "%";
    }

    inline juce::String pan (float v)
    {
        if (juce::approximatelyEqual (v, 0.5f)) return "C";
        if (v < 0.5f)
            return "L" + juce::String (juce::roundToInt ((0.5f - v) * 200.0f));
        return "R" + juce::String (juce::roundToInt ((v - 0.5f) * 200.0f));
    }

    inline juce::String semi (float v, float range = 3.0f)
    {
        const float s = (v - 0.5f) * 2.0f * range;
        const auto sign = s >= 0.0f ? "+" : "";
        return juce::String (sign) + juce::String (s, 2) + juce::String::fromUTF8 (" st");
    }

    inline juce::String oct (float v)
    {
        const float s = (v - 0.5f) * 8.0f;
        const auto sign = s >= 0.0f ? "+" : "";
        return juce::String (sign) + juce::String (s, 1) + " oct";
    }

    inline juce::String hz (float v, float lo = 20.0f, float hi = 18000.0f)
    {
        const float f = lo * std::pow (hi / lo, v);
        if (f >= 1000.0f)
            return juce::String (f / 1000.0f, 2) + " kHz";
        return juce::String ((int) std::round (f)) + " Hz";
    }

    inline juce::String ms (float v, float max_ms = 2000.0f)
    {
        const float m = v * max_ms;
        if (m < 100.0f)  return juce::String (m, 1) + " ms";
        if (m < 1000.0f) return juce::String ((int) std::round (m)) + " ms";
        return juce::String (m / 1000.0f, 2) + " s";
    }

    inline juce::String lfoRateHz (float v)
    {
        // Defer to the single canonical curve in dsp::map.
        const float h = tides::dsp::map::lfoRateHz (v);
        if (h < 0.1f)  return juce::String (h, 3) + " Hz";   // 0.005 Hz precision at the bottom
        if (h < 10.0f) return juce::String (h, 2) + " Hz";
        return juce::String ((int) std::round (h)) + " Hz";
    }

    inline juce::String feedback (float v)
    {
        if (v >= 0.99f) return juce::String::fromUTF8 ("∞ self-osc");
        return pct (v);
    }

    inline juce::String wildTame (float v)
    {
        // DSP convention: w = v directly (unipolar). 0 = fully clean / tame,
        // 1 = max drift+noise+saturation+detune = wild. Display matches:
        // CCW reads as TAME, CW reads as WILD, with the integer percent
        // tracking the actual wild scaling along the way.
        if (v <= 0.005f) return "TAME";
        if (v >= 0.995f) return "WILD";
        return juce::String ("WILD ") + juce::String (juce::roundToInt (v * 100.0f)) + "%";
    }

    inline juce::String waveNoiseMix (float v)
    {
        if (v < 0.5f)
            return juce::String ("W ") + juce::String (juce::roundToInt ((0.5f - v) * 200.0f)) + "%";
        if (juce::approximatelyEqual (v, 0.5f)) return juce::String::fromUTF8 ("·");
        return juce::String ("N ") + juce::String (juce::roundToInt ((v - 0.5f) * 200.0f)) + "%";
    }

    inline juce::String waveMorph (float v)
    {
        if (v < 0.20f) return "SIN";
        if (v < 0.45f) return juce::String::fromUTF8 ("SIN→TRI");
        if (v < 0.55f) return "TRI";
        if (v < 0.70f) return juce::String::fromUTF8 ("TRI→RMP");
        if (v < 0.85f) return juce::String::fromUTF8 ("RMP→PUL");
        return "SQR";
    }

    inline juce::String intSigned (int v)
    {
        return (v > 0 ? juce::String ("+") : juce::String()) + juce::String (v);
    }

    // Bipolar value for source-macro readout: "·" at zero, "+N" / "-N" otherwise.
    inline juce::String macroValue (float v)
    {
        if (std::abs (v) < 0.005f) return juce::String::fromUTF8 ("·");
        const auto sign = v > 0.0f ? "+" : "";
        return juce::String (sign) + juce::String (juce::roundToInt (v * 100.0f));
    }
}
