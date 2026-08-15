#pragma once
#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include "ui/Theme.h"
#include "ui/Buttons.h"
#include "PluginProcessor.h"
#include "vis/EngineSnapshot.h"
#include "state/FactoryPresets.h"
#include "ui/Primitives.h"

namespace tides::ui
{
    // 36 px tall group chip: G·N · name · activity dot. The name is a
    // juce::Label that swaps to a TextEditor on double-click for inline
    // renaming. The label is fed from / commits to preset().groupNames[i].
    class GroupChip : public juce::Component
    {
    public:
        GroupChip (int idx) : index (idx)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);

            // Label is editable on double-click. Background/border drawn by
            // this component; the label only draws its text.
            nameLabel.setEditable (false, true, false);   // dbl-click → edit
            nameLabel.setJustificationType (juce::Justification::centredLeft);
            nameLabel.setFont (font::sans (11.5f, false, 0.01f));
            nameLabel.setColour (juce::Label::textColourId,
                                  col::Ink);
            nameLabel.setColour (juce::Label::backgroundWhenEditingColourId,
                                  col::PanelHi);
            nameLabel.setColour (juce::Label::textWhenEditingColourId,
                                  col::Ink);
            nameLabel.setColour (juce::Label::outlineWhenEditingColourId,
                                  col::Amber.withAlpha (0.5f));
            nameLabel.setBorderSize ({ 0, 0, 0, 0 });
            // A single-click on the chip selects the group; we route that
            // explicitly so the Label doesn't swallow it.
            nameLabel.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (nameLabel);
        }

        void setGroupName (const juce::String& n)
        {
            if (n != nameLabel.getText())
                nameLabel.setText (n, juce::dontSendNotification);
        }
        juce::String getName() const { return nameLabel.getText(); }

        // Called when the label's editor commits (Enter / focus loss).
        std::function<void (const juce::String&)> onNameChanged;

        // Programmatic: enter edit mode (used by the bar's double-click).
        void beginEdit()
        {
            // Re-enable mouse clicks on the label so the editor can take
            // focus, then call into Label's edit machinery.
            nameLabel.setInterceptsMouseClicks (true, true);
            nameLabel.showEditor();
            if (auto* ed = nameLabel.getCurrentTextEditor())
                ed->selectAll();
        }

        void setActive (bool a)        { if (a != active) { active = a; repaint(); } }
        void setActivity (int s)       { if (s != activity) { activity = s; repaint(); } }
        std::function<void()> onClick;

        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
        void mouseDoubleClick (const juce::MouseEvent&) override { beginEdit(); }
        void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hover = false; repaint(); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 0);
            r.removeFromRight (12);          // dot area
            r.removeFromLeft (35);           // skip G·N number
            nameLabel.setBounds (r);
        }

        void paint (juce::Graphics& g) override
        {
            auto rf = getLocalBounds().toFloat();

            // Background
            if (active)
            {
                juce::ColourGradient grad (
                    juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.16f), 0.0f, rf.getY(),
                    juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.06f), 0.0f, rf.getBottom(),
                    false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (rf, 6.0f);
            }
            else
            {
                g.setColour (col::PanelLo);
                g.fillRoundedRectangle (rf, 6.0f);
            }

            // Border
            juce::Colour border = active
                ? juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.45f)
                : (hover ? col::Divider2 : col::DividerGrout);
            g.setColour (border);
            g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);

            // Inset top highlight
            g.setColour (active
                         ? juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.18f)
                         : col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            // G·N label (left of the name).
            auto numLabel = juce::String::fromUTF8 ("G·") + juce::String (index + 1);
            g.setFont (font::mono (9.0f, 0.08f));
            g.setColour (active ? col::Amber : col::InkLo);
            auto numR = getLocalBounds().reduced (10, 0).removeFromLeft (28);
            g.drawText (numLabel, numR, juce::Justification::centredLeft, false);

            // Activity dot (top-right area)
            const float dotX = (float) getWidth() - 12.0f;
            const float dotY = (float) getHeight() * 0.5f - 3.0f;
            const float dotR = 6.0f;
            juce::Colour dotCol = col::InkFaint;
            if      (activity == 2) dotCol = col::Purple;
            else if (activity == 1) dotCol = col::Amber;
            g.setColour (dotCol);
            g.fillEllipse (dotX, dotY, dotR, dotR);
            if (activity > 0)
            {
                g.setColour (dotCol.withAlpha (0.55f));
                g.fillEllipse (dotX - 2.0f, dotY - 2.0f, dotR + 4.0f, dotR + 4.0f);
            }
        }

        juce::Label& label() noexcept { return nameLabel; }

    private:
        int index;
        juce::Label nameLabel;
        bool active { false }, hover { false };
        int  activity { 0 };
    };

    // 56 px tall: 6 group chips (left) · group-meta (right).
    class GroupBar : public juce::Component,
                     private juce::Timer,
                     private juce::ChangeListener
    {
    private:
        // Transparent overlay placed over the morph slider so it joins the
        // patch-mode click-to-assign flow even though it's a horizontal
        // slider, not a Knob.
        class AssignOverlay : public juce::Component
        {
        public:
            std::function<void()> onClick;
            AssignOverlay() { setInterceptsMouseClicks (true, false); }
            void mouseDown (const juce::MouseEvent&) override
            { if (onClick) onClick(); }
        };

    public:
        explicit GroupBar (TidesAudioProcessor& p)
            : proc (p),
              segQuality ({ { "ECO", (int) tides::state::Quality::Eco },
                            { "STD", (int) tides::state::Quality::Std },
                            { "HQ",  (int) tides::state::Quality::HQ  } })
        {
            proc.patchMode.addChangeListener (this);
            for (int i = 0; i < 6; ++i)
            {
                chips[(size_t) i] = std::make_unique<GroupChip> (i);
                addAndMakeVisible (*chips[(size_t) i]);

                chips[(size_t) i]->onClick = [this, i]
                {
                    if (auto* param = proc.apvts.getParameter ("focused_group"))
                        param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
                };

                // Rename via Label commit: write to the live preset and bump
                // stateEpoch so persistence / undo systems notice.
                chips[(size_t) i]->label().onTextChange = [this, i]
                {
                    proc.preset().groupNames[i] = chips[(size_t) i]->label().getText();
                    proc.stateEpoch.fetch_add (1);
                };
                // After editing finishes, re-disable label mouse clicks so
                // single-clicks still go to the chip (group select).
                chips[(size_t) i]->label().onEditorHide = [this, i]
                {
                    chips[(size_t) i]->label().setInterceptsMouseClicks (false, false);
                };

                // Seed the initial name from the live preset.
                chips[(size_t) i]->setGroupName (proc.preset().groupNames[i]);
            }

            // ---- Morph controls (collapsed under the MORPH button) ----
            // MORPH toggle button — expanding the A/B endpoint pickers +
            // slider only when the user asks for them keeps the collapsed
            // group bar simple and stops people mistaking the endpoint combos
            // for "the current preset picker".
            btnMorph.setText ("Morph");
            btnMorph.onClick = [this] {
                morphExpanded = ! morphExpanded;
                syncMorphVisibility();
                // Keep the button and the slider in sync so there's only one
                // "morph on/off" concept for the user: opening the panel
                // nudges the slider off zero (active), closing it snaps it
                // back to zero (off). Any custom slider position the user
                // had dialled in is lost on close — accepted trade-off.
                if (auto* p = proc.apvts.getParameter ("morph"))
                {
                    if (morphExpanded)
                    {
                        // Restore the last non-zero position from before the
                        // previous close (falls back to the small activate
                        // value the first time round).
                        if (p->getValue() <= 0.0001f)
                            p->setValueNotifyingHost (lastMorphValue > 0.0001f
                                                        ? lastMorphValue
                                                        : kMorphActivateValue);
                    }
                    else
                    {
                        // Remember where the user had it so re-opening puts
                        // them right back on the same crossfade point.
                        const float cur = p->getValue();
                        if (cur > 0.0001f) lastMorphValue = cur;
                        p->setValueNotifyingHost (0.0f);
                    }
                }
                btnMorph.setActive (morphExpanded || morphActive);
                resized();
                repaint();
            };
            addAndMakeVisible (btnMorph);

            rebuildMorphCombos();
            cmbMorphA.onChange = [this] { onMorphComboChanged (true); };
            cmbMorphB.onChange = [this] { onMorphComboChanged (false); };
            addChildComponent (cmbMorphA);
            addChildComponent (cmbMorphB);

            sldMorph.setSliderStyle (juce::Slider::LinearHorizontal);
            sldMorph.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sldMorph.setRange (0.0, 1.0, 0.0001);
            morphAttach = std::make_unique<
                juce::AudioProcessorValueTreeState::SliderAttachment> (
                    proc.apvts, "morph", sldMorph);
            addChildComponent (sldMorph);

            morphOverlay.onClick = [this] {
                proc.assignConnection (mod::Dest::Morph);
            };
            addChildComponent (morphOverlay);

            // QUALITY Seg — bound to preset.qualityMode.
            segQuality.setSelectedValue (proc.preset().qualityMode.load(), false);
            segQuality.onChange = [this] (int v) { proc.preset().qualityMode.store (v); };
            addAndMakeVisible (segQuality);

            startTimerHz (30);
        }

        ~GroupBar() override { proc.patchMode.removeChangeListener (this); }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            const bool armed = proc.patchMode.isArmed();
            morphOverlay.setVisible (armed);
            if (armed) morphOverlay.toFront (false);
            repaint();
        }

        static constexpr int kUserItemBase = 10000;
        // Slider value used when the MORPH button flips morph "on". Small
        // enough to be a barely-audible starting point (fully at A) but
        // non-zero so the lock overlay + audio path both engage.
        static constexpr float kMorphActivateValue = 0.05f;

        // Item IDs: factory 1..F ; user (10000+idx) — matches TopBar's browse
        // scheme so the offset is unambiguous. Rebuilds must be idempotent
        // because listUserPresets() can change (Save adds, Delete removes).
        void populateMorphCombo (juce::ComboBox& cb, const juce::Array<juce::File>& userFiles)
        {
            cb.clear (juce::dontSendNotification);
            cb.addSectionHeading ("FACTORY");
            const auto& lib = tides::state::factory::library();
            for (int i = 0; i < (int) lib.size(); ++i)
                cb.addItem (juce::String::fromUTF8 (lib[(size_t) i].name), i + 1);
            if (! userFiles.isEmpty())
            {
                cb.addSeparator();
                cb.addSectionHeading ("USER");
                for (int i = 0; i < userFiles.size(); ++i)
                    cb.addItem (userFiles.getReference (i).getFileNameWithoutExtension(),
                                kUserItemBase + i);
            }
        }

        void rebuildMorphCombos()
        {
            cachedUserFiles = proc.listUserPresets();
            populateMorphCombo (cmbMorphA, cachedUserFiles);
            populateMorphCombo (cmbMorphB, cachedUserFiles);
            syncMorphCombosFromState();
        }

        void syncMorphCombosFromState()
        {
            auto pick = [this] (juce::ComboBox& cb,
                                TidesAudioProcessor::MorphKind kind,
                                int factoryIdx,
                                const juce::String& userName)
            {
                int id = 0;
                if (kind == TidesAudioProcessor::MorphKind::Factory)
                {
                    id = factoryIdx + 1;
                }
                else
                {
                    for (int i = 0; i < cachedUserFiles.size(); ++i)
                        if (cachedUserFiles.getReference (i)
                                .getFileNameWithoutExtension() == userName)
                        { id = kUserItemBase + i; break; }
                }
                if (id > 0) cb.setSelectedId (id, juce::dontSendNotification);
            };
            pick (cmbMorphA, proc.morphKindA(), proc.morphEndpointA(), proc.morphUserA());
            pick (cmbMorphB, proc.morphKindB(), proc.morphEndpointB(), proc.morphUserB());
        }

        void onMorphComboChanged (bool isA)
        {
            auto& cb = isA ? cmbMorphA : cmbMorphB;
            const int id = cb.getSelectedId();
            if (id <= 0) return;
            if (id >= kUserItemBase)
            {
                const int u = id - kUserItemBase;
                if (u >= 0 && u < cachedUserFiles.size())
                {
                    const auto& f = cachedUserFiles.getReference (u);
                    if (isA) proc.setMorphEndpointUserA (f);
                    else     proc.setMorphEndpointUserB (f);
                }
            }
            else
            {
                const int factoryIdx = id - 1;
                if (isA) proc.setMorphEndpointFactoryA (factoryIdx);
                else     proc.setMorphEndpointFactoryB (factoryIdx);
            }
        }

        void syncMorphVisibility()
        {
            const bool show = morphExpanded;
            cmbMorphA.setVisible (show);
            cmbMorphB.setVisible (show);
            sldMorph .setVisible (show);
            // Overlay is driven separately by patch-mode; hide it if the
            // slider itself is hidden so a click-through can't happen.
            if (! show) morphOverlay.setVisible (false);
        }

        void paint (juce::Graphics& g) override
        {
            // Dark Panel background
            g.setColour (col::Panel);
            g.fillRect (getLocalBounds());

            // 1 px dark grout at bottom (separates from main grid)
            g.setColour (col::DividerGrout);
            g.fillRect (getLocalBounds().removeFromBottom (1));

            // Right meta — drawn directly here (no child components for static text)
            auto r = getLocalBounds().reduced (14, 0);
            r.removeFromLeft (chipsTotalWidth() + 14);    // skip past chips area

            // Skip past the MORPH section — its label is on the button now.
            r.removeFromLeft (currentMorphSectionW());
            r.removeFromLeft (10);   // gap before QUALITY

            // ---- QUALITY label (Seg laid out in resized()) ----
            {
                auto qArea = r.removeFromLeft (kQualitySectionW);
                g.setFont (font::mono (9.5f, 0.12f));
                g.setColour (col::InkLo);
                g.drawText ("QUALITY",
                            qArea.removeFromLeft (60),
                            juce::Justification::centredLeft, false);
            }

            // Nothing left to draw — POOL duplicated the viz strip VOICES,
            // FOCUS duplicated the highlighted chip, PRESET CH duplicated
            // the perf row PRESET → CH stepper. All three removed.
        }

        void resized() override
        {
            constexpr int chipH = 36, chipW = 122, gap = 4;
            const int y = (getHeight() - chipH) / 2;
            for (int i = 0; i < 6; ++i)
                chips[(size_t) i]->setBounds (14 + i * (chipW + gap), y, chipW, chipH);

            // MORPH section sits after the chips. Layout when collapsed:
            //   [MORPH button]  ← compact
            // Layout when expanded:
            //   [MORPH button] · comboA · slider · comboB
            const int morphX = 14 + chipsTotalWidth() + 14;
            const int morphY = (getHeight() - 22) / 2;
            const int btnW   = 64;
            const int comboW = 108;
            const int gapM   = 4;

            btnMorph.setBounds (morphX, morphY, btnW, 22);

            if (morphExpanded)
            {
                int x = morphX + btnW + gapM;
                cmbMorphA.setBounds (x, morphY, comboW, 22); x += comboW + gapM;
                const int sliderW = kMorphSectionExpandedW - btnW - comboW * 2 - gapM * 3;
                sldMorph .setBounds (x, morphY + 4, sliderW, 14);
                morphOverlay.setBounds (sldMorph.getBounds());
                x += sliderW + gapM;
                cmbMorphB.setBounds (x, morphY, comboW, 22);
            }

            // QUALITY section right after morph: "QUALITY" label + Seg
            const int qX      = morphX + currentMorphSectionW() + 10;
            const int qLabelW = 60;
            const int segW    = kQualitySectionW - qLabelW;
            segQuality.setBounds (qX + qLabelW, morphY, segW - 6, 22);
        }

    private:
        static constexpr int kMorphSectionCollapsedW = 72;   // just the button + a nudge
        static constexpr int kMorphSectionExpandedW  = 380;
        static constexpr int kQualitySectionW        = 180;
        int chipsTotalWidth() const { return 6 * 122 + 5 * 4; }
        int currentMorphSectionW() const {
            return morphExpanded ? kMorphSectionExpandedW : kMorphSectionCollapsedW;
        }

        void timerCallback() override
        {
            // Focused chip
            int focused = 0;
            if (auto* fp = proc.apvts.getRawParameterValue ("focused_group"))
                focused = (int) fp->load();
            for (int i = 0; i < 6; ++i)
            {
                chips[(size_t) i]->setActive (i == focused);
                // Pull name from live preset (cheap; setName is a no-op if
                // unchanged). Skip while the label's editor is open so we
                // don't yank the text the user is typing.
                if (chips[(size_t) i]->label().getCurrentTextEditor() == nullptr)
                    chips[(size_t) i]->setGroupName (proc.preset().groupNames[i]);
            }

            // Resync morph endpoint combos + QUALITY on external changes.
            // Cheap poll of the user preset list; if it changed (Save added a
            // new one, Delete removed one) rebuild both combos so morph can
            // pick from the current library.
            {
                const auto userFiles = proc.listUserPresets();
                bool changed = userFiles.size() != cachedUserFiles.size();
                for (int i = 0; ! changed && i < userFiles.size(); ++i)
                    changed = userFiles.getReference (i) != cachedUserFiles.getReference (i);
                if (changed) rebuildMorphCombos();
                else         syncMorphCombosFromState();
            }
            segQuality.setSelectedValue (proc.preset().qualityMode.load(), false);

            // Morph "active" tint — when the slider is > 0 the focused group
            // is being overridden block-by-block, so make the slider light up
            // amber and the MORPH button light up so the user knows knob
            // edits will get clobbered by the morph until they pull it back
            // to 0. The main editor also drops a big lock overlay over the
            // panels in this state.
            const float t = proc.morphValue();
            const bool active = t > 0.0005f;
            if (active != morphActive)
            {
                morphActive = active;
                sldMorph.setColour (juce::Slider::trackColourId,
                                     active ? col::Amber : col::Divider2);
                sldMorph.setColour (juce::Slider::backgroundColourId,
                                     active ? col::Amber.withAlpha (0.18f)
                                            : col::PanelLo);
                btnMorph.setActive (active || morphExpanded);
                repaint();
            }

            // Activity per chip — scan snapshot voices, find max state per group.
            int act[6] {};
            const auto& snap = proc.snapshot();
            for (int v = 0; v < vis::EngineSnapshot::kNumVoices; ++v)
            {
                const auto& vs = snap.voices[(size_t) v];
                const int gIdx = vs.group.load();
                const auto st  = (vis::EngineSnapshot::VoiceState) vs.state.load();
                if (gIdx < 0 || gIdx >= 6 || st == vis::EngineSnapshot::Idle) continue;
                const int level = (st == vis::EngineSnapshot::Ringing) ? 2 : 1;
                if (level > act[gIdx]) act[gIdx] = level;
            }
            for (int i = 0; i < 6; ++i)
                chips[(size_t) i]->setActivity (act[i]);

            // Repaint meta (focus/preset ch/pool may change)
            repaint();
        }

        TidesAudioProcessor& proc;
        std::array<std::unique_ptr<GroupChip>, 6> chips;

        juce::ComboBox cmbMorphA, cmbMorphB;
        juce::Slider   sldMorph;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
            morphAttach;
        AssignOverlay  morphOverlay;
        PillButton     btnMorph;
        Seg            segQuality;
        bool           morphActive { false };
        bool           morphExpanded { false };
        float          lastMorphValue { 0.0f };   // remembered across button toggles
        juce::Array<juce::File> cachedUserFiles;
    };
}
