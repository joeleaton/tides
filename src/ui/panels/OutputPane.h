#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"
#include "ui/Pane.h"
#include "ui/Knob.h"
#include "ui/Primitives.h"
#include "ui/Format.h"
#include "PluginProcessor.h"
#include "state/FactoryPresets.h"

namespace tides::ui::panels
{
    // 04 OUTPUT + 05 CHARACTER pane.
    // Row 1:        DRY/WET (M) · PAN (M) · LEVEL (M)
    // Divider + "05 CHARACTER" subhead
    // Row 2:        WILD/TAME (L bipolar) · MASTER (L)
    // Wild/Tame band: WILD ........ TAME  (active side highlighted)
    //                 noise · drift · detune · saturation
    // Footer stub:  ◇ QUALITY · MICROTUNING · MORPH — coming
    class OutputPane : public Pane
    {
    public:
        explicit OutputPane (TidesAudioProcessor& p)
            : Pane ("04", "OUTPUT"),
              knobDryWet (p, KnobSize::M, "DRY/WET", "out_drywet", false, fmt::pct, params::P::OutDryWet, mod::Dest::ResDryWet),
              knobPan    (p, KnobSize::M, "PAN",     "out_pan",    false, fmt::pan, params::P::OutPanning, mod::Dest::ResPanning),
              knobLevel  (p, KnobSize::M, "LEVEL",   "out_level",  false, fmt::pct, params::P::OutLevel,  mod::Dest::ResLevel),
              // Wild/Tame: unipolar 0..1 (DSP scales drift/noise/saturator
              // linearly with the value). CCW → tame, CW → wild — label reads
              // left-to-right accordingly. Now a real mod destination so
              // sources can sweep the character on the fly.
              knobWild   (p, KnobSize::L, juce::String::fromUTF8 ("TAME ↔ WILD"),
                                                       "wild_tame", false, fmt::wildTame, params::P::WildTame, mod::Dest::WildTame),
              knobMaster (p, KnobSize::L, "MASTER",  "master",     false, fmt::pct, params::P::Master,    mod::Dest::Count)
        {
            addAndMakeVisible (knobDryWet);
            addAndMakeVisible (knobPan);
            addAndMakeVisible (knobLevel);
            addAndMakeVisible (knobWild);
            addAndMakeVisible (knobMaster);
        }

        void resized() override
        {
            auto body = bodyBounds();

            // ---- Row 1: DRY/WET · PAN · LEVEL ----
            auto row1 = body.removeFromTop (knobDryWet.cellHeight() + 4);
            layoutRow (row1, { &knobDryWet, &knobPan, &knobLevel });

            // Divider + "05 CHARACTER" subhead reserved area
            body.removeFromTop (6);
            charHeadR = body.removeFromTop (16);
            body.removeFromTop (4);

            // ---- Row 2: WILD/TAME · MASTER ----
            auto row2 = body.removeFromTop (knobWild.cellHeight() + 4);
            layoutRow (row2, { &knobWild, &knobMaster });

            // Quality + Morph moved to GroupBar — no bottom strip.
        }

        void paint (juce::Graphics& g) override
        {
            Pane::paint (g);

            // ---- divider line above CHARACTER subhead ----
            if (! charHeadR.isEmpty())
            {
                const int yDiv = charHeadR.getY() - 4;
                g.setColour (col::DividerGrout);
                g.fillRect (0, yDiv, getWidth(), 1);

                // Subhead "05 CHARACTER"
                auto pad = juce::Rectangle<int> (dim::kPanePadSide, charHeadR.getY(),
                                                  getWidth() - 2 * dim::kPanePadSide,
                                                  charHeadR.getHeight());
                auto numBox = pad.removeFromLeft (24);
                g.setFont (font::mono (10.0f, 0.22f));
                g.setColour (col::InkLo);
                g.drawText ("05", numBox, juce::Justification::topLeft, false);
                g.setColour (col::Ink);
                g.drawText ("CHARACTER", pad, juce::Justification::topLeft, false);
            }

        }

    private:
        static void layoutRow (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
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
        }

        Knob knobDryWet, knobPan, knobLevel;
        Knob knobWild, knobMaster;

        juce::Rectangle<int> charHeadR;
    };
}
