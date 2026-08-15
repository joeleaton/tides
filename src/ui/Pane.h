#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"

namespace tides::ui
{
    // Common pane shell: dark Panel background, 1 px inner top highlight,
    // numbered uppercase-mono section title in the top-left padding.
    // Subclasses lay out their controls in resized().
    class Pane : public juce::Component
    {
    public:
        Pane (juce::String numStr, juce::String nameStr)
            : sectionNum (std::move (numStr)), sectionName (std::move (nameStr)) {}

        void paint (juce::Graphics& g) override
        {
            g.setColour (col::Panel);
            g.fillRect (getLocalBounds());
            g.setColour (col::EdgeHi);
            g.fillRect (getLocalBounds().withHeight (1));

            // Section head
            auto pad = getLocalBounds().reduced (dim::kPanePadSide, dim::kPanePadTop)
                                       .removeFromTop (14);
            auto numBox = pad.removeFromLeft (24);
            g.setFont (font::mono (10.0f, 0.22f));
            g.setColour (col::InkLo);
            g.drawText (sectionNum, numBox, juce::Justification::topLeft, false);
            g.setColour (col::Ink);
            g.drawText (sectionName, pad, juce::Justification::topLeft, false);
        }

        // Body bounds available to subclasses (below the section header row).
        juce::Rectangle<int> bodyBounds() const
        {
            auto r = getLocalBounds().reduced (dim::kPanePadSide, 0);
            r.removeFromTop (dim::kPanePadTop + kHeaderRowH);
            r.removeFromBottom (dim::kPanePadBot);
            return r;
        }
        // Header row right edge — for docking toggles next to the section title.
        juce::Rectangle<int> headerRightBounds() const
        {
            auto r = getLocalBounds().reduced (dim::kPanePadSide, 0);
            return r.withY (dim::kPanePadTop - 4).withHeight (kHeaderRowH);
        }

        static constexpr int kHeaderRowH = 22;

    private:
        juce::String sectionNum;
        juce::String sectionName;
    };
}
