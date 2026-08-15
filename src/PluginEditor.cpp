#include "PluginEditor.h"
#include "ui/Theme.h"

using namespace tides::ui;

// ─────────────────────────────────────────────────────────────────────────
// P1 — TopBar + GroupBar live; the rest of the editor is still the skeleton
// from P0 (panel backgrounds with section titles, placeholders for viz
// strip + perf row).
// ─────────────────────────────────────────────────────────────────────────

TidesAudioProcessorEditor::TidesAudioProcessorEditor (TidesAudioProcessor& p)
    : juce::AudioProcessorEditor (p), proc (p),
      topBar (p), groupBar (p), exciterPane (p), envelopePane (p),
      resonatorPane (p), outputPane (p), quadLfoPane (p), modulationPane (p),
      vizStrip (p), perfRow (p)
{
    addAndMakeVisible (topBar);
    addAndMakeVisible (groupBar);
    addAndMakeVisible (exciterPane);
    addAndMakeVisible (envelopePane);
    addAndMakeVisible (resonatorPane);
    addAndMakeVisible (outputPane);
    addAndMakeVisible (quadLfoPane);
    addAndMakeVisible (modulationPane);
    addAndMakeVisible (vizStrip);
    addAndMakeVisible (perfRow);
    addChildComponent (morphLock);   // shown when morphValue > 0

    setSize (dim::kEditorW, dim::kEditorH);
    setResizable (false, false);
    setOpaque (true);
    lastEpoch = proc.stateEpoch.load();
    startTimer (33);
}

void TidesAudioProcessorEditor::timerCallback()
{
    const int e = proc.stateEpoch.load();
    if (e != lastEpoch) { lastEpoch = e; repaint(); }

    // Morph-lock overlay: covers the panels whenever the morph slider is
    // above 0, so knobs the morph is currently overriding can't be dragged
    // (and if they were, the value would just snap back on the next block).
    const bool morphOn = proc.morphValue() > 0.0005f;
    if (morphOn != morphLock.isVisible())
    {
        morphLock.setVisible (morphOn);
        if (morphOn) morphLock.toFront (false);
    }
}

void TidesAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // Right-click anywhere on the editor cancels patch-mode — a discoverable
    // fall-back for users who didn't notice the Esc hint in the banner.
    if (e.mods.isRightButtonDown() && proc.patchMode.isArmed())
        proc.patchMode.disarm();
}

void TidesAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::BgDeep);

    auto body = getLocalBounds().toFloat();
    g.setColour (col::Bg);
    g.fillRoundedRectangle (body, (float) dim::kCornerR);
    g.setColour (col::Divider2);
    g.drawRoundedRectangle (body.reduced (0.5f), (float) dim::kCornerR, 1.0f);

    auto r = getLocalBounds();

    // Top rows are owned by child components; just skip past their bounds.
    r.removeFromTop (dim::kTopBarH);
    r.removeFromTop (dim::kGroupBarH);

    // Perf row is a real child component; just step past it.
    r.removeFromBottom (dim::kPerfRowH);

    // Viz strip is a real child component — just step past it.
    r.removeFromBottom (dim::kVizStripH);

    // ---- Row 3 main (panels) ----------------------------------------
    auto main = r;
    g.setColour (col::DividerGrout);
    g.fillRect (main);

    // Exciter / Resonator / Output are all real child components — no placeholder
    // painting required in the top main row.
    main.removeFromTop (dim::kMainTopH);

    // Bottom main row: Envelope + Quad LFO + Modulation are all real child
    // components — no placeholder painting required.
    main.removeFromTop (dim::kGrout);
}

void TidesAudioProcessorEditor::resized()
{
    topBar  .setBounds (0, 0,                 getWidth(), dim::kTopBarH);
    groupBar.setBounds (0, dim::kTopBarH,     getWidth(), dim::kGroupBarH);

    // Main top row.
    const int mainY = dim::kTopBarH + dim::kGroupBarH;
    exciterPane.setBounds (0, mainY, dim::kExciterW, dim::kMainTopH);

    // Resonator pane in the center of the top main row.
    const int resX = dim::kExciterW + dim::kGrout;
    const int resW = getWidth() - dim::kExciterW - dim::kOutputW - 2 * dim::kGrout;
    resonatorPane.setBounds (resX, mainY, resW, dim::kMainTopH);

    // Output pane on the right edge.
    outputPane.setBounds (getWidth() - dim::kOutputW, mainY,
                          dim::kOutputW, dim::kMainTopH);

    // Main bottom row.
    const int mainBotY = mainY + dim::kMainTopH + dim::kGrout;
    envelopePane.setBounds (0, mainBotY, dim::kEnvelopeW, dim::kMainBotH);

    // Quad LFO pane in the middle of the bottom main row.
    const int lfoX = dim::kEnvelopeW + dim::kGrout;
    quadLfoPane.setBounds (lfoX, mainBotY, dim::kQuadLFOW, dim::kMainBotH);

    // Modulation pane on the right of the bottom main row.
    const int modX = lfoX + dim::kQuadLFOW + dim::kGrout;
    const int modW = getWidth() - modX;
    modulationPane.setBounds (modX, mainBotY, modW, dim::kMainBotH);

    // Viz strip — below the main rows, above the perf row.
    const int vizY = mainBotY + dim::kMainBotH;
    vizStrip.setBounds (0, vizY, getWidth(), dim::kVizStripH);

    // Perf row at the very bottom.
    perfRow.setBounds (0, vizY + dim::kVizStripH, getWidth(), dim::kPerfRowH);

    // Morph lock overlay covers everything below the group bar so no knob
    // in any pane can be dragged while morph is doing the driving. Top bar
    // and group bar stay live (that's where the morph slider lives, and the
    // user needs to be able to pull it back to 0 or hit INIT / BROWSE).
    morphLock.setBounds (0, mainY, getWidth(),
                         getHeight() - mainY);
}
