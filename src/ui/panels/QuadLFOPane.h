#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "ui/Theme.h"
#include "ui/Pane.h"
#include "ui/Primitives.h"
#include "ui/AtomicKnob.h"
#include "ui/Format.h"
#include "dsp/ParamMap.h"
#include "PluginProcessor.h"

namespace tides::ui::panels
{
    // 06 QUAD LFO pane.
    // Top strip:  inset PanelLo, animated waveform + 4 LFO tabs overlaid
    //             top-left + "WAVE · rate Hz" readout top-right
    // Knob row:   RATE · RANDOM · AMP · W↔RND · SMOOTH  (all small, atomic)
    // Bottom row: wave Seg (∿/△/⩘/⊓) · Bipolar · KB Rst · ◇ 1-SHOT stub
    class QuadLFOPane : public Pane, private juce::Timer
    {
    public:
        explicit QuadLFOPane (TidesAudioProcessor& p)
            : Pane ("06", "QUAD LFO"),
              proc (p),
              preview (p, activeLFO),
              waveSeg ({ { juce::String::fromUTF8 ("∿"), 0 },
                         { juce::String::fromUTF8 ("△"), 1 },
                         { juce::String::fromUTF8 ("⩘"), 2 },
                         { juce::String::fromUTF8 ("⊓"), 3 } }),
              swBipolar ("Bipolar"),
              swKbRst   ("KB Rst"),
              knobRate   (p, &proc.patch().lfo[0].rate,       AtomicKnobSize::S, "RATE",   0.0f, 1.0f, false, fmt::lfoRateHz, false, mod::Dest::Lfo1Rate),
              knobRandom (p, &proc.patch().lfo[0].randomRate, AtomicKnobSize::S, "RANDOM", 0.0f, 1.0f, false, fmt::pct,       false, mod::Dest::Lfo1RandomRate),
              knobAmp    (p, &proc.patch().lfo[0].amplitude,  AtomicKnobSize::S, "AMP",    0.0f, 1.0f, false, fmt::pct,       false, mod::Dest::Lfo1Amplitude),
              // W↔RND: knob value reads as "random amount" (L=wave 0%, R=rand 100%).
              // Atomic stores the engine's mix convention (1=wave, 0=random) —
              // AtomicKnob's `invertValue=true` does the complement on the way in/out.
              knobMix    (p, &proc.patch().lfo[0].mix,        AtomicKnobSize::S, juce::String::fromUTF8 ("W↔RND"), 0.0f, 1.0f, false, fmt::pct, true,  mod::Dest::Lfo1Mix),
              // SMOOTH is intentionally not a modulation destination (engineer §2).
              knobSmooth (p, &proc.patch().lfo[0].smooth,     AtomicKnobSize::S, "SMOOTH", 0.0f, 1.0f, false, fmt::pct,       false, mod::Dest::Count)
        {
            addAndMakeVisible (preview);

            for (int i = 0; i < 4; ++i)
            {
                tabs[i] = std::make_unique<LFOTab> (i, [this] (int idx) { selectLFO (idx); }, &activeLFO);
                preview.addAndMakeVisible (*tabs[i]);
            }

            waveSeg.setSelectedValue (proc.patch().lfo[0].wave.load(), false);
            waveSeg.onChange = [this] (int v) { proc.patch().lfo[activeLFO].wave.store (v); preview.repaint(); };
            addAndMakeVisible (waveSeg);

            swBipolar.setValue (proc.patch().lfo[0].bipolar.load(), false);
            swBipolar.onChange = [this] (bool v) { proc.patch().lfo[activeLFO].bipolar.store (v); };
            addAndMakeVisible (swBipolar);

            swKbRst.setValue (proc.patch().lfo[0].kbReset.load(), false);
            swKbRst.onChange = [this] (bool v) { proc.patch().lfo[activeLFO].kbReset.store (v); };
            addAndMakeVisible (swKbRst);

            addAndMakeVisible (knobRate);
            addAndMakeVisible (knobRandom);
            addAndMakeVisible (knobAmp);
            addAndMakeVisible (knobMix);
            addAndMakeVisible (knobSmooth);

            startTimerHz (15);
        }

        void resized() override
        {
            // CLOCK SYNC stub area in header right
            auto hr = headerRightBounds();
            stubR = juce::Rectangle<int> (getWidth() - dim::kPanePadSide - 110,
                                           hr.getY(), 110, 22);

            auto body = bodyBounds();

            // ---- Preview strip ----
            const int previewH = 70;
            auto previewR = body.removeFromTop (previewH);
            preview.setBounds (previewR);

            // tabs overlay positions inside preview
            for (int i = 0; i < 4; ++i)
                tabs[i]->setBounds (6 + i * 44, 6, 42, 18);

            body.removeFromTop (6);

            // ---- Bottom row reserve (wave Seg · switches) ----
            const int botH = 22;
            auto botR = body.removeFromBottom (botH);
            body.removeFromBottom (4);
            {
                const int segW = 132;
                waveSeg .setBounds (botR.getX(), botR.getY(), segW, botH);
                const int swW = 78, gap = 6;
                int x = botR.getRight() - 90 /* stub */ - gap - swW;
                swKbRst.setBounds (x, botR.getY(), swW, botH);
                x -= (swW + gap);
                swBipolar.setBounds (x, botR.getY(), swW, botH);
                botRowR = botR;
            }

            // ---- Knob row (middle) ----
            {
                AtomicKnob* knobs[5] = { &knobRate, &knobRandom, &knobAmp, &knobMix, &knobSmooth };
                const int n = 5;
                const int cellW = body.getWidth() / n;
                for (int i = 0; i < n; ++i)
                {
                    auto cell = juce::Rectangle<int> (body.getX() + i * cellW, body.getY(),
                                                       cellW, body.getHeight());
                    auto cb = cell.withSizeKeepingCentre (knobs[i]->cellWidth(),
                                                           knobs[i]->cellHeight());
                    knobs[i]->setBounds (cb);
                }
            }
        }

        void paint (juce::Graphics& g) override
        {
            Pane::paint (g);

            // Header right: ◇ CLOCK SYNC stub
            g.setFont (font::mono (8.5f, 0.18f));
            g.setColour (col::InkLo.withAlpha (0.55f));
            g.drawText (juce::String::fromUTF8 ("◇ CLOCK SYNC"),
                        stubR, juce::Justification::centredRight, false);

            // Bottom row: ◇ 1-SHOT stub on right
            if (! botRowR.isEmpty())
            {
                auto sR = botRowR.withX (botRowR.getRight() - 80).withWidth (80);
                g.setFont (font::mono (8.5f, 0.18f));
                g.setColour (col::InkLo.withAlpha (0.50f));
                g.drawText (juce::String::fromUTF8 ("◇ 1-SHOT"),
                            sR, juce::Justification::centredRight, false);
            }
        }

    private:
        void selectLFO (int idx)
        {
            if (idx < 0 || idx > 3 || idx == activeLFO) return;
            activeLFO = idx;
            auto& lfo = proc.patch().lfo[activeLFO];
            knobRate  .setTarget (&lfo.rate);
            knobRandom.setTarget (&lfo.randomRate);
            knobAmp   .setTarget (&lfo.amplitude);
            knobMix   .setTarget (&lfo.mix);
            knobSmooth.setTarget (&lfo.smooth);

            // Modulation destinations follow the active LFO (Lfo1* → Lfo2* etc.).
            const int base = idx * 4;   // Lfo1Rate=16, Lfo2Rate=20, ...
            knobRate  .setModDest ((mod::Dest) ((int) mod::Dest::Lfo1Rate       + base));
            knobMix   .setModDest ((mod::Dest) ((int) mod::Dest::Lfo1Mix        + base));
            knobAmp   .setModDest ((mod::Dest) ((int) mod::Dest::Lfo1Amplitude  + base));
            knobRandom.setModDest ((mod::Dest) ((int) mod::Dest::Lfo1RandomRate + base));

            waveSeg.setSelectedValue (lfo.wave.load(), false);
            swBipolar.setValue (lfo.bipolar.load(), false);
            swKbRst  .setValue (lfo.kbReset.load(), false);
            preview.repaint();
            for (int i = 0; i < 4; ++i) tabs[i]->repaint();
        }

        void timerCallback() override
        {
            // Refresh from atomics on focus / preset change
            auto& lfo = proc.patch().lfo[activeLFO];
            waveSeg.setSelectedValue (lfo.wave.load(), false);
            swBipolar.setValue (lfo.bipolar.load(), false);
            swKbRst  .setValue (lfo.kbReset.load(), false);
        }

        // ---- inner: animated preview strip ----
        class Preview : public juce::Component, private juce::Timer
        {
        public:
            Preview (TidesAudioProcessor& pp, int& activeIdxRef)
                : proc (pp), activeIdxRefRef (activeIdxRef)
            {
                lastMs = juce::Time::getMillisecondCounterHiRes();
                startTimerHz (30);
            }

            void paint (juce::Graphics& g) override
            {
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRoundedRectangle (rf, 5.0f);
                g.setColour (col::Divider2);
                g.drawRoundedRectangle (rf.reduced (0.5f), 5.0f, 1.0f);
                g.setColour (col::EdgeHi);
                g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

                const auto& lfo = proc.patch().lfo[(size_t) activeIdxRefRef];
                const float rate    = lfo.rate.load();
                const float amp     = lfo.amplitude.load();
                const int   wave    = lfo.wave.load();
                const float mix     = lfo.mix.load();           // 1 = wave only, 0 = random only
                const float rndRate = lfo.randomRate.load();    // 0..1 → 0 / 0.1..60 Hz S&H

                const float W = rf.getWidth();
                const float H = rf.getHeight();
                // Top padding clears the LFO1..4 tab strip overlay (24px tall + 6px gap).
                const float padX     = 6.0f;
                const float padYTop  = 30.0f;
                const float padYBot  = 8.0f;
                const float drawW = W - padX * 2.0f;
                const float drawH = H - padYTop - padYBot;
                const float cy = padYTop + drawH * 0.5f;
                const float padY = padYTop;   // alias for clip rect below

                // Baseline
                g.setColour (col::Divider2.withAlpha (0.15f));
                g.drawLine (padX, cy, padX + drawW, cy, 0.5f);

                // Path: 6 cycles across the strip, phase-shifted by accumulated time
                juce::Path path;
                const int N = 360;
                const float cycles = 6.0f;
                const float hz = dsp::map::lfoRateHz (rate);    // single source of truth
                phase = std::fmod (phase + dtSec * hz / cycles, 1.0f);
                for (int i = 0; i <= N; ++i)
                {
                    const float u = (float) i / (float) N;
                    const float t = (u + phase) * cycles;
                    float y = 0.0f;
                    switch (wave)
                    {
                        case 0: y = std::sin (t * juce::MathConstants<float>::twoPi); break;
                        case 1: y = 2.0f * std::abs (std::fmod (t + 0.25f, 1.0f) * 2.0f - 1.0f) - 1.0f; break;
                        case 2: y = (std::fmod (t, 1.0f) * 2.0f) - 1.0f; break;
                        case 3: y = std::fmod (t, 1.0f) < 0.5f ? 1.0f : -1.0f; break;
                    }
                    // Preview shows the pure wave shape scaled by AMP, always
                    // centred on the strip. W↔RND and Bipolar are DSP-side and
                    // don't change the visual amplitude.
                    y *= amp;

                    const float px = padX + u * drawW;
                    const float py = cy - y * (drawH * 0.45f);
                    if (i == 0) path.startNewSubPath (px, py);
                    else         path.lineTo (px, py);
                }

                // ---- W↔RND crossfade --------------------------------------------
                // Internal mix convention: 1 = wave-only, 0 = random-only.
                // Visual `randAmount = 1 - mix`.
                //   wave alpha:    1.00 → 0.18  as randAmount rises
                //   overlay alpha: 0.00 → 1.00  in the same direction
                const float randAmount = 1.0f - mix;
                const float waveAlpha  = juce::jmap (randAmount, 0.0f, 1.0f, 1.0f, 0.18f);

                g.setColour (col::Amber.withAlpha (0.20f * waveAlpha));
                g.strokePath (path, juce::PathStrokeType (3.0f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));
                g.setColour (col::Amber.withAlpha (waveAlpha));
                g.strokePath (path, juce::PathStrokeType (1.4f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));

                // ---- Noise overlay: stepped jitter HUGS the wave ----------------
                // Step density is driven by the *actual* S&H clock from
                // randomRate so the RANDOM knob has a clear visual effect:
                //   randomRate = 0   → 1 wide held step (no scrolling)
                //   randomRate ↑     → more, narrower steps
                // Vertical offset is small (max ±0.30) so the overlay tracks the
                // wave closely rather than spanning the strip.
                if (randAmount > 0.005f)
                {
                    const float waveHz = dsp::map::lfoRateHz   (rate);
                    const float randHz = dsp::map::lfoRandomHz (rndRate);
                    const float timeVisibleSec = (waveHz > 0.0001f)
                        ? cycles / waveHz : (float) cycles;
                    const int   steps = (randHz <= 0.0001f)
                        ? 1
                        : juce::jlimit (1, 96, juce::roundToInt (randHz * timeVisibleSec));
                    const float stepW = drawW / (float) steps;
                    const float scrollSteps = phase * (float) steps;
                    const float scrollFrac  = scrollSteps - std::floor (scrollSteps);
                    const int   scrollInt   = (int) std::floor (scrollSteps);

                    auto waveValueAt = [&] (float u01) -> float
                    {
                        const float t = (u01 + phase) * cycles;
                        float y = 0.0f;
                        switch (wave)
                        {
                            case 0: y = std::sin (t * juce::MathConstants<float>::twoPi); break;
                            case 1: y = 2.0f * std::abs (std::fmod (t + 0.25f, 1.0f) * 2.0f - 1.0f) - 1.0f; break;
                            case 2: y = (std::fmod (t, 1.0f) * 2.0f) - 1.0f; break;
                            case 3: y = std::fmod (t, 1.0f) < 0.5f ? 1.0f : -1.0f; break;
                        }
                        return y;
                    };

                    const float maxOffset = 0.30f * randAmount;   // close to the wave

                    juce::Path noisePath;
                    bool first = true;
                    // Draw 2 extra steps either side so the scrolling line never
                    // shows a starting/ending edge.
                    for (int i = -2; i <= steps + 2; ++i)
                    {
                        const float xStart = padX + ((float) i - scrollFrac) * stepW;
                        const float xEnd   = xStart + stepW;

                        // Sample the wave at this step's center so the noise sits on the wave.
                        const float uCenter = juce::jlimit (0.0f, 1.0f,
                                                             (xStart + stepW * 0.5f - padX) / drawW);
                        const float wy = waveValueAt (uCenter);

                        // Deterministic random per step index (stable as it scrolls).
                        const int   seed = i + scrollInt;
                        const float r    = juce::Random ((juce::int64) seed * 2654435761ULL).nextFloat();
                        const float noiseOffset = (r * 2.0f - 1.0f) * maxOffset;

                        const float yAdd = juce::jlimit (-1.05f, 1.05f, wy + noiseOffset) * amp;
                        const float py   = cy - yAdd * (drawH * 0.45f);

                        if (first) { noisePath.startNewSubPath (xStart, py); first = false; }
                        else        noisePath.lineTo            (xStart, py);
                        noisePath.lineTo (xEnd, py);
                    }

                    // Multi-pass fuzzy stroke (wide-soft → narrow-bright).
                    g.saveState();
                    g.reduceClipRegion (juce::Rectangle<int> ((int) padX, (int) padY,
                                                               (int) drawW, (int) drawH));
                    g.setColour (col::Purple.withAlpha (0.08f * randAmount));
                    g.strokePath (noisePath, juce::PathStrokeType (6.0f));
                    g.setColour (col::Purple.withAlpha (0.18f * randAmount));
                    g.strokePath (noisePath, juce::PathStrokeType (4.0f));
                    g.setColour (col::Purple.withAlpha (0.42f * randAmount));
                    g.strokePath (noisePath, juce::PathStrokeType (2.4f));
                    g.setColour (col::PurpleHot.withAlpha (0.95f * randAmount));
                    g.strokePath (noisePath, juce::PathStrokeType (1.4f));
                    g.restoreState();
                }

                // Rate readout top-right
                const char* waveNames[] = { "SIN", "TRI", "RMP", "SQR" };
                const auto txt = juce::String (waveNames[juce::jlimit (0, 3, wave)])
                                 + juce::String::fromUTF8 ("  ·  ")
                                 + fmt::lfoRateHz (rate);
                g.setFont (font::mono (9.5f, 0.06f));
                g.setColour (col::Amber);
                auto txtR = juce::Rectangle<int> ((int) (padX + drawW * 0.5f), 4,
                                                   (int) (drawW * 0.5f - 4), 14);
                g.drawText (txt, txtR, juce::Justification::centredRight, false);
            }

        private:
            void timerCallback() override
            {
                const double now = juce::Time::getMillisecondCounterHiRes();
                dtSec = (float) ((now - lastMs) * 0.001);
                lastMs = now;
                repaint();
            }

            TidesAudioProcessor& proc;
            int& activeIdxRefRef;
            double lastMs { 0 };
            float  dtSec  { 0.0f };
            float  phase  { 0.0f };
        };

        // ---- inner: LFO tab overlay button ----
        class LFOTab : public juce::Component
        {
        public:
            LFOTab (int idx, std::function<void (int)> onClick, int* activeIdx)
                : index (idx), onClickFn (std::move (onClick)), activePtr (activeIdx)
            {
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
            }

            void mouseDown (const juce::MouseEvent&) override
            {
                if (onClickFn) onClickFn (index);
            }

            void paint (juce::Graphics& g) override
            {
                const bool active = (activePtr && *activePtr == index);
                auto rf = getLocalBounds().toFloat();
                g.setColour (active ? juce::Colour::fromFloatRGBA (0.48f, 0.84f, 1.0f, 0.18f)
                                    : juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.05f, 0.60f));
                g.fillRoundedRectangle (rf, 3.0f);
                g.setColour (active ? col::Amber.withAlpha (0.50f) : col::Divider2);
                g.drawRoundedRectangle (rf.reduced (0.5f), 3.0f, 1.0f);

                g.setColour (active ? col::Amber : col::InkMid);
                g.setFont (font::mono (9.0f, 0.10f));
                g.drawText ("LFO" + juce::String (index + 1),
                            getLocalBounds(), juce::Justification::centred, false);
            }

        private:
            int index;
            std::function<void (int)> onClickFn;
            int* activePtr;
        };

        TidesAudioProcessor& proc;
        int activeLFO { 0 };

        Preview preview;
        std::array<std::unique_ptr<LFOTab>, 4> tabs;

        Seg waveSeg;
        Switch swBipolar, swKbRst;

        AtomicKnob knobRate, knobRandom, knobAmp, knobMix, knobSmooth;

        juce::Rectangle<int> stubR, botRowR;
    };
}
