#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"
#include "ui/Pane.h"
#include "ui/Knob.h"
#include "ui/Primitives.h"
#include "ui/Format.h"
#include "PluginProcessor.h"

namespace tides::ui::panels
{
    // 02 ENVELOPE pane.
    // Header right:  Seg S / M / L  (envRange)
    // Display:       EnvCurve mini-svg
    // Row:           ONSET · A · D · S · R   (5 small knobs)
    // Switch row:    Hold · Legato · Loop    (Loop on => also forces Legato)
    class EnvelopePane : public Pane, private juce::Timer
    {
    public:
        explicit EnvelopePane (TidesAudioProcessor& p)
            : Pane ("02", "ENVELOPE"),
              proc (p),
              curve (p),
              segRange ({ { "S", 0 }, { "M", 1 }, { "L", 2 } }),
              knobOnset   (p, KnobSize::S, "ONSET", "env_onset",   false, [] (float v) { return fmt::ms (v); }, params::P::EnvOnsetDelay, mod::Dest::EnvOnsetDelay),
              knobAttack  (p, KnobSize::S, "A",     "env_attack",  false, [] (float v) { return fmt::ms (v); }, params::P::EnvAttack,     mod::Dest::EnvAttack),
              knobDecay   (p, KnobSize::S, "D",     "env_decay",   false, [] (float v) { return fmt::ms (v); }, params::P::EnvDecay,      mod::Dest::EnvDecay),
              // Sustain & Release are NOT modulation destinations (engineer §2.5).
              knobSustain (p, KnobSize::S, "S",     "env_sustain", false, fmt::pct,                              params::P::EnvSustain,    mod::Dest::Count),
              knobRelease (p, KnobSize::S, "R",     "env_release", false, [] (float v) { return fmt::ms (v); }, params::P::EnvRelease,    mod::Dest::Count),
              swHold   ("Hold"),
              swLegato ("Legato"),
              swLoop   ("Loop")
        {
            addAndMakeVisible (curve);

            segRange.setSelectedValue (proc.patch().envRange.load(), false);
            segRange.onChange = [this] (int v) { proc.patch().envRange.store (v); };
            addAndMakeVisible (segRange);

            addAndMakeVisible (knobOnset);
            addAndMakeVisible (knobAttack);
            addAndMakeVisible (knobDecay);
            addAndMakeVisible (knobSustain);
            addAndMakeVisible (knobRelease);

            swHold.setValue (proc.patch().envHold.load(), false);
            swHold.onChange = [this] (bool v) { proc.patch().envHold.store (v); };
            addAndMakeVisible (swHold);

            swLegato.setValue (proc.patch().envLegato.load(), false);
            swLegato.onChange = [this] (bool v) { proc.patch().envLegato.store (v); };
            addAndMakeVisible (swLegato);

            swLoop.setValue (proc.patch().envLoop.load(), false);
            swLoop.onChange = [this] (bool v)
            {
                proc.patch().envLoop.store (v);
                if (v)   // Loop=on auto-enables Legato per design
                {
                    swLegato.setValue (true, false);
                    proc.patch().envLegato.store (true);
                }
            };
            addAndMakeVisible (swLoop);

            startTimerHz (15);  // resync non-APVTS switches/seg on focus + preset change
        }

        void resized() override
        {
            // S/M/L docked right of section title
            const int segW = 78, segH = 22;
            auto hr = headerRightBounds();
            segRange.setBounds (getWidth() - dim::kPanePadSide - segW,
                                hr.getY(), segW, segH);

            auto body = bodyBounds();

            // Top: envelope curve display
            const int curveH = 64;
            auto curveR = body.removeFromTop (curveH);
            curve.setBounds (curveR);

            body.removeFromTop (8);

            // Bottom: switches row
            const int swRowH = 22;
            auto swRow = body.removeFromBottom (swRowH);
            body.removeFromBottom (6);
            {
                const int n = 3;
                const int gap = 4;
                const int cellW = (swRow.getWidth() - gap * (n - 1)) / n;
                swHold  .setBounds (swRow.getX(),                       swRow.getY(), cellW, swRowH);
                swLegato.setBounds (swRow.getX() + (cellW + gap),       swRow.getY(), cellW, swRowH);
                swLoop  .setBounds (swRow.getX() + 2 * (cellW + gap),   swRow.getY(), cellW, swRowH);
            }

            // Middle: 5 small knobs evenly spaced
            {
                const int n = 5;
                const int cellW = body.getWidth() / n;
                Knob* knobs[n] = { &knobOnset, &knobAttack, &knobDecay, &knobSustain, &knobRelease };
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

    private:
        void timerCallback() override
        {
            segRange.setSelectedValue (proc.patch().envRange.load(), false);
            swHold  .setValue (proc.patch().envHold  .load(), false);
            swLegato.setValue (proc.patch().envLegato.load(), false);
            swLoop  .setValue (proc.patch().envLoop  .load(), false);
        }

        // ---- inner: EnvCurve display --------------------------------
        class EnvCurve : public juce::Component, private juce::Timer
        {
        public:
            explicit EnvCurve (TidesAudioProcessor& p) : proc (p)
            {
                startTimerHz (20);
            }

            void paint (juce::Graphics& g) override
            {
                // Inset panel
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRoundedRectangle (rf, 4.0f);
                g.setColour (col::DividerGrout);
                g.drawRoundedRectangle (rf.reduced (0.5f), 4.0f, 1.0f);
                g.setColour (col::EdgeHi);
                g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

                // Read current ADSR values from APVTS
                auto get = [&] (const char* id) -> float
                {
                    if (auto* prm = proc.apvts.getParameter (id))
                        return prm->getValue();
                    return 0.0f;
                };
                const float onset  = get ("env_onset");
                const float attack = get ("env_attack");
                const float decay  = get ("env_decay");
                const float sus    = get ("env_sustain");
                const float rel    = get ("env_release");

                // Drawing area (mimic design's 100×100 viewBox with 5..95 X range)
                const float padX = 6.0f, padY = 6.0f;
                const float W = (float) getWidth()  - padX * 2.0f;
                const float H = (float) getHeight() - padY * 2.0f;
                const float x0 = padX;
                const float y0 = padY;
                const float bottomY = y0 + H - 4.0f;
                const float topY    = y0 + 4.0f;

                // Baseline
                g.setColour (col::Divider2.withAlpha (0.18f));
                g.drawLine (x0, bottomY, x0 + W, bottomY, 0.5f);

                // Time scale (matches design's EnvCurve)
                const float total = 0.05f + onset + attack + decay + 0.30f + rel + 0.05f;
                auto seg = [&] (float frac) { return frac / total * W; };
                const float susH = sus * (H - 12.0f);

                juce::Path path;
                float x = x0;
                path.startNewSubPath (x, bottomY);
                x += seg (onset);                              path.lineTo (x, bottomY);
                x += seg (juce::jmax (attack, 0.005f));        path.lineTo (x, topY);
                x += seg (juce::jmax (decay,  0.005f));        path.lineTo (x, bottomY - susH);
                float sustEnd = juce::jmin (x + seg (0.30f), x0 + W - seg (rel));
                path.lineTo (sustEnd, bottomY - susH);
                x = sustEnd;
                x += seg (juce::jmax (rel, 0.005f));           path.lineTo (x, bottomY);

                // Glow
                g.setColour (col::Amber.withAlpha (0.18f));
                g.strokePath (path, juce::PathStrokeType (3.5f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));
                // Curve
                g.setColour (col::Amber);
                g.strokePath (path, juce::PathStrokeType (1.5f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));

                // Sustain-level dotted leader to right edge
                g.setColour (col::InkFaint);
                const float dashes[] = { 2.0f, 3.0f };
                juce::Line<float> leader { sustEnd, bottomY - susH, x0 + W, bottomY - susH };
                g.drawDashedLine (leader, dashes, 2, 0.5f);
            }

        private:
            void timerCallback() override { repaint(); }
            TidesAudioProcessor& proc;
        };

        TidesAudioProcessor& proc;
        EnvCurve curve;
        Seg segRange;
        Knob knobOnset, knobAttack, knobDecay, knobSustain, knobRelease;
        Switch swHold, swLegato, swLoop;
    };
}
