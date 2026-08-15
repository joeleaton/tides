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
    // 01 EXCITER pane.
    // Header right:  KB RST · EXT IN
    // Row 1:         WAVE (L)   COARSE (M)   FINE (M)
    // Row 2:         S&H RATE   W↔N MIX     LEVEL
    // Footer hint:   SUBTRACTIVE SECTION · feeds resonator input
    class ExciterPane : public Pane, private juce::Timer
    {
    public:
        explicit ExciterPane (TidesAudioProcessor& p)
            : Pane ("01", "EXCITER"),
              proc (p),
              knobWave   (p, KnobSize::L, "WAVE",     "ex_wave",       false, fmt::waveMorph,            params::P::ExWave,        mod::Dest::ExciterWave),
              // ex_coarse / ex_fine share one mod destination (ExciterFrequency).
              knobCoarse (p, KnobSize::M, "COARSE",   "ex_coarse",     false, fmt::oct,                  params::P::ExCoarseFreq,  mod::Dest::ExciterFrequency),
              knobFine   (p, KnobSize::M, "FINE",     "ex_fine",       false, [] (float v) { return fmt::semi (v, 3.0f); }, params::P::ExFineFreq, mod::Dest::ExciterFrequency),
              knobShRate (p, KnobSize::M, "S&H RATE", "ex_sh_rate",    false, fmt::pct,                  params::P::ExNoiseSHRate, mod::Dest::ExciterNoiseSHRate),
              knobWNMix  (p, KnobSize::M, juce::String::fromUTF8 ("W↔N MIX"), "ex_wave_noise", false, fmt::waveNoiseMix, params::P::ExWaveNoiseMix, mod::Dest::ExciterWaveNoiseMix),
              knobLevel  (p, KnobSize::M, "LEVEL",    "ex_level",      false, fmt::pct,                  params::P::ExLevel,       mod::Dest::ExciterLevel),
              swKbRst    ("KB Rst"),
              swExtIn    ("Ext In")
        {
            addAndMakeVisible (knobWave);
            addAndMakeVisible (knobCoarse);
            addAndMakeVisible (knobFine);
            addAndMakeVisible (knobShRate);
            addAndMakeVisible (knobWNMix);
            addAndMakeVisible (knobLevel);

            // Switches → per-group PatchState atomics
            swKbRst.setValue (proc.patch().kbReset.load(), false);
            swKbRst.onChange = [this] (bool v) { proc.patch().kbReset.store (v); };
            addAndMakeVisible (swKbRst);

            swExtIn.setValue (proc.patch().extIn.load(), false);
            swExtIn.onChange = [this] (bool v) { proc.patch().extIn.store (v); };
            addAndMakeVisible (swExtIn);

            startTimerHz (15);   // resync switches on focus / preset change
        }

        void resized() override
        {
            // Switches docked right of section title
            auto hr = headerRightBounds().reduced (0, 0);
            const int swW = 78, swH = 22, swG = 6;
            int x = getWidth() - dim::kPanePadSide - swW;
            swExtIn.setBounds (x, hr.getY(), swW, swH);
            x -= swW + swG;
            swKbRst.setBounds (x, hr.getY(), swW, swH);

            auto body = bodyBounds();
            // Footer hint reservation
            const int hintH = 18;
            auto footerR = body.removeFromBottom (hintH).reduced (0, 2);
            (void) footerR;

            // Two equal rows of three knobs, gap 12
            auto row1 = body.removeFromTop (body.getHeight() / 2 - 2);
            body.removeFromTop (4);
            auto row2 = body;

            auto layoutRow = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
            {
                const int n = (int) knobs.size();
                const int cellW = r.getWidth() / n;
                int i = 0;
                for (auto* k : knobs)
                {
                    auto cell = juce::Rectangle<int> (r.getX() + i * cellW, r.getY(),
                                                       cellW, r.getHeight());
                    auto cb = cell.withSizeKeepingCentre (k->cellWidth(), k->cellHeight());
                    k->setBounds (cb);
                    ++i;
                }
            };
            layoutRow (row1, { &knobWave, &knobCoarse, &knobFine });
            layoutRow (row2, { &knobShRate, &knobWNMix, &knobLevel });
        }

        void paint (juce::Graphics& g) override
        {
            Pane::paint (g);

            // Footer hint
            auto body = bodyBounds();
            auto footerR = juce::Rectangle<int> (body.getX(), body.getBottom() - 18,
                                                  body.getWidth(), 18);
            // Top hairline
            g.setColour (col::DividerGrout);
            g.fillRect (footerR.withHeight (1));
            g.setFont (font::mono (8.5f, 0.10f));
            g.setColour (col::InkLo);
            g.drawText (juce::String::fromUTF8 ("SUBTRACTIVE SECTION  ·  feeds resonator input"),
                        footerR, juce::Justification::centred, false);
        }

    private:
        void timerCallback() override
        {
            // Resync the non-APVTS switches in case the focused group changed
            // or a preset was loaded.
            swKbRst.setValue (proc.patch().kbReset.load(), false);
            swExtIn.setValue (proc.patch().extIn.load(), false);
        }

        TidesAudioProcessor& proc;
        Knob knobWave, knobCoarse, knobFine, knobShRate, knobWNMix, knobLevel;
        Switch swKbRst, swExtIn;
    };
}
