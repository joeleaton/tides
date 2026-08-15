#pragma once
#include <JuceHeader.h>
#include "ui/Theme.h"
#include "ui/Buttons.h"
#include "PluginProcessor.h"
#include "state/FactoryPresets.h"

namespace tides::ui
{
    // 44 px tall: brand block · preset bar · action buttons.
    class TopBar : public juce::Component,
                   private juce::Timer,
                   private juce::ChangeListener
    {
    public:
        explicit TopBar (TidesAudioProcessor& p) : proc (p)
        {
            proc.patchMode.addChangeListener (this);
            addAndMakeVisible (btnPrev);   btnPrev.setText   (juce::String::fromUTF8 ("◀"));
            addAndMakeVisible (btnNext);   btnNext.setText   (juce::String::fromUTF8 ("▶"));
            addAndMakeVisible (btnBrowse); btnBrowse.setText ("Browse");
            addAndMakeVisible (ab);
            addAndMakeVisible (btnSave);   btnSave.setText   ("Save");
            addAndMakeVisible (btnInit);   btnInit.setText   ("Init");
            addAndMakeVisible (btnNotes);  btnNotes.setText  (juce::String::fromUTF8 ("◇ Notes"));

            // A/B → toggle compare. Visual state polled in timerCallback.
            ab.onClick = [this] { proc.toggleCompare(); };

            // Prev / Next / Browse step through the factory library.
            btnPrev.onClick   = [this] { stepPreset (-1); };
            btnNext.onClick   = [this] { stepPreset (+1); };
            btnBrowse.onClick = [this] { showBrowseMenu(); };
            btnSave.onClick   = [this] { showSaveDialog(); };
            btnInit.onClick   = [this] { proc.loadInitPatch(); repaint(); };
            btnNotes.onClick  = [this] { showPresetNotes(); };

            startTimerHz (30);
        }

        ~TopBar() override { proc.patchMode.removeChangeListener (this); }

        // Right-click anywhere on the top bar cancels patch-mode.
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isRightButtonDown() && proc.patchMode.isArmed())
                proc.patchMode.disarm();
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds();

            // Gradient PanelHi → Panel
            g.setGradientFill (juce::ColourGradient (col::PanelHi, 0.0f, (float) r.getY(),
                                                     col::Panel,    0.0f, (float) r.getBottom(),
                                                     false));
            g.fillRect (r);
            // 1 px dark grout at bottom
            g.setColour (col::DividerGrout);
            g.fillRect (r.removeFromBottom (1));

            // Brand block — icon + name + version
            auto brand = juce::Rectangle<int> (14, 0, 210, getHeight());

            // Two-wave "tides" icon in cyan + purple, drawn procedurally
            // (so it scales with the theme colours and needs no asset file).
            {
                auto icon = brand.removeFromLeft (24)
                                  .withSizeKeepingCentre (22, 22);
                paintTidesIcon (g, icon);
                brand.removeFromLeft (8);
            }

            // TIDES wordmark
            g.setColour (col::Ink);
            g.setFont (font::sans (15.0f, true, 0.16f));
            g.drawText ("TIDES", brand.removeFromLeft (60),
                        juce::Justification::centredLeft, false);

            // Tiny cyan dot separator between brand mark and version
            g.setColour (col::Amber);
            g.fillEllipse ((float) brand.getX() - 4.0f,
                           (float) (getHeight() / 2 - 2),
                           4.0f, 4.0f);
            brand.removeFromLeft (8);

            g.setColour (col::InkLo);
            g.setFont (font::mono (9.0f, 0.10f));
            g.drawText (juce::String::fromUTF8 ("v1.1  ·  BBD/RESONATOR"), brand,
                        juce::Justification::centredLeft, false);

            // Brand right-divider
            g.setColour (col::DividerGrout);
            g.fillRect (juce::Rectangle<int> (234, 8, 1, getHeight() - 16));

            // Preset bar — replaced by a patch-mode banner when armed.
            auto presetR = presetBarBounds();
            if (proc.patchMode.isArmed())
                paintPatchModeBanner (g, presetR);
            else
                paintPresetBar (g, presetR);
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            repaint();   // banner appears/disappears
        }

        // Big amber HUD-style banner that replaces the preset bar while
        // patch-mode is armed. The source name + cancel hint sit right in
        // the spot the user normally looks for "what preset am I on?".
        void paintPatchModeBanner (juce::Graphics& g, juce::Rectangle<int> r)
        {
            const int srcIdx = proc.patchMode.armedSource();
            const auto rf = r.toFloat();

            // Subtle purple-amber gradient background
            juce::ColourGradient grad (
                col::Purple.withAlpha (0.22f),  0.0f, (float) r.getY(),
                col::Amber .withAlpha (0.22f),  0.0f, (float) r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (rf, 6.0f);
            g.setColour (col::Amber.withAlpha (0.55f));
            g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.5f);
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            // Source label on the left
            g.setFont (font::mono (12.0f, 0.14f, true));
            g.setColour (col::PurpleHot);
            auto pad = r.reduced (16, 0);
            const auto srcLabel = juce::String::fromUTF8 ("▸ PATCH MODE  ·  ")
                                + juce::String (sourceNameForArmed (srcIdx));
            g.drawText (srcLabel, pad,
                        juce::Justification::centredLeft, false);

            // Hint on the right
            g.setFont (font::mono (9.5f, 0.16f));
            g.setColour (col::Amber);
            g.drawText (juce::String::fromUTF8 ("→ CLICK ANY KNOB  ·  ESC OR RIGHT-CLICK CANCELS"),
                        pad, juce::Justification::centredRight, false);
        }

        static const char* sourceNameForArmed (int s)
        {
            switch ((tides::mod::Source) s)
            {
                case tides::mod::Source::Envelope:   return "ENV";
                case tides::mod::Source::Lfo1:       return "LFO1";
                case tides::mod::Source::Lfo2:       return "LFO2";
                case tides::mod::Source::Lfo3:       return "LFO3";
                case tides::mod::Source::Lfo4:       return "LFO4";
                case tides::mod::Source::Modwheel:   return "MWHL";
                case tides::mod::Source::Velocity:   return "VEL";
                case tides::mod::Source::Aftertouch: return "AT";
                case tides::mod::Source::KeyFollow:  return "KEY";
                default: return "?";
            }
        }

        // ── Brand icon — two stacked sine arcs in cyan + purple,
        // suggesting the interference/tide-on-tide motion that the synth
        // emulates. Fully procedural so it tracks the theme colours.
        static void paintTidesIcon (juce::Graphics& g, juce::Rectangle<int> r)
        {
            const float iw = (float) r.getWidth();
            const float ih = (float) r.getHeight();
            const float x0 = (float) r.getX();
            const float y0 = (float) r.getY();

            auto buildWave = [&] (float yFracCentre, float phaseOffset)
            {
                juce::Path p;
                constexpr int N = 24;
                for (int i = 0; i <= N; ++i)
                {
                    const float t = (float) i / (float) N;
                    const float x = x0 + t * iw;
                    const float y = y0 + ih * yFracCentre
                                       - std::sin ((t + phaseOffset)
                                                    * juce::MathConstants<float>::twoPi)
                                         * ih * 0.18f;
                    if (i == 0) p.startNewSubPath (x, y);
                    else        p.lineTo            (x, y);
                }
                return p;
            };

            const auto top = buildWave (0.34f, 0.00f);
            const auto bot = buildWave (0.70f, 0.50f);

            // Glow halos
            g.setColour (col::Amber.withAlpha (0.35f));
            g.strokePath (top, juce::PathStrokeType (3.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));
            g.setColour (col::Purple.withAlpha (0.35f));
            g.strokePath (bot, juce::PathStrokeType (3.0f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));

            // Cores
            g.setColour (col::Amber);
            g.strokePath (top, juce::PathStrokeType (1.3f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));
            g.setColour (col::Purple);
            g.strokePath (bot, juce::PathStrokeType (1.3f,
                                  juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded));
        }

        void resized() override
        {
            // Action buttons laid out from the right, 6 px gap
            constexpr int btnH = 28, gap = 6;
            const int y = (getHeight() - btnH) / 2;
            int x = getWidth() - 14;

            auto place = [&] (juce::Component& c, int w)
            {
                x -= w;
                c.setBounds (x, y, w, btnH);
                x -= gap;
            };
            place (btnNotes, 76);
            place (btnInit,  56);
            place (btnSave,  60);
            place (ab,       52);
            place (btnBrowse,72);
            place (btnNext,  28);
            place (btnPrev,  28);

            actionsLeft = x;       // remember for preset-bar right edge
        }

    private:
        juce::Rectangle<int> presetBarBounds() const
        {
            constexpr int presetX = 246;     // after brand block + divider
            constexpr int padR    = 12;
            const int x = presetX;
            const int w = juce::jmax (0, actionsLeft - padR - x);
            const int h = 28;
            return { x, (getHeight() - h) / 2, w, h };
        }

        void paintPresetBar (juce::Graphics& g, juce::Rectangle<int> r)
        {
            const bool savedShowing = proc.showingSaved.load();
            const auto rf = r.toFloat();

            // Background
            if (savedShowing)
            {
                juce::ColourGradient grad (col::Amber.withAlpha (0.12f), 0.0f, (float) r.getY(),
                                           col::PanelLo,                  0.0f, (float) r.getBottom(),
                                           false);
                g.setGradientFill (grad);
            }
            else
            {
                g.setColour (col::PanelLo);
            }
            g.fillRoundedRectangle (rf, 6.0f);

            // Border
            juce::Colour border = savedShowing
                ? juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.50f)
                : col::DividerGrout;
            g.setColour (border);
            g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
            // Inset shadow
            g.setColour (col::EdgeHi);
            g.fillRect (rf.reduced (1.0f).withHeight (1.0f));

            // Live preset info. If a user preset is currently loaded, its
            // name overrides the factory index display. If INIT was hit, the
            // bar reads "INIT · blank patch".
            const bool isInit  = proc.isInitLoaded();
            const auto userName = proc.currentUserPresetName();
            const bool isUser   = ! isInit && userName.isNotEmpty();
            const int  idx      = proc.currentPresetIndex();
            const auto& pi      = tides::state::factory::info (
                                      juce::jlimit (0,
                                          (int) tides::state::factory::library().size() - 1,
                                          idx));
            const auto numStr = isInit  ? juce::String ("INI")
                              : isUser  ? juce::String ("USR")
                                        : juce::String (idx + 1).paddedLeft ('0', 3);

            auto pad = r.reduced (12, 0);
            g.setFont (font::mono (9.5f, 0.08f));
            g.setColour ((isUser || isInit) ? col::Amber : col::InkMid);
            g.drawText (numStr, pad.removeFromLeft (32),
                        juce::Justification::centredLeft, false);
            pad.removeFromLeft (8);

            // Preset name (fromUTF8 — name may contain non-ASCII).
            g.setFont (font::sans (13.0f, false, 0.01f));
            g.setColour (col::Ink);
            auto tagsArea = pad.removeFromRight (260);
            auto nameArea = pad;
            g.drawText (isInit ? juce::String ("INIT")
                       : isUser ? userName
                                : juce::String::fromUTF8 (pi.name),
                        nameArea,
                        juce::Justification::centredLeft, false);

            // Right side: SAVED badge OR category + tags
            g.setFont (font::mono (9.5f, 0.08f));
            if (savedShowing)
            {
                auto badge = tagsArea.removeFromLeft (118).reduced (0, 6);
                g.setColour (juce::Colour::fromFloatRGBA (0.480f, 0.843f, 1.0f, 0.5f));
                g.drawRoundedRectangle (badge.toFloat(), 3.0f, 1.0f);
                g.setColour (col::Amber);
                g.setFont (font::mono (8.5f, 0.16f));
                g.drawText (juce::String::fromUTF8 ("▸ SAVED VERSION"), badge,
                            juce::Justification::centred, false);
            }
            else if (isUser)
            {
                g.setColour (col::Amber.withAlpha (0.75f));
                g.setFont (font::mono (9.5f, 0.16f));
                g.drawText (juce::String::fromUTF8 ("· user · saved"),
                            tagsArea, juce::Justification::centredLeft, false);
            }
            else
            {
                // Category chip — pi.category and pi.tags are UTF-8 (mid-dot
                // separators), must decode through fromUTF8 not the implicit
                // const char* → Latin-1 conversion.
                const auto catTxt = juce::String::fromUTF8 ("· ")
                                  + juce::String::fromUTF8 (pi.category);
                const int catW = juce::GlyphArrangement::getStringWidthInt (
                                    font::mono (9.5f, 0.16f), catTxt) + 8;
                g.setColour (col::InkMid);
                g.drawText (catTxt, tagsArea.removeFromLeft (catW),
                            juce::Justification::centredLeft, false);

                g.setColour (col::InkLo);
                g.setFont (font::mono (9.5f, 0.08f));
                g.drawText (juce::String::fromUTF8 ("· ")
                              + juce::String::fromUTF8 (pi.tags),
                            tagsArea,
                            juce::Justification::centredLeft, false);
            }
        }

        void timerCallback() override
        {
            const bool s = proc.showingSaved.load();
            if (s != ab.getShowingSaved())
            {
                ab.setShowingSaved (s);
                repaint();      // preset bar restyles too
            }
            const int idx = proc.currentPresetIndex();
            if (idx != lastPresetIdx) { lastPresetIdx = idx; repaint(); }
        }

        void stepPreset (int delta)
        {
            const int n = tides::state::factory::count();
            if (n <= 0) return;
            int next = proc.currentPresetIndex() + delta;
            while (next < 0)   next += n;
            while (next >= n)  next -= n;
            proc.loadFactoryPreset (next);
            repaint();
        }

        void showPresetNotes()
        {
            // Preset description — not MIDI in. Pulls the current factory
            // entry's category / name / tags and shows them in a small alert.
            const int idx = proc.currentPresetIndex();
            const auto& pi = tides::state::factory::info (idx);

            const auto msg = juce::String::fromUTF8 ("Preset ")
                            + juce::String (idx + 1).paddedLeft ('0', 3)
                            + juce::String::fromUTF8 ("  ·  ")
                            + juce::String::fromUTF8 (pi.category)
                            + "\n\n"
                            + juce::String::fromUTF8 (pi.tags)
                            + "\n\n"
                            + "Single-group presets load into the currently focused\n"
                            + "voice group. Multi-group (LAYERED) presets replace\n"
                            + "all groups. Use BROWSE for the full library, A/B to\n"
                            + "compare your edits against the loaded baseline.";

            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::NoIcon,
                juce::String::fromUTF8 (pi.name),
                msg,
                "Close",
                this);
        }

        void showSaveDialog()
        {
            // Default name = current factory preset name or the user preset
            // currently loaded — so re-saving doesn't require retyping.
            juce::String defaultName = proc.currentUserPresetName();
            if (defaultName.isEmpty())
                defaultName = juce::String::fromUTF8 (
                    tides::state::factory::info (proc.currentPresetIndex()).name);

            auto* aw = new juce::AlertWindow ("Save Preset",
                "Save the focused group's sound as a user preset.",
                juce::AlertWindow::NoIcon);
            aw->addTextEditor ("name", defaultName, "Preset name:");
            aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true,
                juce::ModalCallbackFunction::create (
                    [this, aw] (int result)
                    {
                        if (result == 1)
                        {
                            const auto name = aw->getTextEditorContents ("name").trim();
                            if (name.isNotEmpty())
                                attemptSave (name);
                        }
                        delete aw;
                    }),
                false);
        }

        // Save with overwrite confirmation if the name collides with an
        // existing user preset. Re-saving over the *currently loaded* user
        // preset (same name) skips the prompt — that's the common "save
        // current edits" path and the user clearly intended it.
        void attemptSave (const juce::String& name)
        {
            const bool exists       = proc.userPresetExists (name);
            const bool sameAsLoaded = (name == proc.currentUserPresetName());

            if (exists && ! sameAsLoaded)
            {
                juce::AlertWindow::showOkCancelBox (
                    juce::AlertWindow::WarningIcon,
                    "Overwrite preset?",
                    "A user preset named \"" + name + "\" already exists. "
                    "Overwriting will replace its contents permanently.",
                    "Overwrite", "Cancel", this,
                    juce::ModalCallbackFunction::create (
                        [this, name] (int result)
                        {
                            if (result == 1)
                            {
                                proc.saveCurrentAsUserPreset (name);
                                repaint();
                            }
                        }));
                return;
            }

            proc.saveCurrentAsUserPreset (name);
            repaint();
        }

        void showBrowseMenu()
        {
            const auto& lib = tides::state::factory::library();
            const int current = proc.currentPresetIndex();

            juce::PopupMenu root;

            // Layer toggle — sticky within the session. When ON, every
            // subsequent preset selection lands on the focused group and
            // leaves the rest of the rack alone, so you can stack
            // presets across groups. When OFF (default), single-group
            // preset loads replace the whole rack.
            //
            // Heavily flagged because users were previously missing it
            // at the top of the menu: explicit section header above,
            // emoji bullet so the item visually stands out from the
            // category list, separator below.
            root.addSectionHeader (juce::String::fromUTF8 ("◇ MODE"));
            root.addItem (9994,
                          juce::String::fromUTF8 (layerMode
                              ? "◈   Layer mode  ON    (loads into focused group only)"
                              : "◇   Layer mode  OFF   (loads replace the rack)"),
                          true, layerMode);
            root.addSeparator();
            root.addSectionHeader ("PRESETS");

            juce::String prevCat;
            juce::PopupMenu sub;

            auto flushCategory = [&] (const juce::String& cat)
            {
                if (cat.isNotEmpty())
                    root.addSubMenu (cat, sub);
                sub.clear();
            };

            for (int i = 0; i < (int) lib.size(); ++i)
            {
                const auto& pi = lib[(size_t) i];
                const juce::String cat (pi.category);
                if (cat != prevCat)
                {
                    flushCategory (prevCat);
                    prevCat = cat;
                }
                sub.addItem (i + 1, juce::String::fromUTF8 (pi.name),
                             true, i == current);
            }
            flushCategory (prevCat);

            // User presets — listed at the bottom, item IDs offset by 10000
            // so they don't collide with the factory IDs. The Manage entry
            // uses a sentinel ID (9999) outside both ranges.
            const auto userFiles = proc.listUserPresets();
            juce::PopupMenu userSub;
            const auto currentUser = proc.currentUserPresetName();
            for (int i = 0; i < userFiles.size(); ++i)
            {
                const auto& f  = userFiles.getReference (i);
                const auto nm = f.getFileNameWithoutExtension();
                userSub.addItem (10000 + i, nm, true, nm == currentUser);
            }
            if (! userFiles.isEmpty())
                userSub.addSeparator();
            userSub.addItem (9999, "Manage Presets...",
                             ! userFiles.isEmpty() || true);
            userSub.addItem (9998, "Reveal Folder in Finder");
            root.addSeparator();
            root.addSubMenu ("USER", userSub);

            root.showMenuAsync (
                juce::PopupMenu::Options().withTargetComponent (&btnBrowse),
                [this, userFiles] (int chosen)
                {
                    if (chosen <= 0) return;
                    if (chosen == 9998)
                    {
                        proc.userPresetDirectory().revealToUser();
                        return;
                    }
                    if (chosen == 9999)
                    {
                        showManagePresetsDialog();
                        return;
                    }
                    if (chosen == 9994)
                    {
                        // Layer toggle clicked — flip the flag and re-open
                        // the menu so the user can immediately load with
                        // the new mode in effect.
                        layerMode = ! layerMode;
                        juce::MessageManager::callAsync (
                            [this] { showBrowseMenu(); });
                        return;
                    }
                    if (chosen >= 10000)
                    {
                        const int idx = chosen - 10000;
                        if (idx >= 0 && idx < userFiles.size())
                            proc.loadUserPreset (userFiles.getReference (idx));
                            // User presets only ever touch the focused
                            // group, so the layer flag is a no-op here.
                    }
                    else
                    {
                        proc.loadFactoryPreset (chosen - 1, layerMode);
                    }
                    repaint();
                });
        }

        // Modal list + Rename / Delete / Reveal buttons. Re-opens itself
        // after each action so the user can chain edits.
        void showManagePresetsDialog (int initialSelection = 0)
        {
            const auto files = proc.listUserPresets();

            auto* aw = new juce::AlertWindow ("Manage User Presets",
                files.isEmpty()
                    ? "You have no user presets yet. Use SAVE to store the\n"
                      "focused group's sound, then come back here to organise."
                    : "Select a preset, then Rename, Delete, or Reveal it.",
                juce::AlertWindow::NoIcon);

            juce::StringArray names;
            for (int i = 0; i < files.size(); ++i)
                names.add (files.getReference (i).getFileNameWithoutExtension());

            if (! files.isEmpty())
            {
                aw->addComboBox ("preset", names, "Preset:");
                if (auto* cb = aw->getComboBoxComponent ("preset"))
                    cb->setSelectedItemIndex (
                        juce::jlimit (0, names.size() - 1, initialSelection), juce::dontSendNotification);
            }

            // Button IDs: 1 = Rename, 2 = Delete, 3 = Reveal, 0 = Close.
            if (! files.isEmpty())
            {
                aw->addButton ("Rename", 1);
                aw->addButton ("Delete", 2);
                aw->addButton ("Reveal", 3);
            }
            aw->addButton ("Close", 0, juce::KeyPress (juce::KeyPress::escapeKey));

            aw->enterModalState (true,
                juce::ModalCallbackFunction::create (
                    [this, aw, files] (int result)
                    {
                        const int sel = (aw->getComboBoxComponent ("preset") != nullptr)
                                          ? aw->getComboBoxComponent ("preset")->getSelectedItemIndex()
                                          : -1;
                        delete aw;

                        if (result == 0 || sel < 0 || sel >= files.size())
                            return;

                        const auto file = files.getReference (sel);
                        switch (result)
                        {
                            case 1: promptRename (file, sel); break;
                            case 2: promptDelete (file, sel); break;
                            case 3: file.revealToUser();
                                    juce::MessageManager::callAsync (
                                        [this, sel] { showManagePresetsDialog (sel); });
                                    break;
                            default: break;
                        }
                    }),
                false);
        }

        void promptRename (const juce::File& file, int reopenIndex)
        {
            auto* aw = new juce::AlertWindow ("Rename Preset",
                "Enter a new name for \"" + file.getFileNameWithoutExtension() + "\".",
                juce::AlertWindow::NoIcon);
            aw->addTextEditor ("name", file.getFileNameWithoutExtension(), "New name:");
            aw->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
            aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            aw->enterModalState (true,
                juce::ModalCallbackFunction::create (
                    [this, aw, file, reopenIndex] (int result)
                    {
                        const auto newName = aw->getTextEditorContents ("name").trim();
                        delete aw;

                        if (result == 1 && newName.isNotEmpty())
                        {
                            if (! proc.renameUserPreset (file, newName))
                            {
                                juce::AlertWindow::showMessageBoxAsync (
                                    juce::AlertWindow::WarningIcon,
                                    "Rename failed",
                                    "Could not rename — a preset with that name "
                                    "may already exist.",
                                    "OK");
                            }
                            else
                            {
                                repaint();
                            }
                        }
                        juce::MessageManager::callAsync (
                            [this, reopenIndex] { showManagePresetsDialog (reopenIndex); });
                    }),
                false);
        }

        void promptDelete (const juce::File& file, int reopenIndex)
        {
            juce::AlertWindow::showOkCancelBox (
                juce::AlertWindow::WarningIcon,
                "Delete preset?",
                "Delete \"" + file.getFileNameWithoutExtension() + "\"? "
                "This cannot be undone.",
                "Delete", "Cancel", this,
                juce::ModalCallbackFunction::create (
                    [this, file, reopenIndex] (int result)
                    {
                        if (result == 1)
                        {
                            proc.deleteUserPreset (file);
                            repaint();
                        }
                        juce::MessageManager::callAsync (
                            [this, reopenIndex] { showManagePresetsDialog (reopenIndex - 1); });
                    }));
        }

        TidesAudioProcessor& proc;
        PillButton btnPrev, btnNext, btnBrowse, btnSave, btnInit, btnNotes;
        ABButton   ab;
        int actionsLeft { 0 };
        int lastPresetIdx { -1 };
        // Session-sticky "Layer mode" — when on, factory preset loads
        // from BROWSE preserve all non-focused groups. Toggled via the
        // checkable item at the top of the BROWSE menu.
        bool layerMode { false };
    };
}
