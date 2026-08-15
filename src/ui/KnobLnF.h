#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"

namespace tides::ui
{
    // Custom rotary look matching design_handoff_tides:
    //   240° sweep from -210° to +30° (clockwise = increase),
    //   inset face with edge stroke + gradient highlight,
    //   2 px ring track + fill (unipolar) or symmetric pos/neg split (bipolar),
    //   bipolar detent tick at 12 o'clock,
    //   saved-value tick just outside the ring,
    //   mod-ribbon arcs stacked further out (read from slider properties).
    //
    // The slider's juce::Slider::getProperties() carries the supplementary
    // state the L&F needs:
    //   "bipolar"  (bool)        — true for attenuverters / centred params
    //   "saved"    (double)      — saved value in slider's own range (omitted = no tick)
    //   "modCount" (int)         — number of mod ribbons to draw (placeholder)
    //   "selecting"(bool)        — patch-mode purple face stroke
    class KnobLnF : public juce::LookAndFeel_V4
    {
    public:
        KnobLnF()
        {
            // Defaults that affect any plain juce::Slider that uses this L&F.
            setColour (juce::Slider::rotarySliderFillColourId,    col::Amber);
            setColour (juce::Slider::rotarySliderOutlineColourId, col::RingBase);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                               float sliderPosNorm,
                               float /*rotaryStart*/, float /*rotaryEnd*/,
                               juce::Slider& slider) override
        {
            const float startDeg = -210.0f;
            const float endDeg   =   30.0f;
            const float spanDeg  = endDeg - startDeg;     // 240°

            const auto& props = slider.getProperties();
            const bool bipolar   = (bool) props.getWithDefault ("bipolar",  false);
            const bool selecting = (bool) props.getWithDefault ("selecting", false);
            const bool hasSaved  = props.contains ("saved");
            const double savedV  = (double) props.getWithDefault ("saved", 0.0);

            // Geometry — matches design's 100×100 viewBox at r=30 (ring),
            // r=24 (face). Ratios 0.30 / 0.24 against the cell diameter.
            // The cell is intentionally a fair bit larger than the visible
            // disc so ticks and modulation ribbons stack outside the ring
            // without ever leaving the slider's bounds.
            const float diameter = (float) juce::jmin (w, h);
            const float cx = (float) x + (float) w * 0.5f;
            const float cy = (float) y + (float) h * 0.5f;
            const float ringR  = diameter * 0.30f;
            const float ringT  = 2.0f;
            const float faceR  = diameter * 0.24f;
            const float ptrIn  = diameter * 0.10f;
            const float ptrOut = diameter * 0.26f;

            const float minV = (float) slider.getMinimum();
            const float maxV = (float) slider.getMaximum();
            const float v    = (float) slider.getValue();
            const float vNorm = (v - minV) / juce::jmax (1.0e-6f, (maxV - minV));   // 0..1
            (void) sliderPosNorm;   // we compute from value directly

            auto deg = [&] (float n01) { return startDeg + n01 * spanDeg; };

            auto pathArc = [&] (float aStart, float aEnd) -> juce::Path
            {
                juce::Path p;
                p.addCentredArc (cx, cy, ringR, ringR, 0.0f,
                                 juce::degreesToRadians (aStart + 90.0f),
                                 juce::degreesToRadians (aEnd   + 90.0f),
                                 true);
                return p;
            };

            // --- track ring -------------------------------------------------
            g.setColour (col::RingBase);
            g.strokePath (pathArc (startDeg, endDeg), juce::PathStrokeType (ringT));

            // --- ring fill --------------------------------------------------
            if (bipolar)
            {
                const float vBi = (v - (minV + maxV) * 0.5f) / juce::jmax (1.0e-6f, (maxV - minV) * 0.5f); // -1..+1
                const float centreDeg = startDeg + spanDeg * 0.5f;
                if (vBi >= 0.0f)
                    g.setColour (col::Pos);
                else
                    g.setColour (col::Neg);
                const float fillEnd = centreDeg + vBi * (spanDeg * 0.5f);
                if (! juce::approximatelyEqual (vBi, 0.0f))
                {
                    juce::PathStrokeType st (ringT, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded);
                    g.strokePath (pathArc (juce::jmin (centreDeg, fillEnd),
                                           juce::jmax (centreDeg, fillEnd)), st);
                }
            }
            else
            {
                g.setColour (col::Amber);
                juce::PathStrokeType st (ringT, juce::PathStrokeType::curved,
                                         juce::PathStrokeType::rounded);
                g.strokePath (pathArc (startDeg, deg (vNorm)), st);
            }

            // --- saved-value tick (just outside the ring) -------------------
            if (hasSaved)
            {
                const float sNorm = bipolar
                    ? ((float) savedV - (minV + maxV) * 0.5f) / juce::jmax (1.0e-6f, (maxV - minV) * 0.5f) * 0.5f + 0.5f
                    : ((float) savedV - minV) / juce::jmax (1.0e-6f, (maxV - minV));
                if (std::abs ((float) savedV - v) > 0.001f)
                {
                    const float a = juce::degreesToRadians (deg (sNorm) + 90.0f);
                    const float r1 = ringR + 2.0f;
                    const float r2 = ringR + diameter * 0.09f;
                    g.setColour (col::Amber.withAlpha (0.95f));
                    juce::Line<float> l { cx + r1 * std::sin (a), cy - r1 * std::cos (a),
                                          cx + r2 * std::sin (a), cy - r2 * std::cos (a) };
                    g.drawLine (l, 2.0f);
                }
            }

            // --- face -------------------------------------------------------
            // Outline + dark face fill
            g.setColour (col::Knob);
            g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);

            // Radial highlight (gradient stop top-left to bottom-right)
            {
                juce::ColourGradient grad { col::KnobHi,
                                            cx - faceR * 0.4f, cy - faceR * 0.6f,
                                            col::Knob.darker (0.6f),
                                            cx + faceR * 0.6f, cy + faceR * 0.7f,
                                            true };
                g.setGradientFill (grad);
                g.fillEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f);
            }

            // Face stroke (purple in patch-mode select)
            g.setColour (selecting ? col::Purple.withAlpha (0.8f) : col::KnobEdge);
            g.drawEllipse (cx - faceR, cy - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

            // --- bipolar detent (small bright tick at 12 o'clock) -----------
            if (bipolar)
            {
                const float a = juce::degreesToRadians (startDeg + spanDeg * 0.5f + 90.0f);
                const float r1 = ringR - diameter * 0.06f;
                const float r2 = ringR + diameter * 0.05f;
                g.setColour (col::Ink);
                juce::Line<float> l { cx + r1 * std::sin (a), cy - r1 * std::cos (a),
                                      cx + r2 * std::sin (a), cy - r2 * std::cos (a) };
                g.drawLine (l, 2.0f);
            }

            // --- pointer ----------------------------------------------------
            const float ptrA = juce::degreesToRadians (deg (vNorm) + 90.0f);
            const float p1x = cx + ptrIn  * std::sin (ptrA);
            const float p1y = cy - ptrIn  * std::cos (ptrA);
            const float p2x = cx + ptrOut * std::sin (ptrA);
            const float p2y = cy - ptrOut * std::cos (ptrA);
            g.setColour (col::AmberHot);
            g.drawLine ({ p1x, p1y, p2x, p2y }, 2.0f);

            // --- modulation ribbons (always-visible per-route range arcs) ---
            // Each route published by Knob::updateModRibbons() draws as an
            // arc just outside the main ring, polarity-coloured. The arc
            // spans from (current value − |eff depth|) to (current value +
            // |eff depth|), clamped to [0,1], to show how far that source
            // can swing the destination at full source amplitude. Multiple
            // routes stack outward at increasing radii.
            const int modCount = (int) props.getWithDefault ("modCount", 0);
            for (int m = 0; m < modCount; ++m)
            {
                const float effDepth = (float) (double) props.getWithDefault (
                    "mod_" + juce::String (m) + "_depth", 0.0);
                if (std::abs (effDepth) < 0.001f) continue;

                const float lo = juce::jlimit (0.0f, 1.0f, vNorm - std::abs (effDepth));
                const float hi = juce::jlimit (0.0f, 1.0f, vNorm + std::abs (effDepth));
                if (hi - lo < 0.001f) continue;
                const float aStart = deg (lo);
                const float aEnd   = deg (hi);

                const float r = ringR + diameter * 0.13f + (float) m * 3.5f;
                juce::Path arc;
                arc.addCentredArc (cx, cy, r, r, 0.0f,
                                   juce::degreesToRadians (aStart + 90.0f),
                                   juce::degreesToRadians (aEnd   + 90.0f),
                                   true);

                const juce::Colour ribbonCol = effDepth >= 0.0f
                    ? col::Pos.withAlpha (0.55f)   // softer so the live dot pops
                    : col::Neg.withAlpha (0.55f);
                g.setColour (ribbonCol);
                g.strokePath (arc, juce::PathStrokeType (2.0f,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded));

                // Animated dot — current position of the source on this
                // ribbon. source × effDepth shows where the parameter is
                // *right now*, riding along the arc as the LFO sweeps.
                const float liveSrc = (float) (double) props.getWithDefault (
                    "mod_" + juce::String (m) + "_live", 0.0);
                const float pos = juce::jlimit (0.0f, 1.0f, vNorm + liveSrc * effDepth);
                const float aDot = juce::degreesToRadians (deg (pos) + 90.0f);
                const float dotX = cx + r * std::sin (aDot);
                const float dotY = cy - r * std::cos (aDot);
                const juce::Colour dotCol = effDepth >= 0.0f ? col::Pos : col::Neg;
                g.setColour (dotCol.withAlpha (0.30f));
                g.fillEllipse (dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f);   // glow
                g.setColour (dotCol);
                g.fillEllipse (dotX - 2.0f, dotY - 2.0f, 4.0f, 4.0f);   // core
            }
        }
    };
}
