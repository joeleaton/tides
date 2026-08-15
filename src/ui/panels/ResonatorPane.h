#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"
#include "ui/Pane.h"
#include "ui/Knob.h"
#include "ui/Primitives.h"
#include "ui/Format.h"
#include "PluginProcessor.h"
#include "vis/EngineSnapshot.h"

namespace tides::ui::panels
{
    // 03 BBD RESONATOR pane.
    // Header right:  Invert · KF VCO · NOTE stepper (-12..+12)
    // Hero box:      flow lines + IN FILTER (L) · FEEDBACK (XL) · FB FILTER (L)
    // Bottom row:    FINE TUNE (M) · GLIDE (M) · SELF-OSC indicator tile
    class ResonatorPane : public Pane, private juce::Timer
    {
    public:
        explicit ResonatorPane (TidesAudioProcessor& p)
            : Pane ("03", "BBD RESONATOR"),
              proc (p),
              hero (p),
              knobInFilter (p, KnobSize::L,  "IN FILTER", "res_in_filter",  false, [] (float v) { return fmt::hz (v); }, params::P::ResInputFilter,    mod::Dest::ResInputFilter),
              knobFeedback (p, KnobSize::XL, "FEEDBACK",  "res_feedback",   false, fmt::feedback,                         params::P::ResFeedback,       mod::Dest::ResFeedback),
              knobFBFilter (p, KnobSize::L,  "FB FILTER", "res_fb_filter",  false, [] (float v) { return fmt::hz (v); }, params::P::ResFeedbackFilter, mod::Dest::ResFeedbackFilter),
              knobFineTune (p, KnobSize::M,  "FINE TUNE", "res_fine_tune",  true,  [] (float v) { return fmt::semi (v, 3.0f); }, params::P::ResFineTune, mod::Dest::ResFineTune),
              knobGlide    (p, KnobSize::M,  "GLIDE",     "res_glide",      false, [] (float v) { return fmt::ms (v, 1000.0f); }, params::P::ResGlide,    mod::Dest::ResGlide),
              swInvert ("Invert"),
              swKfVco  ("KF VCO"),
              stepNote (-12, 12)
        {
            // ---- hero box (knob children) ----
            addChildComponent (hero);   // visible; just add and make visible
            addAndMakeVisible (hero);
            hero.addAndMakeVisible (knobInFilter);
            hero.addAndMakeVisible (knobFeedback);
            hero.addAndMakeVisible (knobFBFilter);

            // ---- below-hero knobs ----
            addAndMakeVisible (knobFineTune);
            addAndMakeVisible (knobGlide);

            // ---- header switches/stepper ----
            swInvert.setValue (proc.patch().resInvert.load(), false);
            swInvert.onChange = [this] (bool v) { proc.patch().resInvert.store (v); };
            addAndMakeVisible (swInvert);

            swKfVco.setValue (proc.patch().keyFollowVco.load(), false);
            swKfVco.onChange = [this] (bool v) { proc.patch().keyFollowVco.store (v); };
            addAndMakeVisible (swKfVco);

            stepNote.setValue (proc.patch().resNoteOffset.load(), false);
            stepNote.setFormatter (fmt::intSigned);
            stepNote.onChange = [this] (int v) { proc.patch().resNoteOffset.store (v); };
            addAndMakeVisible (stepNote);

            startTimerHz (15);
        }

        void resized() override
        {
            // ---- header right: Invert · KF VCO · NOTE label + Stepper ----
            auto hr = headerRightBounds();
            const int swW = 78, swH = 22, gap = 6;
            const int stW = 84;
            const int noteLblW = 32;
            int x = getWidth() - dim::kPanePadSide - stW;
            stepNote.setBounds (x, hr.getY(), stW, swH);
            x -= noteLblW;
            noteLabelR = juce::Rectangle<int> (x, hr.getY(), noteLblW, swH);
            x -= swW + gap;
            swKfVco.setBounds (x, hr.getY(), swW, swH);
            x -= swW + gap;
            swInvert.setBounds (x, hr.getY(), swW, swH);

            auto body = bodyBounds();

            // ---- hero box at top ----
            const int heroH = 156;
            auto heroR = body.removeFromTop (heroH);
            hero.setBounds (heroR);

            // hero internal layout
            {
                auto hb = juce::Rectangle<int> (0, 0, heroR.getWidth(), heroR.getHeight())
                              .reduced (12, 10);
                const int n = 3;
                const int cellW = hb.getWidth() / n;
                Knob* knobs[n] = { &knobInFilter, &knobFeedback, &knobFBFilter };
                for (int i = 0; i < n; ++i)
                {
                    auto cell = juce::Rectangle<int> (hb.getX() + i * cellW, hb.getY(),
                                                       cellW, hb.getHeight());
                    auto cb = cell.withSizeKeepingCentre (knobs[i]->cellWidth(),
                                                           knobs[i]->cellHeight());
                    knobs[i]->setBounds (cb);
                }
            }

            body.removeFromTop (10);

            // ---- bottom row: FINE TUNE · GLIDE · SELF-OSC tile ----
            {
                const int n = 3;
                const int cellW = body.getWidth() / n;
                // FINE TUNE
                auto c0 = juce::Rectangle<int> (body.getX(), body.getY(), cellW, body.getHeight());
                knobFineTune.setBounds (c0.withSizeKeepingCentre (knobFineTune.cellWidth(),
                                                                    knobFineTune.cellHeight()));
                // GLIDE
                auto c1 = juce::Rectangle<int> (body.getX() + cellW, body.getY(), cellW, body.getHeight());
                knobGlide.setBounds (c1.withSizeKeepingCentre (knobGlide.cellWidth(),
                                                                 knobGlide.cellHeight()));
                // SELF-OSC tile bounds saved for paint()
                selfOscR = juce::Rectangle<int> (body.getX() + 2 * cellW, body.getY(),
                                                  cellW, body.getHeight());
            }
        }

        void paint (juce::Graphics& g) override
        {
            Pane::paint (g);

            // NOTE label next to stepper
            g.setColour (col::InkLo);
            g.setFont (font::mono (8.5f, 0.16f));
            g.drawText ("NOTE", noteLabelR, juce::Justification::centredRight, false);

            // SELF-OSC indicator tile
            if (! selfOscR.isEmpty())
            {
                const bool ignited = anyFocusedVoiceSelfOsc();

                // Tile inset
                auto tile = selfOscR.withSizeKeepingCentre (56, 56);
                auto rf = tile.toFloat();
                g.setColour (col::PanelLo);
                g.fillRoundedRectangle (rf, 6.0f);
                g.setColour (col::DividerGrout);
                g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (col::EdgeHi);
                g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

                // Glow + dot
                const float cx = rf.getCentreX();
                const float cy = rf.getCentreY();
                if (ignited)
                {
                    g.setColour (col::Purple.withAlpha (0.45f));
                    g.fillEllipse (cx - 12.0f, cy - 12.0f, 24.0f, 24.0f);
                    g.setColour (col::PurpleHot);
                }
                else
                {
                    g.setColour (col::InkFaint);
                }
                g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

                // Label + value (mimic .knob-cell)
                auto lbl = juce::Rectangle<int> (selfOscR.getX(), tile.getBottom() + 2,
                                                  selfOscR.getWidth(), 12);
                auto val = lbl.translated (0, 14);
                g.setColour (col::InkMid);
                g.setFont (font::mono (9.0f, 0.08f));
                g.drawText ("SELF-OSC", lbl, juce::Justification::centred, false);
                g.setColour (col::InkLo);
                g.setFont (font::mono (9.0f, 0.0f));
                g.drawText (ignited ? "IGNITED" : "stable", val, juce::Justification::centred, false);
            }
        }

    private:
        void timerCallback() override
        {
            swInvert.setValue (proc.patch().resInvert.load(), false);
            swKfVco .setValue (proc.patch().keyFollowVco.load(), false);
            stepNote.setValue (proc.patch().resNoteOffset.load(), false);
            repaint();   // SELF-OSC indicator + flow accent track feedback knob
        }

        bool anyFocusedVoiceSelfOsc() const
        {
            const int focusedIdx = proc.preset().focused.load();
            const auto& snap = proc.snapshot();
            for (size_t i = 0; i < snap.voices.size(); ++i)
                if (snap.voices[i].selfOsc.load()
                 && snap.voices[i].group.load() == focusedIdx
                 && snap.voices[i].state.load() != vis::EngineSnapshot::Idle)
                    return true;
            return false;
        }

        // ---- inner: hero inset with flow lines drawn beneath knobs ----
        class HeroBox : public juce::Component, private juce::Timer
        {
        public:
            explicit HeroBox (TidesAudioProcessor& p) : proc (p)
            {
                setInterceptsMouseClicks (false, true);
                startTimerHz (12);
            }

            void paint (juce::Graphics& g) override
            {
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRoundedRectangle (rf, 6.0f);
                g.setColour (col::Divider2);
                g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
                g.setColour (col::EdgeHi);
                g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

                // Flow lines (subtle, behind knobs)
                const float fb = (float) proc.patch().get (params::P::ResFeedback);
                const auto baseAlpha = juce::jlimit (0.10f, 0.55f, 0.10f + fb * 0.55f);

                const float cy = rf.getCentreY() + 6.0f;
                const float x1 = rf.getX() + 8.0f;
                const float x2 = rf.getRight() - 8.0f;

                // Horizontal signal line
                g.setColour (col::Amber.withAlpha (0.22f));
                g.drawLine (x1, cy, x2, cy, 0.6f);

                // Feedback loop arc (deepens with fb)
                juce::Path arc;
                const float midX = rf.getCentreX();
                const float rightThird = midX + (x2 - midX) * 0.55f;
                arc.startNewSubPath (midX, cy);
                arc.cubicTo (midX, rf.getBottom() - 4.0f,
                             rightThird, rf.getBottom() - 4.0f,
                             rightThird, cy);
                g.setColour (col::Amber.withAlpha (baseAlpha));
                juce::PathStrokeType pst (0.8f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded);
                if (fb < 0.05f)
                {
                    const float dashes[] = { 2.5f, 3.5f };
                    juce::Path dashed;
                    pst.createDashedStroke (dashed, arc, dashes, 2);
                    g.fillPath (dashed);
                }
                else
                {
                    g.strokePath (arc, pst);
                }
            }

        private:
            void timerCallback() override { repaint(); }
            TidesAudioProcessor& proc;
        };

        TidesAudioProcessor& proc;
        HeroBox hero;
        Knob knobInFilter, knobFeedback, knobFBFilter;
        Knob knobFineTune, knobGlide;
        Switch swInvert, swKfVco;
        Stepper stepNote;
        juce::Rectangle<int> noteLabelR;
        juce::Rectangle<int> selfOscR;
    };
}
