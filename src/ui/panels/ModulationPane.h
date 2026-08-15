#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "ui/Theme.h"
#include "ui/Pane.h"
#include "ui/MacroKnob.h"
#include "PluginProcessor.h"
#include "mod/ModDefs.h"

namespace tides::ui::panels
{
    // ---- Source / destination label tables -------------------------------
    namespace modlabels
    {
        inline const char* sourceName (int s)
        {
            switch ((mod::Source) s)
            {
                case mod::Source::Envelope:   return "ENV";
                case mod::Source::Lfo1:       return "LFO1";
                case mod::Source::Lfo2:       return "LFO2";
                case mod::Source::Lfo3:       return "LFO3";
                case mod::Source::Lfo4:       return "LFO4";
                case mod::Source::Modwheel:   return "MWHL";
                case mod::Source::Velocity:   return "VEL";
                case mod::Source::Aftertouch: return "AT";
                case mod::Source::KeyFollow:  return "KEY";
                default: return "?";
            }
        }

        // Compact destination labels — must fit in a 56-px tile column at
        // 8.5-pt mono next to a 3-char signed-depth readout ("+50").
        inline const char* destShort (mod::Dest d)
        {
            switch (d)
            {
                case mod::Dest::ExciterWave:        return "WAVE";
                case mod::Dest::ExciterNoiseSHRate: return "S&H";
                case mod::Dest::ExciterWaveNoiseMix:return "W/N";
                case mod::Dest::ExciterFrequency:   return "FREQ";
                case mod::Dest::ExciterLevel:       return "EXLV";
                case mod::Dest::EnvOnsetDelay:      return "ONS";
                case mod::Dest::EnvAttack:          return "ATK";
                case mod::Dest::EnvDecay:           return "DEC";
                case mod::Dest::ResInputFilter:     return "INFL";
                case mod::Dest::ResFeedbackFilter:  return "FBFL";
                case mod::Dest::ResFeedback:        return "FB";
                case mod::Dest::ResGlide:           return "GLD";
                case mod::Dest::ResFineTune:        return "TUNE";
                case mod::Dest::ResDryWet:          return "D/W";
                case mod::Dest::ResPanning:         return "PAN";
                case mod::Dest::ResLevel:           return "LVL";
                case mod::Dest::Lfo1Rate:           return "L1.R";
                case mod::Dest::Lfo1Mix:            return "L1.M";
                case mod::Dest::Lfo1Amplitude:      return "L1.A";
                case mod::Dest::Lfo1RandomRate:     return "L1.N";
                case mod::Dest::Lfo2Rate:           return "L2.R";
                case mod::Dest::Lfo2Mix:            return "L2.M";
                case mod::Dest::Lfo2Amplitude:      return "L2.A";
                case mod::Dest::Lfo2RandomRate:     return "L2.N";
                case mod::Dest::Lfo3Rate:           return "L3.R";
                case mod::Dest::Lfo3Mix:            return "L3.M";
                case mod::Dest::Lfo3Amplitude:      return "L3.A";
                case mod::Dest::Lfo3RandomRate:     return "L3.N";
                case mod::Dest::Lfo4Rate:           return "L4.R";
                case mod::Dest::Lfo4Mix:            return "L4.M";
                case mod::Dest::Lfo4Amplitude:      return "L4.A";
                case mod::Dest::Lfo4RandomRate:     return "L4.N";
                case mod::Dest::WildTame:           return "WILD";
                case mod::Dest::Morph:              return "MORPH";
                default: return "?";
            }
        }
    }

    // 07 MODULATION pane — tiles-with-mappings layout.
    //
    // Header right :  ▸ SELECT ANY KNOB · ESC TO CANCEL  (only when armed)
    // Body         :  9 source columns side-by-side. Each column:
    //                   · name + detach (top)
    //                   · macro attenuverter (centre)
    //                   · macro value readout
    //                   · stacked mapping rows below: "DEST  ±NN"
    //                     coloured red for positive depth, blue for negative,
    //                     dimmed when muted/disabled. Right-click a row to
    //                     delete the mapping.
    //
    // Click a source tile to arm patch-mode; while armed, every modulatable
    // Knob/AtomicKnob paints a purple "selecting" outline and the next click
    // adds (or bumps) a routing. Esc cancels.
    class ModulationPane : public Pane,
                           private juce::Timer,
                           private juce::ChangeListener,
                           private juce::KeyListener
    {
    public:
        explicit ModulationPane (TidesAudioProcessor& p)
            : Pane ("07", "MODULATION"),
              proc (p)
        {
            // Tile display order matches the design's: LFOs first, then ENV,
            // then performance sources.
            const int order[9] = {
                (int) mod::Source::Lfo1, (int) mod::Source::Lfo2, (int) mod::Source::Lfo3, (int) mod::Source::Lfo4,
                (int) mod::Source::Envelope,
                (int) mod::Source::Modwheel, (int) mod::Source::Velocity, (int) mod::Source::Aftertouch, (int) mod::Source::KeyFollow,
            };
            for (int i = 0; i < 9; ++i)
            {
                tiles[(size_t) i] = std::make_unique<SourceTile> (p, order[i], *this);
                addAndMakeVisible (*tiles[(size_t) i]);
            }

            proc.patchMode.addChangeListener (this);
            startTimerHz (10);

            setWantsKeyboardFocus (true);
            addKeyListener (this);
        }

        ~ModulationPane() override
        {
            proc.patchMode.removeChangeListener (this);
            removeKeyListener (this);
        }

        void resized() override
        {
            auto body = bodyBounds();

            // 9 columns, full body height. 2-px gap between tiles.
            const int gap = 2;
            const int n   = 9;
            const int tileW = (body.getWidth() - (n - 1) * gap) / n;
            for (int i = 0; i < n; ++i)
                tiles[(size_t) i]->setBounds (body.getX() + i * (tileW + gap),
                                               body.getY(),
                                               tileW, body.getHeight());
        }

        void paint (juce::Graphics& g) override
        {
            Pane::paint (g);

            // Banner appears in the section-header row to the right of the title
            // whenever patch-mode is armed.
            if (proc.patchMode.isArmed())
            {
                auto hr = headerRightBounds();
                const int srcIdx = proc.patchMode.armedSource();
                g.setFont (font::mono (9.0f, 0.10f));
                g.setColour (col::Amber);
                g.drawText (juce::String::fromUTF8 ("▸ ")
                            + modlabels::sourceName (srcIdx)
                            + juce::String::fromUTF8 (" → SELECT ANY KNOB · ESC TO CANCEL"),
                            juce::Rectangle<int> (dim::kPanePadSide + 110, hr.getY(),
                                                    getWidth() - dim::kPanePadSide * 2 - 110, 22),
                            juce::Justification::centredRight, false);
            }
        }

        using juce::Component::keyPressed;   // un-hide the Component overload
        bool keyPressed (const juce::KeyPress& k, juce::Component*) override
        {
            if (k.isKeyCode (juce::KeyPress::escapeKey) && proc.patchMode.isArmed())
            {
                proc.patchMode.disarm();
                return true;
            }
            return false;
        }

        // Called by SourceTile when its (non-knob, non-detach, non-mapping)
        // area is clicked.
        void toggleArmSource (int srcIndex)
        {
            if (proc.patchMode.isArmed() && proc.patchMode.armedSource() == srcIndex)
                proc.patchMode.disarm();
            else
                proc.patchMode.arm (srcIndex);
            grabKeyboardFocus();
        }

    private:
        void timerCallback() override
        {
            // Refresh tiles so newly-added connections and modulated-knob
            // value updates show.
            for (auto& t : tiles) if (t) t->refresh();
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            repaint();   // banner appears/disappears
            for (auto& t : tiles) if (t) t->refresh();
        }

        // ---- Source tile ----------------------------------------------
        class SourceTile : public juce::Component
        {
        public:
            SourceTile (TidesAudioProcessor& procRef, int srcIndex, ModulationPane& owner)
                : proc (procRef), pane (owner), idx (srcIndex),
                  macro (procRef.apvts,
                         juce::String ("macro_") + juce::String (srcIndex))
            {
                addAndMakeVisible (macro);
                setMouseCursor (juce::MouseCursor::PointingHandCursor);
            }

            void refresh() { repaint(); }

            void resized() override
            {
                const int dia = 32;
                macro.setBounds ((getWidth() - dia) / 2, 20, dia, dia);
            }

            void mouseWheelMove (const juce::MouseEvent& e,
                                 const juce::MouseWheelDetails& w) override
            {
                // Only intercept wheel when the cursor is over the mappings
                // region, so dragging the macro knob etc. isn't affected.
                if (e.position.y < (float) kMappingsTopY) return;

                // Snap to row steps — small deltas accumulate to one step.
                wheelAccum -= w.deltaY;
                while (wheelAccum >= 0.20f)  { ++scrollOffset; wheelAccum -= 0.20f; }
                while (wheelAccum <= -0.20f) { --scrollOffset; wheelAccum += 0.20f; }
                clampScroll();
                repaint();
            }

            void mouseDown (const juce::MouseEvent& e) override
            {
                const auto p = e.position.toInt();

                // Detach button area
                if (detachR.contains (p))
                {
                    auto& d = proc.patch().sourceDetached[(size_t) idx];
                    d.store (! d.load());
                    repaint();
                    return;
                }

                // Macro knob area — let the child handle (it's a child component,
                // so events on it don't actually reach here unless they hit a
                // padding pixel; defensive no-op).
                if (macro.getBounds().contains (p))
                    return;

                // Mapping row right-click → delete that mapping.
                if (e.mods.isRightButtonDown() || e.mods.isCtrlDown())
                {
                    const int rowIdx = mappingRowAt (p);
                    if (rowIdx >= 0)
                    {
                        proc.removeConnection (rowIdx);
                        repaint();
                        return;
                    }
                }

                // Otherwise: toggle arm/disarm patch-mode for this source.
                pane.toggleArmSource (idx);
            }

            void paint (juce::Graphics& g) override
            {
                const float macroV   = proc.patch().sourceMacro[(size_t) idx].load();
                const bool  detached = proc.patch().sourceDetached[(size_t) idx].load();
                const bool  active   = std::abs (macroV) > 0.005f;
                const bool  armed    = proc.patchMode.isArmed()
                                        && proc.patchMode.armedSource() == idx;

                auto rf = getLocalBounds().toFloat();

                // Tile background
                g.setColour (armed ? col::Purple.withAlpha (0.18f)
                                    : (active ? col::PanelHi : col::PanelLo));
                g.fillRoundedRectangle (rf, 5.0f);
                g.setColour (armed ? col::Purple
                                    : (active ? col::Amber.withAlpha (0.50f)
                                              : col::DividerGrout));
                g.drawRoundedRectangle (rf.reduced (0.5f), 5.0f, armed ? 1.6f : 1.0f);
                if (armed)
                {
                    g.setColour (col::Purple.withAlpha (0.18f));
                    g.drawRoundedRectangle (rf.reduced (-1.5f), 6.5f, 2.0f);
                }
                g.setColour (col::EdgeHi);
                g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

                // Source name (top-left)
                g.setColour (armed ? col::PurpleHot : (active ? col::Amber : col::Ink));
                g.setFont (font::mono (9.5f, 0.12f));
                g.drawText (modlabels::sourceName (idx),
                            6, 4, getWidth() - 28, 14,
                            juce::Justification::topLeft, false);

                // Detach (source on/off) button — bigger, clearer pill shape.
                detachR = juce::Rectangle<int> (getWidth() - 22, 2, 20, 18);
                {
                    auto br = detachR.toFloat();
                    g.setColour (detached ? col::Purple.withAlpha (0.25f)
                                          : col::PanelLo);
                    g.fillRoundedRectangle (br, 3.0f);
                    g.setColour (detached ? col::Purple : col::DividerGrout);
                    g.drawRoundedRectangle (br.reduced (0.5f), 3.0f, 1.0f);
                    g.setFont (font::mono (11.0f, 0.0f));
                    g.setColour (detached ? col::PurpleHot : col::InkMid);
                    g.drawText (detached ? juce::String::fromUTF8 ("◈")
                                         : juce::String::fromUTF8 ("◇"),
                                detachR, juce::Justification::centred, false);
                }

                // Macro value text below the knob — compact.
                g.setFont (font::mono (8.0f, 0.0f));
                g.setColour (col::InkLo);
                const auto vt = std::abs (macroV) < 0.005f
                    ? juce::String::fromUTF8 ("·")
                    : (macroV > 0.0f ? juce::String ("+") : juce::String())
                        + juce::String (juce::roundToInt (macroV * 100.0f));
                g.drawText (vt, 0, 53, getWidth(), 10,
                            juce::Justification::centred, false);

                // Faint separator
                g.setColour (col::DividerGrout);
                g.fillRect (4, 65, getWidth() - 8, 1);

                // Mapping rows
                paintMappings (g);
            }

        private:
            // Returns the connection index for the row at point p, or -1.
            int mappingRowAt (juce::Point<int> p) const
            {
                for (const auto& m : mappingRects)
                    if (m.bounds.contains (p)) return m.index;
                return -1;
            }

            void paintMappings (juce::Graphics& g)
            {
                mappingRects.clear();
                const int total = proc.patch().numConnections.load();

                // Collect this source's mapping indices first so we can scroll
                // through them and know how many exist above / below.
                juce::Array<int> mine;
                for (int i = 0; i < total; ++i)
                    if ((int) proc.patch().connections[(size_t) i].source == idx)
                        mine.add (i);

                const int sourceCount = mine.size();
                lastSourceCount = sourceCount;
                clampScroll();   // in case mappings were deleted while scrolled

                const int rowH       = 12;
                const int firstY     = kMappingsTopY;
                // Reserve a fixed strip at the very bottom for the centered
                // scroll-indicator (showing ▴ / ▾ when overflow exists). The
                // strip is always reserved so the layout doesn't jump as the
                // user scrolls.
                const int indicatorH = 14;
                const int lastY      = getHeight() - 2 - indicatorH;
                int       y          = firstY;
                int       hiddenBelow = 0;
                const int hiddenAbove = scrollOffset;

                for (int k = scrollOffset; k < sourceCount; ++k)
                {
                    const int i = mine[k];
                    const auto& c = proc.patch().connections[(size_t) i];

                    if (y + rowH > lastY)
                    {
                        hiddenBelow = sourceCount - k;
                        break;
                    }

                    auto row = juce::Rectangle<int> (3, y, getWidth() - 6, rowH);
                    mappingRects.push_back ({ i, row });

                    const bool live = c.enabled && ! c.muted;
                    const float depth = c.depth;
                    const auto polCol = (depth >= 0 ? col::Pos : col::Neg)
                                            .withAlpha (live ? 0.85f : 0.30f);

                    // Faint pill background
                    g.setColour (col::PanelLo.darker (0.3f).withAlpha (live ? 0.8f : 0.4f));
                    g.fillRoundedRectangle (row.toFloat(), 3.0f);
                    g.setColour (polCol);
                    g.drawRoundedRectangle (row.toFloat().reduced (0.5f), 3.0f, 1.0f);

                    // Dest abbrev (left)
                    g.setColour (live ? col::Ink : col::InkFaint);
                    g.setFont (font::mono (8.0f, 0.04f));
                    g.drawText (modlabels::destShort (c.dest),
                                row.reduced (3, 0).removeFromLeft (row.getWidth() - 24),
                                juce::Justification::centredLeft, false);

                    // Signed depth (right)
                    const auto pctTxt = (depth > 0 ? juce::String ("+") : juce::String())
                                        + juce::String (juce::roundToInt (depth * 100));
                    g.setColour (polCol.withAlpha (live ? 1.0f : 0.5f));
                    g.setFont (font::mono (7.5f, 0.0f, true));
                    g.drawText (pctTxt,
                                juce::Rectangle<int> (row.getRight() - 24, row.getY(), 21, rowH),
                                juce::Justification::centredRight, false);

                    y += rowH;
                }

                // Empty-state hint
                if (sourceCount == 0)
                {
                    g.setColour (col::InkFaint);
                    g.setFont (font::mono (8.0f, 0.08f));
                    g.drawText (juce::String::fromUTF8 ("· no routes"),
                                juce::Rectangle<int> (4, firstY + 6, getWidth() - 8, 12),
                                juce::Justification::centred, false);
                }

                // Wide centered scroll indicator strip at the bottom.
                const auto stripR = juce::Rectangle<int> (
                    2, getHeight() - indicatorH,
                    getWidth() - 4, indicatorH).reduced (0, 1);
                if (hiddenAbove > 0 || hiddenBelow > 0)
                {
                    // Faint pill background spanning the strip
                    g.setColour (col::Amber.withAlpha (0.10f));
                    g.fillRoundedRectangle (stripR.toFloat(), 3.0f);
                    g.setColour (col::Amber.withAlpha (0.35f));
                    g.drawRoundedRectangle (stripR.toFloat().reduced (0.5f), 3.0f, 1.0f);

                    g.setColour (col::Amber);
                    g.setFont (font::mono (12.0f, 0.0f, true));

                    if (hiddenAbove > 0 && hiddenBelow > 0)
                    {
                        // Both directions — split the strip in half.
                        const auto left  = stripR.withWidth (stripR.getWidth() / 2);
                        const auto right = stripR.withTrimmedLeft (stripR.getWidth() / 2);
                        g.drawText (juce::String::fromUTF8 ("▴ ") + juce::String (hiddenAbove),
                                    left,  juce::Justification::centred, false);
                        g.drawText (juce::String::fromUTF8 ("▾ ") + juce::String (hiddenBelow),
                                    right, juce::Justification::centred, false);
                    }
                    else if (hiddenAbove > 0)
                    {
                        g.drawText (juce::String::fromUTF8 ("▴  ") + juce::String (hiddenAbove) + " above",
                                    stripR, juce::Justification::centred, false);
                    }
                    else
                    {
                        g.drawText (juce::String::fromUTF8 ("▾  ") + juce::String (hiddenBelow) + " below",
                                    stripR, juce::Justification::centred, false);
                    }
                }
            }

            void clampScroll()
            {
                // (Re)calculate the max scroll based on what fit at last paint.
                // We can't know exactly without painting, but `lastSourceCount`
                // gives a usable bound: never let scrollOffset exceed
                // sourceCount - 1.
                if (scrollOffset < 0) scrollOffset = 0;
                const int hardMax = juce::jmax (0, lastSourceCount - 1);
                if (scrollOffset > hardMax) scrollOffset = hardMax;
            }

            // Y at which the mapping pills begin — used by both paint and the
            // mouseWheel filter (wheel only scrolls when over this region).
            static constexpr int kMappingsTopY = 69;

            TidesAudioProcessor& proc;
            ModulationPane& pane;
            int idx;
            MacroKnob macro;
            juce::Rectangle<int> detachR;

            struct MappingHit { int index; juce::Rectangle<int> bounds; };
            mutable std::vector<MappingHit> mappingRects;

            int   scrollOffset    { 0 };
            int   lastSourceCount { 0 };
            float wheelAccum      { 0.0f };
        };

        TidesAudioProcessor& proc;
        std::array<std::unique_ptr<SourceTile>, 9> tiles;
    };
}
