#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"
#include "ui/KnobLnF.h"

namespace tides::ui
{
    // Compact label-less bipolar knob used in the Modulation pane's source
    // tiles. Binds to APVTS via SliderAttachment (the source macros are part
    // of the host-automation surface), so changes persist across the per-
    // block APVTS → PatchState sync and survive focused-group switches.
    class MacroKnob : public juce::Component
    {
    public:
        MacroKnob (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (juce::degreesToRadians (-210.0f + 360.0f),
                                        juce::degreesToRadians (  30.0f + 360.0f),
                                        true);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            slider.setVelocityBasedMode (false);
            slider.setMouseDragSensitivity (180);
            slider.setLookAndFeel (&lnf);
            slider.getProperties().set ("bipolar", true);
            slider.setDoubleClickReturnValue (true, 0.0);   // recenter
            addAndMakeVisible (slider);

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                            apvts, paramID, slider);
        }

        ~MacroKnob() override { slider.setLookAndFeel (nullptr); }

        void resized() override { slider.setBounds (getLocalBounds()); }

    private:
        KnobLnF lnf;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
}
