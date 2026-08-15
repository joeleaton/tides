#pragma once
#include <JuceHeader.h>
#include <memory>
#include "ui/Theme.h"
#include "ui/Primitives.h"
#include "ui/Format.h"
#include "PluginProcessor.h"

namespace tides::ui
{
    // Row 5 — performance routing. Left half = the focused group's routing
    // (MIDI CH · POLY · PRIO · UNI · DET · SPR), right half = preset-wide
    // (CH · Preset Uni · PRIO · MPE stub). Total height kPerfRowH = 64.
    class PerfRow : public juce::Component, private juce::Timer
    {
    public:
        explicit PerfRow (TidesAudioProcessor& procRef)
            : proc (procRef),
              stMidiCh (1, 16),
              stPoly   (0, 16),
              stUni    (1, 6),
              stPresetCh (1, 16),
              swPresetUni ("Preset Uni"),
              swMpe ("MPE")
        {
            // ----- per-group routing fields -----
            // Route through the processor so the choice is persisted to the
            // global settings file (across sessions / plugin instances).
            stMidiCh.onChange = [this] (int v) {
                proc.setGroupMidiChannel (proc.preset().focused.load(), v);
            };
            addAndMakeVisible (stMidiCh);

            stPoly.setFormatter ([] (int v) {
                if (v == 0) return juce::String ("OFF");
                if (v == 1) return juce::String ("MONO");
                return juce::String (v);
            });
            stPoly.onChange = [this] (int v) {
                proc.preset().groupPoly[(size_t) proc.preset().focused.load()].store (v);
            };
            addAndMakeVisible (stPoly);

            populatePriorityCombo (priGroup);
            priGroup.onChange = [this] {
                proc.preset().groupPriority[(size_t) proc.preset().focused.load()]
                    .store (priGroup.getSelectedItemIndex());
            };
            addAndMakeVisible (priGroup);

            stUni.onChange = [this] (int v) {
                proc.patch().unisonVoices.store (v);
            };
            addAndMakeVisible (stUni);

            msDetune.setFormatter (fmt::pct);
            msDetune.onChange = [this] (float v) { proc.patch().unisonDetune.store (v); };
            addAndMakeVisible (msDetune);

            msSpread.setFormatter (fmt::pct);
            msSpread.onChange = [this] (float v) { proc.patch().unisonSpread.store (v); };
            addAndMakeVisible (msSpread);

            // ----- preset master fields -----
            stPresetCh.onChange = [this] (int v) {
                proc.setPresetMidiChannel (v);
            };
            addAndMakeVisible (stPresetCh);

            swPresetUni.onChange = [this] (bool v) { proc.preset().presetUnison.store (v); };
            addAndMakeVisible (swPresetUni);

            populatePriorityCombo (priPreset);
            priPreset.onChange = [this] {
                proc.preset().presetPriority.store (priPreset.getSelectedItemIndex());
            };
            addAndMakeVisible (priPreset);

            swMpe.setValue (proc.preset().mpeMode.load(), false);
            swMpe.onChange = [this] (bool v) { proc.preset().mpeMode.store (v); };
            addAndMakeVisible (swMpe);

            // Initial sync + periodic refresh (focus switch / preset load).
            syncFromState();
            startTimerHz (10);
        }

        void resized() override
        {
            // 1-px hairline at the top is painted in paint().
            auto body = getLocalBounds().withTrimmedTop (1).reduced (0, 4);

            auto rightR = body.removeFromRight (kRightW).reduced (8, 0);
            // Vertical divider painted in paint(); keep a small gap
            body.removeFromRight (1);
            auto leftR = body.reduced (12, 0);

            layoutLeft (leftR);
            layoutRight (rightR);
        }

        void paint (juce::Graphics& g) override
        {
            // Background + top hairline
            g.setColour (col::Panel);
            g.fillAll();
            g.setColour (col::DividerGrout);
            g.fillRect (0, 0, getWidth(), 1);

            // Vertical divider between left and right halves
            const int dividerX = getWidth() - kRightW - 1;
            g.setColour (col::Divider2.withAlpha (0.4f));
            g.fillRect (dividerX, 6, 1, getHeight() - 12);

            // Captions
            paintCaption (g,
                          juce::Rectangle<int> (12, 4, 200, 12),
                          juce::String::fromUTF8 ("G·")
                             + juce::String (proc.preset().focused.load() + 1)
                             + " ROUTING");
            paintCaption (g,
                          juce::Rectangle<int> (dividerX + 12, 4, 200, 12),
                          "PRESET");

            // MPE toggle is a real Switch component now — painted by itself.
        }

    private:
        // Width of the right (preset) half. The vertical divider line and
        // the PRESET caption are positioned from this so everything stays
        // in lockstep.
        static constexpr int kRightW = 420;

        static void populatePriorityCombo (juce::ComboBox& cb)
        {
            // Order matches tides::state::Priority enum.
            cb.addItem ("1stAvail+Steal",   1);
            cb.addItem ("1stAvail NoSteal", 2);
            cb.addItem ("RR+Steal",         3);
            cb.addItem ("LRU",              4);
            cb.addItem ("Reuse",            5);
        }

        void paintCaption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& s)
        {
            g.setFont (font::mono (8.5f, 0.20f));
            g.setColour (col::InkLo);
            g.drawText (s, r, juce::Justification::topLeft, false);
        }

        // Per-field label drawn to the left of a control. Used by both halves.
        void paintFieldLabel (juce::Graphics&, const juce::String&, juce::Rectangle<int>) {}

        void layoutLeft (juce::Rectangle<int> r)
        {
            r.removeFromTop (16);   // caption row

            const int fieldH    = 22;
            const int gap       = 10;
            const int labelW    = 32;
            const int stepW     = 70;
            const int comboW    = 130;
            const int miniW     = 130;   // DET / SPR — extended

            auto field = [&] (juce::Component& c, int w) -> juce::Rectangle<int> {
                auto lab = r.removeFromLeft (labelW); (void) lab;
                auto ctl = r.removeFromLeft (w);
                c.setBounds (ctl.withSizeKeepingCentre (w, fieldH));
                r.removeFromLeft (gap);
                return lab;
            };

            leftLabelRects.clear();
            leftLabelRects.push_back ({ "MIDI CH", field (stMidiCh,  stepW) });
            leftLabelRects.push_back ({ "POLY",    field (stPoly,    stepW) });
            leftLabelRects.push_back ({ "PRIO",    field (priGroup,  comboW) });
            leftLabelRects.push_back ({ "UNI",     field (stUni,     stepW - 14) });
            leftLabelRects.push_back ({ "DET",     field (msDetune,  miniW) });
            leftLabelRects.push_back ({ "SPR",     field (msSpread,  miniW) });
        }

        void layoutRight (juce::Rectangle<int> r)
        {
            r.removeFromTop (16);

            const int fieldH = 22;
            const int gap    = 10;
            const int labelW = 24;
            const int stepW  = 70;
            const int switchW = 90;
            const int comboW = 110;
            const int mpeW   = 70;

            auto field = [&] (juce::Component& c, int w) -> juce::Rectangle<int> {
                auto lab = r.removeFromLeft (labelW);
                auto ctl = r.removeFromLeft (w);
                c.setBounds (ctl.withSizeKeepingCentre (w, fieldH));
                r.removeFromLeft (gap);
                return lab;
            };

            rightLabelRects.clear();
            rightLabelRects.push_back ({ "CH", field (stPresetCh, stepW) });

            // Switch has its own label baked in, so no separate label column.
            r.removeFromLeft (4);
            swPresetUni.setBounds (r.removeFromLeft (switchW)
                                      .withSizeKeepingCentre (switchW, fieldH));
            r.removeFromLeft (gap);

            rightLabelRects.push_back ({ "PRIO", field (priPreset, comboW) });

            // MPE toggle, aligned with the rest of the field row.
            swMpe.setBounds (r.removeFromLeft (mpeW)
                                .withSizeKeepingCentre (mpeW, fieldH));
        }

        void timerCallback() override { syncFromState(); }

        void syncFromState()
        {
            using namespace tides::state;
            const int g = proc.preset().focused.load();

            stMidiCh   .setValue (proc.preset().groupChannel[(size_t) g].load(),  false);
            stPoly     .setValue (proc.preset().groupPoly   [(size_t) g].load(),  false);
            priGroup   .setSelectedItemIndex (
                            juce::jlimit (0, kNumPriority - 1,
                                          proc.preset().groupPriority[(size_t) g].load()),
                            juce::dontSendNotification);

            stUni      .setValue (proc.patch().unisonVoices.load(), false);
            msDetune   .setValue (proc.patch().unisonDetune.load(), false);
            msSpread   .setValue (proc.patch().unisonSpread.load(), false);

            stPresetCh .setValue (proc.preset().presetChannel.load(),  false);
            swPresetUni.setValue (proc.preset().presetUnison.load(),   false);
            priPreset  .setSelectedItemIndex (
                            juce::jlimit (0, kNumPriority - 1,
                                          proc.preset().presetPriority.load()),
                            juce::dontSendNotification);
            swMpe.setValue (proc.preset().mpeMode.load(), false);

            // Caption pulls "G·N ROUTING" from focused — repaint when it shifts.
            if (g != lastFocused) { lastFocused = g; repaint(); }

            // Draw field labels via component repaint pass — we just trigger
            // a paint by invalidating the regions we draw text into.
            repaint (0, 16, getWidth(), 24);
        }

        // Field labels (drawn in paint via the cached label rects)
        struct LabelRect { juce::String label; juce::Rectangle<int> bounds; };
        std::vector<LabelRect> leftLabelRects, rightLabelRects;

    public:
        // Field-label drawing has to happen during paint, so override after
        // primary paint to overlay labels.
        void paintOverChildren (juce::Graphics& g) override
        {
            g.setFont (font::mono (8.5f, 0.12f));
            g.setColour (col::InkLo);
            for (auto& lr : leftLabelRects)
                g.drawText (lr.label, lr.bounds, juce::Justification::centredLeft, false);
            for (auto& lr : rightLabelRects)
                g.drawText (lr.label, lr.bounds, juce::Justification::centredLeft, false);
        }

    private:
        TidesAudioProcessor& proc;
        int lastFocused { -1 };

        Stepper  stMidiCh, stPoly, stUni;
        juce::ComboBox priGroup;

        MiniSlider msDetune;
        MiniSlider msSpread;

        Stepper stPresetCh;
        Switch  swPresetUni;
        juce::ComboBox priPreset;
        Switch  swMpe;
    };
}
