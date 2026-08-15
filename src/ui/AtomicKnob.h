#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include "ui/Theme.h"
#include "ui/KnobLnF.h"
#include "PluginProcessor.h"
#include "mod/ModDefs.h"

namespace tides::ui
{
    enum class AtomicKnobSize { S = 0, M = 1, L = 2, XL = 3 };

    // Knob whose value lives in an std::atomic<float> (not APVTS). Used for
    // LFO params (per-LFO atomics on PatchState). Also patch-mode-aware: an
    // optional mod::Dest marks the knob as a modulation target so it joins
    // the "select any knob to assign" flow.
    class AtomicKnob : public juce::Component,
                       private juce::Timer,
                       private juce::ChangeListener
    {
    private:
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
        AtomicKnob (TidesAudioProcessor& procRef,
                    std::atomic<float>* target,
                    AtomicKnobSize size,
                    const juce::String& labelText,
                    float lo = 0.0f,
                    float hi = 1.0f,
                    bool  bipolar = false,
                    std::function<juce::String (float)> formatter = {},
                    bool  invertValue = false,
                    mod::Dest dest = mod::Dest::Count)
            : proc (procRef),
              ptr (target),
              sz (size),
              label (labelText),
              minV (lo),
              maxV (hi),
              inverted (invertValue),
              modDest (dest),
              fmt (std::move (formatter))
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (juce::degreesToRadians (-210.0f + 360.0f),
                                        juce::degreesToRadians (  30.0f + 360.0f),
                                        true);
            slider.setRange (minV, maxV, 0.0);
            slider.setValue (ptr ? viewFromAtomic (ptr->load()) : 0.0, juce::dontSendNotification);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            slider.setVelocityBasedMode (false);
            slider.setMouseDragSensitivity (200);
            slider.setLookAndFeel (&lnf);
            slider.getProperties().set ("bipolar", bipolar);
            slider.onValueChange = [this]
            {
                if (ptr) ptr->store (atomicFromView ((float) slider.getValue()));
                repaint();
            };
            addAndMakeVisible (slider);

            assignOverlay.onClick = [this] { proc.assignConnection (modDest); };
            addChildComponent (assignOverlay);

            proc.patchMode.addChangeListener (this);
            refreshPatchModeVisual();

            startTimerHz (30);
        }

        ~AtomicKnob() override
        {
            proc.patchMode.removeChangeListener (this);
            slider.setLookAndFeel (nullptr);
        }

        // Retarget at runtime (e.g. when the active LFO tab changes).
        void setTarget (std::atomic<float>* newTarget)
        {
            ptr = newTarget;
            if (ptr) slider.setValue (viewFromAtomic (ptr->load()), juce::dontSendNotification);
            repaint();
        }

        void setModDest (mod::Dest d) { modDest = d; refreshPatchModeVisual(); }
        mod::Dest getModDest() const { return modDest; }

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
            assignOverlay.setVisible (armed);
            if (armed) assignOverlay.toFront (false);
        }

        int diameter() const
        {
            switch (sz)
            {
                case AtomicKnobSize::S:  return dim::kKnobS;
                case AtomicKnobSize::M:  return dim::kKnobM;
                case AtomicKnobSize::L:  return dim::kKnobL;
                case AtomicKnobSize::XL: return dim::kKnobXL;
            }
            return dim::kKnobS;
        }

        static constexpr int kTopPad         = 4;
        static constexpr int kGapAfterKnob   = 4;
        static constexpr int kLabelH         = 12;
        static constexpr int kGapAfterLabel  = 2;
        static constexpr int kValueH         = 12;

        void timerCallback() override
        {
            if (ptr)
            {
                const float view = viewFromAtomic (ptr->load());
                if (! juce::approximatelyEqual ((float) slider.getValue(), view))
                    slider.setValue (view, juce::dontSendNotification);
            }
            updateModRibbons();
        }

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
            if (count != prev || count > 0) slider.repaint();
        }

        float viewFromAtomic  (float a) const { return inverted ? (maxV + minV - a) : a; }
        float atomicFromView  (float v) const { return inverted ? (maxV + minV - v) : v; }

        TidesAudioProcessor& proc;
        std::atomic<float>* ptr;
        AtomicKnobSize sz;
        juce::String label;
        float minV, maxV;
        bool  inverted { false };
        mod::Dest modDest;
        std::function<juce::String (float)> fmt;

        KnobLnF lnf;
        juce::Slider slider;
        AssignOverlay assignOverlay;
    };
}
