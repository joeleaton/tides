#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "ui/Theme.h"
#include "PluginProcessor.h"
#include "vis/EngineSnapshot.h"

namespace tides::ui
{
    // Row 4 — full-width visualisation strip (128 px tall):
    //   SCOPE (left)  ·  VOICES (centre)  ·  METERS (right)
    //
    // All three blocks read from proc.snapshot() at ~30 fps with light
    // analogue-style peak hold + decay on the meters.
    class VizStrip : public juce::Component, private juce::Timer
    {
    public:
        explicit VizStrip (TidesAudioProcessor& procRef)
            : scopeBlock (procRef),
              voicesBlock (procRef),
              metersBlock (procRef)
        {
            addAndMakeVisible (scopeBlock);
            addAndMakeVisible (voicesBlock);
            addAndMakeVisible (metersBlock);
            startTimerHz (30);
        }

        void paint (juce::Graphics& g) override
        {
            // Grout fill between blocks
            g.fillAll (col::DividerGrout);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto scope = r.removeFromLeft (dim::kScopeW);
            r.removeFromLeft (dim::kGrout);
            auto meters = r.removeFromRight (dim::kMetersW);
            r.removeFromRight (dim::kGrout);
            scopeBlock .setBounds (scope);
            metersBlock.setBounds (meters);
            voicesBlock.setBounds (r);
        }

    private:
        void timerCallback() override
        {
            scopeBlock .repaint();
            voicesBlock.repaint();
            metersBlock.tick();
        }

        // ---- block: scope -------------------------------------------------
        class ScopeBlock : public juce::Component
        {
        public:
            explicit ScopeBlock (TidesAudioProcessor& p) : proc (p) {}

            void paint (juce::Graphics& g) override
            {
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRect (rf);

                // Section head
                paintHead (g, juce::String::fromUTF8 ("SCOPE  ·  master out"),
                              masterPeakDb());

                auto scopeR = getLocalBounds().withTrimmedTop (kHeadH).reduced (8, 4);

                // Centre + grid lines
                g.setColour (col::Divider2.withAlpha (0.08f));
                const float cy = (float) scopeR.getCentreY();
                const float dashes[] = { 2.0f, 4.0f };
                juce::Line<float> centre { (float) scopeR.getX(), cy,
                                            (float) scopeR.getRight(), cy };
                g.drawDashedLine (centre, dashes, 2, 0.5f);

                g.setColour (col::Divider2.withAlpha (0.05f));
                for (int i = 1; i < 4; ++i)
                {
                    const float y = scopeR.getY() + scopeR.getHeight() * i / 4.0f;
                    g.drawLine ((float) scopeR.getX(), y,
                                (float) scopeR.getRight(), y, 0.5f);
                }

                // Waveform
                constexpr int kSamples = 512;
                float buf[kSamples];
                const int n = proc.snapshot().readLatest (buf, kSamples);
                if (n <= 1) return;

                const float w = (float) scopeR.getWidth();
                const float h = (float) scopeR.getHeight();
                const float half = h * 0.5f - 4.0f;

                juce::Path p;
                for (int i = 0; i < n; ++i)
                {
                    const float x = (float) scopeR.getX() + (float) i / (float) (n - 1) * w;
                    const float y = cy - juce::jlimit (-1.0f, 1.0f, buf[i]) * half;
                    if (i == 0) p.startNewSubPath (x, y);
                    else        p.lineTo            (x, y);
                }
                // Glow
                g.setColour (col::Amber.withAlpha (0.28f));
                g.strokePath (p, juce::PathStrokeType (3.0f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));
                // Wave
                g.setColour (col::Amber);
                g.strokePath (p, juce::PathStrokeType (1.2f,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded));
            }

        private:
            juce::String masterPeakDb() const
            {
                const float p = proc.snapshot().masterPeak.load();
                if (p > 0.95f) return "CLIP";
                if (p < 0.001f) return juce::String::fromUTF8 ("−∞ dB");
                const float db = 20.0f * std::log10 (p);
                return juce::String (db, 1) + " dB";
            }

            void paintHead (juce::Graphics& g, const juce::String& label,
                            const juce::String& value)
            {
                auto head = getLocalBounds().removeFromTop (kHeadH).reduced (12, 6);
                g.setColour (col::InkLo);
                g.setFont (font::mono (8.5f, 0.14f));
                g.drawText (label, head, juce::Justification::centredLeft, false);
                g.setFont (font::mono (8.5f, 0.06f));
                g.drawText (value, head, juce::Justification::centredRight, false);
            }

            static constexpr int kHeadH = 22;
            TidesAudioProcessor& proc;
        };

        // ---- block: voices ------------------------------------------------
        class VoicesBlock : public juce::Component
        {
        public:
            explicit VoicesBlock (TidesAudioProcessor& p) : proc (p) {}

            void paint (juce::Graphics& g) override
            {
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRect (rf);

                // Section head
                const auto& snap = proc.snapshot();
                int active = 0, ringing = 0, selfOsc = 0;
                for (size_t i = 0; i < snap.voices.size(); ++i)
                {
                    const auto state = snap.voices[i].state.load();
                    if (state != vis::EngineSnapshot::Idle) ++active;
                    if (state == vis::EngineSnapshot::Ringing) ++ringing;
                    if (snap.voices[i].selfOsc.load()) ++selfOsc;
                }
                const juce::String headL = juce::String::fromUTF8 ("VOICES  ·  ")
                                            + juce::String (active) + "/16";
                const juce::String headR = (selfOsc > 0)
                    ? juce::String (selfOsc) + " self-osc"
                    : juce::String (ringing) + " ringing";
                paintHead (g, headL, headR);

                // Single row of 16 voice cells, vertically centered in the
                // block. Cells are tall + narrow chips.
                auto body = getLocalBounds().withTrimmedTop (22).reduced (8, 6);
                const int cells = 16;
                const int gap = 3;
                const int cellW = (body.getWidth() - (cells - 1) * gap) / cells;
                const int cellH = body.getHeight();

                const int focused = proc.preset().focused.load();

                for (int i = 0; i < cells; ++i)
                {
                    auto cell = juce::Rectangle<int> (
                        body.getX() + i * (cellW + gap),
                        body.getY(),
                        cellW, cellH);

                    const auto& v = snap.voices[(size_t) i];
                    const int  state = (int) v.state.load();
                    const int  group = v.group.load();
                    const float energy = v.energy.load();
                    const bool  selfOscV = v.selfOsc.load();
                    const int  note   = v.note.load();

                    const bool idle = (state == vis::EngineSnapshot::Idle);
                    const bool isFocused = (group == focused);
                    const auto hue = (group >= 0 && group < 6)
                        ? col::GroupHues[(size_t) group]
                        : col::Amber;

                    // Cell background
                    g.setColour (idle ? col::PanelLo.darker (0.4f)
                                       : hue.withAlpha (0.15f));
                    g.fillRoundedRectangle (cell.toFloat(), 3.0f);

                    // Energy fill (bottom-up)
                    if (! idle && energy > 0.01f)
                    {
                        const auto fillH = juce::jlimit (0.0f, 1.0f, energy) * (float) cell.getHeight();
                        auto fill = cell.toFloat().withTrimmedTop (cell.getHeight() - fillH);
                        g.setColour (hue.withAlpha (selfOscV ? 0.95f : 0.55f));
                        g.fillRoundedRectangle (fill, 3.0f);
                    }

                    // Outline + focused emphasis
                    g.setColour (isFocused && ! idle ? hue
                                                      : col::DividerGrout);
                    g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 3.0f,
                                            isFocused && ! idle ? 1.5f : 1.0f);

                    // Index + note text
                    g.setFont (font::mono (7.5f, 0.08f, true));
                    g.setColour (idle ? col::InkFaint : col::Ink);
                    g.drawText (juce::String (i + 1),
                                cell.reduced (3, 1).removeFromTop (10),
                                juce::Justification::topLeft, false);

                    g.setFont (font::mono (8.0f, 0.0f));
                    g.setColour (idle ? col::InkFaint : (selfOscV ? col::PurpleHot : col::Ink));
                    // Only show a note for active voices — idle voices may
                    // still have a stale `note` value left over from the last
                    // gate they played.
                    g.drawText ((! idle && note >= 0)
                                    ? noteName (note)
                                    : juce::String::fromUTF8 ("·"),
                                cell.reduced (3, 1).removeFromBottom (11),
                                juce::Justification::centred, false);
                }
            }

        private:
            static juce::String noteName (int n)
            {
                static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
                if (n < 0) return {};
                const int octave = n / 12 - 1;
                return juce::String (names[n % 12]) + juce::String (octave);
            }

            void paintHead (juce::Graphics& g, const juce::String& label,
                            const juce::String& value)
            {
                auto head = getLocalBounds().removeFromTop (22).reduced (12, 6);
                g.setColour (col::InkLo);
                g.setFont (font::mono (8.5f, 0.14f));
                g.drawText (label, head, juce::Justification::centredLeft, false);
                g.setFont (font::mono (8.5f, 0.06f));
                g.drawText (value, head, juce::Justification::centredRight, false);
            }

            TidesAudioProcessor& proc;
        };

        // ---- block: meters ------------------------------------------------
        class MetersBlock : public juce::Component
        {
        public:
            explicit MetersBlock (TidesAudioProcessor& p) : proc (p) {}

            // Called from the parent timer at 30 fps — drives peak-hold decay.
            void tick()
            {
                const auto& s = proc.snapshot();
                const float master = s.masterPeak.load();
                const float ext    = s.extInPeak.load();

                // Same peak feeds L and R for now (mono master); will split
                // when the engine starts publishing stereo peaks.
                lvl[0] = std::max (master, lvl[0] * 0.85f);
                lvl[1] = std::max (master, lvl[1] * 0.85f);
                lvl[2] = std::max (ext,    lvl[2] * 0.85f);
                for (int i = 0; i < 3; ++i)
                    hold[i] = (lvl[i] > hold[i]) ? lvl[i] : juce::jmax (lvl[i], hold[i] * 0.98f);
                repaint();
            }

            void paint (juce::Graphics& g) override
            {
                auto rf = getLocalBounds().toFloat();
                g.setColour (col::PanelLo);
                g.fillRect (rf);

                paintHead (g, "METERS", "dB");

                auto body = getLocalBounds().withTrimmedTop (22).reduced (8, 6);

                // dB ruler on the left
                const char* ticks[] = { "0", "−6", "−12", "−24", "−60" };
                const float tickPos[] = { 0.0f, 0.30f, 0.50f, 0.66f, 0.95f };  // dB top→bottom (normalized 0..1)
                auto ruler = body.removeFromLeft (24);
                g.setColour (col::InkLo);
                g.setFont (font::mono (7.0f, 0.0f));
                for (int i = 0; i < 5; ++i)
                {
                    const int y = ruler.getY() + (int) (tickPos[i] * ruler.getHeight()) - 4;
                    g.drawText (juce::String::fromUTF8 (ticks[i]),
                                ruler.getX(), y, ruler.getWidth(), 8,
                                juce::Justification::centredRight, false);
                }

                body.removeFromLeft (6);

                // Three meter columns: L · R · (divider) · EXT — wide enough
                // to fill the meters block out to its right edge so the bars
                // read at-a-glance rather than as thin slivers.
                // Narrower bars w/ generous gaps so the meters read like the
                // design reference, not as solid chunks.
                const int colW    = 20;
                const int colGap  = 18;
                const int divGap  = 22;
                // ruler 24 + gap 6 + 20 + 18 + 20 + 22 + 1 + 22 + 20 = 153 ≈ inner ~204

                drawMeter (g, body.removeFromLeft (colW), "L",   lvl[0], hold[0], false);
                body.removeFromLeft (colGap);
                drawMeter (g, body.removeFromLeft (colW), "R",   lvl[1], hold[1], false);
                body.removeFromLeft (divGap);
                g.setColour (col::Divider2.withAlpha (0.4f));
                g.fillRect (body.getX(), body.getY() + 4, 1, body.getHeight() - 14);
                body.removeFromLeft (divGap);
                drawMeter (g, body.removeFromLeft (colW), "EXT", lvl[2], hold[2], true);
            }

        private:
            void drawMeter (juce::Graphics& g, juce::Rectangle<int> r,
                            const juce::String& label, float v, float peakHold,
                            bool dim)
            {
                const float alpha = dim ? 0.55f : 1.0f;

                // Reserve a label below the bar
                auto bar = r.withTrimmedBottom (12);

                g.setColour (col::PanelLo.darker (0.5f));
                g.fillRoundedRectangle (bar.toFloat(), 2.0f);
                g.setColour (col::Divider2.withAlpha (0.18f));
                g.drawRoundedRectangle (bar.toFloat().reduced (0.5f), 2.0f, 1.0f);

                // Fill from bottom up
                const float vClamp = juce::jlimit (0.0f, 1.0f, v);
                const int fillH = (int) std::round (vClamp * bar.getHeight());
                auto fill = juce::Rectangle<int> (bar.getX(), bar.getBottom() - fillH,
                                                   bar.getWidth(), fillH);

                // Gradient: amber up to ~-6 dB, red at the top
                if (fillH > 0)
                {
                    juce::ColourGradient grad (
                        col::Amber.withAlpha (0.85f * alpha),
                        (float) fill.getX(), (float) fill.getBottom(),
                        (vClamp > 0.7f ? col::Pos : col::Amber).withAlpha (0.95f * alpha),
                        (float) fill.getX(), (float) fill.getY(), false);
                    g.setGradientFill (grad);
                    g.fillRect (fill);
                }

                // Peak hold marker
                const float ph = juce::jlimit (0.0f, 1.0f, peakHold);
                const int phY = bar.getBottom() - (int) std::round (ph * bar.getHeight());
                g.setColour ((ph > 0.95f ? col::Pos : col::AmberHot).withAlpha (alpha));
                g.fillRect (bar.getX(), phY - 1, bar.getWidth(), 2);

                // Reference lines: 0 dB at ~top, -6 dB at ~30 % from top
                g.setColour (col::Pos.withAlpha (0.40f * alpha));
                g.drawLine ((float) bar.getX(), (float) bar.getY(),
                            (float) bar.getRight(), (float) bar.getY(), 0.5f);

                // Label
                g.setColour (col::InkLo.withAlpha (alpha));
                g.setFont (font::mono (8.0f, 0.10f));
                g.drawText (label,
                            r.withY (bar.getBottom()).withHeight (12),
                            juce::Justification::centred, false);
            }

            void paintHead (juce::Graphics& g, const juce::String& label,
                            const juce::String& value)
            {
                auto head = getLocalBounds().removeFromTop (22).reduced (12, 6);
                g.setColour (col::InkLo);
                g.setFont (font::mono (8.5f, 0.14f));
                g.drawText (label, head, juce::Justification::centredLeft, false);
                g.setFont (font::mono (8.5f, 0.06f));
                g.drawText (value, head, juce::Justification::centredRight, false);
            }

            TidesAudioProcessor& proc;
            std::array<float, 3> lvl  { 0, 0, 0 };
            std::array<float, 3> hold { 0, 0, 0 };
        };

        ScopeBlock   scopeBlock;
        VoicesBlock  voicesBlock;
        MetersBlock  metersBlock;
    };
}
