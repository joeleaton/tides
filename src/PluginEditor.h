#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/TopBar.h"
#include "ui/GroupBar.h"
#include "ui/Theme.h"
#include "ui/panels/ExciterPane.h"
#include "ui/panels/EnvelopePane.h"
#include "ui/panels/ResonatorPane.h"
#include "ui/panels/OutputPane.h"
#include "ui/panels/QuadLFOPane.h"
#include "ui/panels/ModulationPane.h"
#include "ui/VizStrip.h"
#include "ui/PerfRow.h"

namespace tides::ui
{
    // Translucent overlay dropped on top of the six edit panes while morph
    // is active. Eats mouse clicks so knobs can't drift, and paints a
    // hatched amber-tinted wash + a "MORPH ACTIVE" strip so the reason is
    // obvious. Sized by the editor to cover just the panels (top bar and
    // group bar stay live so the user can pull morph back to 0).
    class MorphLockOverlay : public juce::Component
    {
    public:
        MorphLockOverlay()
        {
            setInterceptsMouseClicks (true, false);
            setMouseCursor (juce::MouseCursor::NoCursor);
        }
        void paint (juce::Graphics& g) override
        {
            auto rf = getLocalBounds().toFloat();
            // Wash
            g.setColour (col::BgDeep.withAlpha (0.55f));
            g.fillRect (rf);
            // Diagonal hatch — cheap "not editable" texture
            g.setColour (col::Amber.withAlpha (0.06f));
            const float step = 12.0f;
            for (float x = -rf.getHeight(); x < rf.getWidth(); x += step)
                g.drawLine (x, 0.0f, x + rf.getHeight(), rf.getHeight(), 1.0f);
            // Banner
            const int bh = 34;
            auto banner = getLocalBounds().withHeight (bh)
                            .withCentre ({ getWidth() / 2, getHeight() / 2 });
            g.setColour (col::Amber.withAlpha (0.14f));
            g.fillRoundedRectangle (banner.toFloat(), 6.0f);
            g.setColour (col::Amber.withAlpha (0.55f));
            g.drawRoundedRectangle (banner.toFloat().reduced (0.5f), 6.0f, 1.2f);
            g.setColour (col::Amber);
            g.setFont (font::mono (11.0f, 0.20f, true));
            g.drawText (juce::String::fromUTF8 ("▸ MORPH ACTIVE  ·  PULL MORPH TO 0 TO EDIT"),
                        banner, juce::Justification::centred, false);
        }
        // Absorb clicks so nothing beneath moves.
        void mouseDown (const juce::MouseEvent&) override {}
    };
}

// Tides editor — 1280×800 fixed-size.
// Phases: P0 frame + skeleton · P1 TopBar + GroupBar · ... · P10 patch mode.
class TidesAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit TidesAudioProcessorEditor (TidesAudioProcessor&);
    ~TidesAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    TidesAudioProcessor&   proc;
    int                    lastEpoch { 0 };
    tides::ui::TopBar          topBar;
    tides::ui::GroupBar        groupBar;
    tides::ui::panels::ExciterPane  exciterPane;
    tides::ui::panels::EnvelopePane envelopePane;
    tides::ui::panels::ResonatorPane resonatorPane;
    tides::ui::panels::OutputPane    outputPane;
    tides::ui::panels::QuadLFOPane   quadLfoPane;
    tides::ui::panels::ModulationPane modulationPane;
    tides::ui::VizStrip               vizStrip;
    tides::ui::PerfRow                perfRow;
    tides::ui::MorphLockOverlay       morphLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TidesAudioProcessorEditor)
};
