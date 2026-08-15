#pragma once
#include <JuceHeader.h>
#include <functional>
#include "ui/Theme.h"
#include "ui/KnobLnF.h"
#include "PluginProcessor.h"
#include "state/ParameterIDs.h"
#include "mod/ModDefs.h"

namespace tides::ui
{
    enum class KnobSize { S = 0, M = 1, L = 2, XL = 3 };

    // Single-cell Knob: label · rotary · value. Hosts a juce::Slider attached
    // to APVTS, and a custom rotary L&F (KnobLnF) that paints the ring/face/
    // detent/saved-tick. Cell layout matches design's .knob-cell.
    //
    // Optional `dest` (mod::Dest) marks the knob as modulatable. While the
    // Modulation pane's patch-mode is armed, modulatable knobs paint a
    // purple "selecting" outline and treat the next click as "assign me as
    // the destination for the armed source" instead of a value change.
    class Knob : public juce::Component,
                 private juce::Timer,
                 private juce::ChangeListener
    {
    private:
        // Transparent overlay placed ON TOP of the slider. When patch-mode is
        // armed and this knob is modulatable the overlay is shown +
        // intercepts mouse clicks (assigns the routing). When not armed the
        // overlay is hidden so the slider receives clicks normally.
        class AssignOverlay : public juce::Component
        {
        public:
            std::function<void()> onClick;
            AssignOverlay() { setInterceptsMouseClicks (true, false); }
            void mouseDown (const juce::MouseEvent&) override
            {
                if (onClick) onClick();
            }
        };

    public:
        Knob (TidesAudioProcessor& procRef,
              KnobSize size,
              const juce::String& labelText,
              const juce::String& apvtsParamID,
              bool bipolar = false,
              std::function<juce::String (float)> formatter = {},
              params::P savedParam = params::P::NumParams,
              mod::Dest dest = mod::Dest::Count)              // Count == not modulatable
            : proc (procRef),
              sz (size),
              label (labelText),
              fmt (std::move (formatter)),
              saved (savedParam),
              modDest (dest)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (juce::degreesToRadians (-210.0f + 360.0f),
                                        juce::degreesToRadians (  30.0f + 360.0f),
                                        true);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            slider.setVelocityBasedMode (false);
            slider.setMouseDragSensitivity (200);
            slider.setLookAndFeel (&lnf);
            slider.getProperties().set ("bipolar", bipolar);
            addAndMakeVisible (slider);

            assignOverlay.onClick = [this] { proc.assignConnection (modDest); };
            addChildComponent (assignOverlay);   // child but initially hidden

            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                            proc.apvts, apvtsParamID, slider);

            // Repaint to keep the value text and the saved tick fresh.
            slider.onValueChange = [this] { repaint(); };

            proc.patchMode.addChangeListener (this);
            refreshPatchModeVisual();

            // 30 Hz so the animated ribbon dot moves smoothly.
            startTimerHz (30);
        }

        ~Knob() override
        {
            proc.patchMode.removeChangeListener (this);
            slider.setLookAndFeel (nullptr);
        }

        void resized() override
        {
            const int dia = diameter();
            slider.setBounds ((getWidth() - dia) / 2, kTopPad, dia, dia);
            assignOverlay.setBounds (slider.getBounds());
        }

        void paint (juce::Graphics& g) override
        {
            const int dia = diameter();
            const int labelY = kTopPad + dia + kGapAfterKnob;
            const int valueY = labelY + kLabelH + kGapAfterLabel;

            g.setColour (col::InkMid);
            g.setFont (font::mono (9.0f, 0.08f));
            g.drawText (label, 0, labelY, getWidth(), kLabelH,
                        juce::Justification::centred, false);

            g.setColour (col::InkLo);
            g.setFont (font::mono (9.0f, 0.0f));
            const auto v = (float) slider.getValue();
            const auto vText = fmt ? fmt (v) : juce::String (v, 2);
            g.drawText (vText, 0, valueY, getWidth(), kValueH,
                        juce::Justification::centred, false);
        }

        int cellWidth()  const { return diameter() + 12; }
        int cellHeight() const { return kTopPad + diameter() + kGapAfterKnob
                                      + kLabelH + kGapAfterLabel + kValueH; }

        void setSelecting (bool s) { slider.getProperties().set ("selecting", s); slider.repaint(); }
        void setModDest (mod::Dest d) { modDest = d; refreshPatchModeVisual(); }
        mod::Dest getModDest() const { return modDest; }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            refreshPatchModeVisual();
        }

        bool isAssignmentTarget() const
        {
            return modDest != mod::Dest::Count && proc.patchMode.isArmed();
        }

        void refreshPatchModeVisual()
        {
            const bool armed = isAssignmentTarget();
            slider.getProperties().set ("selecting", armed);
            slider.repaint();
            // The overlay must sit on top of the slider in z-order to intercept
            // clicks reliably — toBack on the slider, then show the overlay.
            assignOverlay.setVisible (armed);
            if (armed) assignOverlay.toFront (false);
        }

        int diameter() const
        {
            switch (sz)
            {
                case KnobSize::S:  return dim::kKnobS;
                case KnobSize::M:  return dim::kKnobM;
                case KnobSize::L:  return dim::kKnobL;
                case KnobSize::XL: return dim::kKnobXL;
            }
            return dim::kKnobM;
        }

        static constexpr int kTopPad         = 4;
        static constexpr int kGapAfterKnob   = 4;
        static constexpr int kLabelH         = 12;
        static constexpr int kGapAfterLabel  = 2;
        static constexpr int kValueH         = 12;

        void timerCallback() override
        {
            // Saved-tick from the baseline.
            if (saved != params::P::NumParams)
            {
                const int g = proc.preset().focused.load();
                const float sv = proc.baseline().group[(size_t) g].get (saved);
                const double existing = (double) slider.getProperties().getWithDefault ("saved", -999.0);
                if (! juce::approximatelyEqual (existing, (double) sv))
                {
                    slider.getProperties().set ("saved", (double) sv);
                    slider.repaint();
                }
            }
            updateModRibbons();
        }

        // Scan the focused group's mod-matrix for connections targeting this
        // knob's destination and publish a compact summary on the slider's
        // properties so KnobLnF can draw a ribbon arc per route, polarity-
        // coloured, length = ±effective depth, stacked outside the ring.
        void updateModRibbons()
        {
            if (modDest == mod::Dest::Count) return;
            auto& patch = proc.patch();
            const auto& snap = proc.snapshot();
            const int n = patch.numConnections.load();
            constexpr int kMax = 6;
            int count = 0;
            for (int i = 0; i < n && count < kMax; ++i)
            {
                const auto& c = patch.connections[(size_t) i];
                if (c.dest != modDest || ! c.enabled || c.muted) continue;
                const float macro = patch.sourceMacro[(size_t) c.source].load();
                const float eff   = c.depth * macro;
                if (std::abs (eff) < 0.001f) continue;
                const int   srcIdx = (int) c.source;
                const float live   = (srcIdx >= 0
                                      && srcIdx < (int) snap.liveSources.size())
                                     ? snap.liveSources[(size_t) srcIdx].load()
                                     : 0.0f;
                slider.getProperties().set ("mod_" + juce::String (count) + "_src",   srcIdx);
                slider.getProperties().set ("mod_" + juce::String (count) + "_depth", (double) eff);
                slider.getProperties().set ("mod_" + juce::String (count) + "_live",  (double) live);
                ++count;
            }
            const int prev = (int) slider.getProperties().getWithDefault ("modCount", 0);
            slider.getProperties().set ("modCount", count);
            // Always repaint when we have mods so the animated dot keeps
            // moving even if the depth & macro haven't changed.
            if (count != prev || count > 0) slider.repaint();
        }

        TidesAudioProcessor& proc;
        KnobSize sz;
        juce::String label;
        std::function<juce::String (float)> fmt;
        params::P saved;
        mod::Dest modDest;

        KnobLnF lnf;
        juce::Slider slider;
        AssignOverlay assignOverlay;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
}
