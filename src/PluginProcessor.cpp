#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "state/FactoryPresets.h"

static const char* kMacroNames[9] =
{ "Macro Envelope","Macro LFO1","Macro LFO2","Macro LFO3","Macro LFO4",
  "Macro Modwheel","Macro Velocity","Macro Aftertouch","Macro KeyFollow" };

static juce::String macroId (int i) { return juce::String ("macro_") + juce::String (i); }

juce::AudioProcessorValueTreeState::ParameterLayout
TidesAudioProcessor::createParameterLayout()
{
    using namespace juce;
    using namespace tides::params;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto pp = static_cast<P> (i);
        const auto d  = descriptor (pp);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID (d.id, 1), d.name,
            NormalisableRange<float> (0.0f, 1.0f, 0.0001f), d.def));
    }
    for (int i = 0; i < 9; ++i)
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID (macroId (i), 1), kMacroNames[i],
            NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID ("focused_group", 1), "Focused Group",
        StringArray { "Group 1","Group 2","Group 3","Group 4","Group 5","Group 6" },
        0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID ("morph", 1), "Morph",
        NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    return layout;
}

TidesAudioProcessor::TidesAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Ext In", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Tides", createParameterLayout())
{
    for (int i = 0; i < tides::params::kNumParams; ++i)
        contParamPtr[(size_t) i] = apvts.getRawParameterValue (
            tides::params::descriptor (static_cast<tides::params::P> (i)).id);
    for (int i = 0; i < 9; ++i)
        macroParamPtr[(size_t) i] = apvts.getRawParameterValue (macroId (i));
    focusedParamPtr = apvts.getRawParameterValue ("focused_group");
    morphParamPtr   = apvts.getRawParameterValue ("morph");

    // Initial morph endpoint caches — preset 0 and preset 1 by default.
    setMorphEndpointFactoryA (0);
    setMorphEndpointFactoryB (juce::jmin (1, tides::state::factory::count() - 1));

    // Restore any user-saved MIDI channel preferences on top of the
    // just-constructed default preset. Host-persisted state (via
    // setStateInformation) takes precedence later if the plugin was
    // instantiated from a saved DAW project.
    applyUserMidiChannels();
}

// Route MIDI CC messages to per-group params per the manual:
//   - CC on a group's MIDI channel  -> that group's param/macro.
//   - CC on the preset MIDI channel -> the focused group only.
// Groups can share channels (one CC then targets all matching groups).
// Updates each target group's PatchState directly; if the focused group is
// among the targets, also pushes the value through APVTS so host
// automation and per-block APVTS->PatchState sync stay consistent.
// (CC1, CC120, CC123, channel/poly aftertouch are handled by the engine
//  as global broadcasts and are not consumed here.)
void TidesAudioProcessor::routeParamCCs (juce::MidiBuffer& midi)
{
    using namespace tides;

    const int focused = presetState.focused.load();
    auto markTargets = [&] (int ch, std::array<bool, state::kMaxGroups>& tg)
    {
        if (ch == presetState.presetChannel.load())
        {
            tg[(size_t) focused] = true;
            return;
        }
        for (int g = 0; g < state::kMaxGroups; ++g)
            if (presetState.groupChannel[g].load() == ch)
                tg[(size_t) g] = true;
    };

    for (const auto meta : midi)
    {
        const auto& m = meta.getMessage();
        if (! m.isController()) continue;
        const int cc  = m.getControllerNumber();
        if (cc == 1 || cc == 120 || cc == 123) continue;     // engine handles
        const int val = m.getControllerValue();

        std::array<bool, state::kMaxGroups> targets {};
        markTargets (m.getChannel(), targets);
        bool any = false;
        for (bool b : targets) if (b) { any = true; break; }
        if (! any) continue;

        // Continuous params: lookup CC in descriptor table.
        params::P matched = params::P::NumParams;
        for (int i = 0; i < params::kNumParams; ++i)
        {
            const auto pp = static_cast<params::P> (i);
            if (params::descriptor (pp).cc == cc) { matched = pp; break; }
        }
        if (matched != params::P::NumParams)
        {
            const float v = (float) val / 127.0f;
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (targets[(size_t) g]) presetState.group[(size_t) g].set (matched, v);
            if (targets[(size_t) focused])
                if (auto* p = apvts.getParameter (params::descriptor (matched).id))
                    p->setValueNotifyingHost (p->convertTo0to1 (v));
            continue;
        }

        // Source macros: CC 102..110 -> macros 0..8 (bipolar, 64 = center).
        if (cc >= 102 && cc < 102 + mod::kNumSources)
        {
            const int s = cc - 102;
            const float v = juce::jlimit (-1.0f, 1.0f, ((float) val - 64.0f) / 63.0f);
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (targets[(size_t) g])
                    presetState.group[(size_t) g].sourceMacro[(size_t) s].store (v);
            if (targets[(size_t) focused])
                if (auto* p = apvts.getParameter (juce::String ("macro_") + juce::String (s)))
                    p->setValueNotifyingHost (p->convertTo0to1 (v));
        }
    }
}

void TidesAudioProcessor::pushFocusedGroupToAPVTS()
{
    using namespace tides::params;
    const int g = presetState.focused.load();
    auto& fg = presetState.group[(size_t) g];

    // (Earlier phases seeded `fg.numConnections.store(4)` here for placeholder
    // routing rows — removed now that patch-mode actually creates real ones.)

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto pp = static_cast<P> (i);
        if (auto* p = apvts.getParameter (descriptor (pp).id))
            p->setValueNotifyingHost (p->convertTo0to1 (fg.get (pp)));
    }
    for (int i = 0; i < 9; ++i)
        if (auto* p = apvts.getParameter (macroId (i)))
            p->setValueNotifyingHost (p->convertTo0to1 (
                fg.sourceMacro[(size_t) i].load()));

    pendingFocusSync.store (false);
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
    extMono.assign((size_t) juce::jmax(1, samplesPerBlock), 0.0f);
}

bool TidesAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo()
        && out != juce::AudioChannelSet::mono())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled()
        || in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

void TidesAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;   // critical in a feedback loop

    // ---- APVTS -> PatchState (focused group) ----
    // Focus switch is gated: while pendingFocusSync is true, the audio
    // thread skips writes (otherwise stale APVTS values would corrupt the
    // newly-focused group before the message-thread sync repopulates them).
    if (focusedParamPtr != nullptr)
    {
        const int fgIdx = (int) focusedParamPtr->load();
        if (fgIdx != presetState.focused.load())
        {
            presetState.focused.store (fgIdx);
            pendingFocusSync.store (true);
            juce::MessageManager::callAsync ([this] { pushFocusedGroupToAPVTS(); });
        }
    }
    if (! pendingFocusSync.load())
    {
        using namespace tides::params;
        auto& fg = presetState.group[(size_t) presetState.focused.load()];
        for (int i = 0; i < kNumParams; ++i)
            if (auto* ap = contParamPtr[(size_t) i])
                fg.set (static_cast<P> (i), ap->load());
        for (int i = 0; i < 9; ++i)
            if (auto* ap = macroParamPtr[(size_t) i])
                fg.sourceMacro[(size_t) i].store (ap->load());
    }

    const int n = buffer.getNumSamples();

    // Mono-mix the Ext In bus before the engine clears the output.
    auto inBus = getBusBuffer (buffer, true, 0);
    const int ic = inBus.getNumChannels();
    if ((int) extMono.size() < n) extMono.assign ((size_t) n, 0.0f);
    float extPk = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float m = ic > 0 ? inBus.getReadPointer (0)[i] : 0.0f;
        if (ic > 1) m = 0.5f * (m + inBus.getReadPointer (1)[i]);
        extMono[(size_t) i] = m;
        const float a = std::abs (m);
        if (a > extPk) extPk = a;
    }
    // Combine with smoothed previous peak so it decays between blocks.
    auto& snapEx = engine.snapshot().extInPeak;
    const float prev = snapEx.load (std::memory_order_relaxed) * 0.85f;
    snapEx.store (extPk > prev ? extPk : prev, std::memory_order_relaxed);

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    routeParamCCs (midi);    // per-group param CC routing before engine

    // Preset morph: blend focused-group continuous params from cacheA → cacheB
    // by morphPos. Runs *after* CC routing so live param edits via CC still
    // pass through, and *after* APVTS sync so the user can also drive it
    // from the host. With morphPos = 0 this is a no-op.
    applyMorphToFocusedGroup();

    // Preset/group loads ask us to drop active notes here so the new
    // routing doesn't strand voices.
    if (pendingVoiceKill.exchange (false))
        midi.addEvent (juce::MidiMessage::allNotesOff (1),  0);   // engine kill path

    auto outBus = getBusBuffer (buffer, false, 0);
    engine.processBlock (outBus, midi, presetState, extMono.data(), n);
}

// ---- state persistence ---------------------------------------------------
// Full patch: continuous params (by stable string id), discrete switches,
// the four LFOs and the modulation matrix. Live performance inputs
// (modwheel/aftertouch) are transient and intentionally not stored.

static juce::ValueTree patchToTree (const tides::state::PatchState& p)
{
    using namespace tides;
    juce::ValueTree root ("TidesState");
    root.setProperty ("version", 1, nullptr);

    juce::ValueTree pv ("Params");
    for (int i = 0; i < params::kNumParams; ++i)
    {
        const auto pp = static_cast<params::P> (i);
        pv.setProperty (juce::Identifier (params::descriptor (pp).id),
                        p.get (pp), nullptr);
    }
    root.addChild (pv, -1, nullptr);

    juce::ValueTree sw ("Switches");
    sw.setProperty ("envRange",     p.envRange.load(),     nullptr);
    sw.setProperty ("envHold",      p.envHold.load(),      nullptr);
    sw.setProperty ("envLegato",    p.envLegato.load(),    nullptr);
    sw.setProperty ("envLoop",      p.envLoop.load(),      nullptr);
    sw.setProperty ("kbReset",      p.kbReset.load(),      nullptr);
    sw.setProperty ("resInvert",    p.resInvert.load(),    nullptr);
    sw.setProperty ("keyFollowVco", p.keyFollowVco.load(), nullptr);
    sw.setProperty ("extIn",        p.extIn.load(),        nullptr);
    sw.setProperty ("resNoteOffset",p.resNoteOffset.load(),nullptr);
    sw.setProperty ("uniVoices",    p.unisonVoices.load(), nullptr);
    sw.setProperty ("uniDetune",    p.unisonDetune.load(), nullptr);
    sw.setProperty ("uniSpread",    p.unisonSpread.load(), nullptr);
    root.addChild (sw, -1, nullptr);

    for (int l = 0; l < 4; ++l)
    {
        const auto& L = p.lfo[(size_t) l];
        juce::ValueTree lt ("LFO");
        lt.setProperty ("i",    l,                   nullptr);
        lt.setProperty ("rate", L.rate.load(),       nullptr);
        lt.setProperty ("rnd",  L.randomRate.load(), nullptr);
        lt.setProperty ("amp",  L.amplitude.load(),  nullptr);
        lt.setProperty ("mix",  L.mix.load(),        nullptr);
        lt.setProperty ("wave", L.wave.load(),       nullptr);
        lt.setProperty ("bip",  L.bipolar.load(),    nullptr);
        lt.setProperty ("smo",  L.smooth.load(),     nullptr);
        lt.setProperty ("kbr",  L.kbReset.load(),    nullptr);
        root.addChild (lt, -1, nullptr);
    }

    juce::ValueTree md ("Mods");
    const int n = p.numConnections.load();
    md.setProperty ("n", n, nullptr);
    for (int c = 0; c < n; ++c)
    {
        const auto& cc = p.connections[(size_t) c];
        juce::ValueTree ct ("C");
        ct.setProperty ("s",  (int) cc.source, nullptr);
        ct.setProperty ("d",  (int) cc.dest,   nullptr);
        ct.setProperty ("dp", cc.depth,        nullptr);
        ct.setProperty ("en", cc.enabled,      nullptr);
        ct.setProperty ("mu", cc.muted,        nullptr);
        ct.setProperty ("dd", (int) cc.depthDir, nullptr);
        md.addChild (ct, -1, nullptr);
    }
    for (int s = 0; s < mod::kNumSources; ++s)
    {
        md.setProperty ("macro" + juce::String (s),
                        p.sourceMacro[(size_t) s].load(), nullptr);
        md.setProperty ("det" + juce::String (s),
                        p.sourceDetached[(size_t) s].load(), nullptr);
    }
    root.addChild (md, -1, nullptr);
    return root;
}

static void treeToPatch (const juce::ValueTree& root, tides::state::PatchState& p)
{
    using namespace tides;
    p.resetToInit();   // defaults for anything missing / version drift
    if (! root.hasType ("TidesState")) return;

    auto pv = root.getChildWithName ("Params");
    for (int i = 0; i < params::kNumParams; ++i)
    {
        const auto pp = static_cast<params::P> (i);
        const juce::Identifier id (params::descriptor (pp).id);
        if (pv.hasProperty (id)) p.set (pp, (float) pv.getProperty (id));
    }

    auto sw = root.getChildWithName ("Switches");
    if (sw.isValid())
    {
        p.envRange     .store ((int)  sw.getProperty ("envRange",     1));
        p.envHold      .store ((bool) sw.getProperty ("envHold",      false));
        p.envLegato    .store ((bool) sw.getProperty ("envLegato",    false));
        p.envLoop      .store ((bool) sw.getProperty ("envLoop",      false));
        p.kbReset      .store ((bool) sw.getProperty ("kbReset",      false));
        p.resInvert    .store ((bool) sw.getProperty ("resInvert",    false));
        p.keyFollowVco .store ((bool) sw.getProperty ("keyFollowVco", true));
        p.extIn        .store ((bool) sw.getProperty ("extIn", false));
        p.resNoteOffset.store ((int)  sw.getProperty ("resNoteOffset",0));
        p.unisonVoices .store ((int)  sw.getProperty ("uniVoices",  1));
        p.unisonDetune .store ((float)sw.getProperty ("uniDetune",  0.3f));
        p.unisonSpread .store ((float)sw.getProperty ("uniSpread",  0.5f));
    }

    for (auto lt : root)
    {
        if (! lt.hasType ("LFO")) continue;
        const int l = (int) lt.getProperty ("i", -1);
        if (l < 0 || l > 3) continue;
        auto& L = p.lfo[(size_t) l];
        L.rate      .store ((float) lt.getProperty ("rate", 0.25f));
        L.randomRate.store ((float) lt.getProperty ("rnd",  0.0f));
        L.amplitude .store ((float) lt.getProperty ("amp",  1.0f));
        L.mix       .store ((float) lt.getProperty ("mix",  1.0f));
        L.wave      .store ((int)   lt.getProperty ("wave", 0));
        L.bipolar   .store ((bool)  lt.getProperty ("bip",  true));
        // Backward-compat: old presets stored a bool here; juce::var bool
        // converts cleanly to 0.0f/1.0f via getProperty<float>.
        L.smooth    .store ((float) lt.getProperty ("smo",  0.0f));
        L.kbReset   .store ((bool)  lt.getProperty ("kbr",  false));
    }

    auto md = root.getChildWithName ("Mods");
    if (md.isValid())
    {
        int idx = 0;
        for (auto ct : md)
        {
            if (! ct.hasType ("C") || idx >= mod::kMaxConnections) continue;
            auto& cc = p.connections[(size_t) idx++];
            cc.source  = static_cast<mod::Source> ((int) ct.getProperty ("s", 0));
            cc.dest    = static_cast<mod::Dest>   ((int) ct.getProperty ("d", 0));
            cc.depth   = (float) ct.getProperty ("dp", 0.0f);
            cc.enabled = (bool)  ct.getProperty ("en", false);
            cc.muted   = (bool)  ct.getProperty ("mu", false);
            cc.depthDir = (int8_t) (int) ct.getProperty ("dd", cc.depth >= 0.0f ? 1 : -1);
        }
        p.numConnections.store ((int) md.getProperty ("n", idx));
        for (int s = 0; s < mod::kNumSources; ++s)
        {
            p.sourceMacro[(size_t) s].store (
                (float) md.getProperty ("macro" + juce::String (s), 0.0f));
            p.sourceDetached[(size_t) s].store (
                (bool) md.getProperty ("det" + juce::String (s), false));
        }
    }
}

static juce::ValueTree presetToTree (const tides::state::Preset& pr)
{
    juce::ValueTree root ("TidesPreset");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("focused",  pr.focused.load(),       nullptr);
    root.setProperty ("psChan",   pr.presetChannel.load(), nullptr);
    root.setProperty ("psUni",    pr.presetUnison.load(),  nullptr);
    root.setProperty ("psPrio",   pr.presetPriority.load(),nullptr);
    root.setProperty ("psQual",   pr.qualityMode.load(),   nullptr);
    root.setProperty ("psMpe",    pr.mpeMode.load(),       nullptr);
    root.setProperty ("psMpeBnd", pr.mpeBendSemis.load(),  nullptr);
    // Morph endpoint selection so the picked A/B survive across sessions.
    root.setProperty ("morphA",     pr.morphA.load(),     nullptr);
    root.setProperty ("morphB",     pr.morphB.load(),     nullptr);
    root.setProperty ("morphAKind", pr.morphAKind.load(), nullptr);
    root.setProperty ("morphBKind", pr.morphBKind.load(), nullptr);
    root.setProperty ("morphAUser", pr.morphAUser,        nullptr);
    root.setProperty ("morphBUser", pr.morphBUser,        nullptr);
    for (int g = 0; g < tides::state::kMaxGroups; ++g)
    {
        juce::ValueTree gt ("Group");
        gt.setProperty ("i",  g, nullptr);
        gt.setProperty ("ch", pr.groupChannel[g].load(),  nullptr);
        gt.setProperty ("py", pr.groupPoly[g].load(),     nullptr);
        gt.setProperty ("pr", pr.groupPriority[g].load(), nullptr);
        gt.setProperty ("nm", pr.groupNames[g],           nullptr);
        gt.addChild (patchToTree (pr.group[(size_t) g]), -1, nullptr);
        root.addChild (gt, -1, nullptr);
    }
    return root;
}

static void treeToPreset (const juce::ValueTree& root, tides::state::Preset& pr)
{
    pr.initRouting();
    for (auto& g : pr.group) g.resetToInit();
    if (! root.hasType ("TidesPreset")) return;

    pr.focused.store       ((int)  root.getProperty ("focused", 0));
    pr.presetChannel.store ((int)  root.getProperty ("psChan", 16));
    pr.presetUnison.store  ((bool) root.getProperty ("psUni", false));
    pr.presetPriority.store((int)  root.getProperty ("psPrio", 0));
    pr.qualityMode.store   ((int)  root.getProperty ("psQual",
                                       (int) tides::state::Quality::Std));
    pr.mpeMode.store       ((bool) root.getProperty ("psMpe",    false));
    pr.mpeBendSemis.store  ((float) root.getProperty ("psMpeBnd", 48.0f));
    pr.morphA.store     ((int)  root.getProperty ("morphA",     0));
    pr.morphB.store     ((int)  root.getProperty ("morphB",     1));
    pr.morphAKind.store ((int)  root.getProperty ("morphAKind", 0));
    pr.morphBKind.store ((int)  root.getProperty ("morphBKind", 0));
    pr.morphAUser =        root.getProperty ("morphAUser", juce::String()).toString();
    pr.morphBUser =        root.getProperty ("morphBUser", juce::String()).toString();
    for (auto gt : root)
    {
        if (! gt.hasType ("Group")) continue;
        const int g = (int) gt.getProperty ("i", -1);
        if (g < 0 || g >= tides::state::kMaxGroups) continue;
        pr.groupChannel[g] .store ((int) gt.getProperty ("ch", g + 1));
        pr.groupPoly[g]    .store ((int) gt.getProperty ("py", g == 0 ? 16 : 0));
        pr.groupPriority[g].store ((int) gt.getProperty ("pr", 0));
        if (gt.hasProperty ("nm"))
            pr.groupNames[g] = gt.getProperty ("nm").toString();
        auto ps = gt.getChildWithName ("TidesState");
        if (ps.isValid()) treeToPatch (ps, pr.group[(size_t) g]);
    }
}

// Deep-copy a Preset by round-tripping through the existing serialiser.
// Slow but correct; only called on preset load / A/B toggle (message thread,
// not RT-critical).
static void copyPreset (const tides::state::Preset& src, tides::state::Preset& dst)
{
    auto tree = presetToTree (src);
    treeToPreset (tree, dst);
}

void TidesAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos (destData, false);
    presetToTree (presetState).writeToStream (mos);
}

void TidesAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! tree.isValid()) return;

    // Gate the audio-thread APVTS->PatchState sync while we swap state,
    // so stale APVTS values don't overwrite the freshly-loaded focused group.
    pendingFocusSync.store (true);
    treeToPreset (tree, presetState);

    // Rebuild morph endpoint caches from the restored kind/index/user name.
    auto rebuildMorphCache = [this] (bool isA)
    {
        const auto kind = isA ? (MorphKind) presetState.morphAKind.load()
                              : (MorphKind) presetState.morphBKind.load();
        if (kind == MorphKind::Factory)
        {
            if (isA) setMorphEndpointFactoryA (presetState.morphA.load());
            else     setMorphEndpointFactoryB (presetState.morphB.load());
        }
        else
        {
            const auto name = isA ? presetState.morphAUser : presetState.morphBUser;
            auto file = userPresetDirectory().getChildFile (name + ".tides");
            if (file.existsAsFile())
            {
                if (isA) setMorphEndpointUserA (file);
                else     setMorphEndpointUserB (file);
            }
        }
    };
    rebuildMorphCache (true);
    rebuildMorphCache (false);

    // MIDI channels are a global user preference, not per-project state, so
    // the last globally-saved choices always win over whatever the host
    // just handed us. Setting a new channel in this project will still
    // persist to the global settings via setGroupMidiChannel().
    applyUserMidiChannels();

    // Capture the just-loaded preset as the saved baseline and as the
    // starting edit state. Subsequent edits diverge presetState from
    // baselineState; A/B toggle compares the two via editState.
    copyPreset (presetState, baselineState);
    copyPreset (presetState, editState);
    showingSaved.store (false);

    if (auto* fgp = apvts.getParameter ("focused_group"))
        fgp->setValueNotifyingHost (
            fgp->convertTo0to1 ((float) presetState.focused.load()));

    pushFocusedGroupToAPVTS();   // clears the gate + bumps epoch
}

void TidesAudioProcessor::toggleCompare()
{
    pendingFocusSync.store (true);          // gate audio-thread sync
    if (! showingSaved.load())
    {
        copyPreset (presetState,   editState);     // remember pre-press edits
        copyPreset (baselineState, presetState);   // hear saved
        showingSaved.store (true);
    }
    else
    {
        copyPreset (editState, presetState);       // restore edits
        showingSaved.store (false);
    }
    pushFocusedGroupToAPVTS();   // mirror live group to APVTS + bump epoch
}

// ─── Mod-matrix editing (UI-thread only) ───────────────────────────────
void TidesAudioProcessor::assignConnection (tides::mod::Dest dest, float depthStep)
{
    if (! patchMode.isArmed()) return;
    const int src = patchMode.armedSource();
    if (src < 0 || src >= tides::mod::kNumSources) { patchMode.disarm(); return; }
    if ((int) dest < 0 || (int) dest >= tides::mod::kNumDests) { patchMode.disarm(); return; }

    auto& p = patch();
    const int n = p.numConnections.load();

    // If a routing for this src→dest already exists, bump its depth in the
    // current direction. Hitting a boundary flips the direction so a steady
    // stream of clicks traces a triangle through the full ±1 range rather
    // than saturating at +1 with no way back.
    for (int i = 0; i < n; ++i)
    {
        auto& c = p.connections[(size_t) i];
        if ((int) c.source == src && c.dest == dest)
        {
            const float step    = depthStep * (float) c.depthDir;
            float       newDepth = c.depth + step;
            if (newDepth >= 1.0f)       { newDepth =  1.0f; c.depthDir = -1; }
            else if (newDepth <= -1.0f) { newDepth = -1.0f; c.depthDir = +1; }
            c.depth   = newDepth;
            c.enabled = true;
            c.muted   = false;
            patchMode.disarm();
            stateEpoch.fetch_add (1);
            return;
        }
    }

    // Otherwise append a new connection.
    if (n < tides::mod::kMaxConnections)
    {
        auto& c = p.connections[(size_t) n];
        c.source  = (tides::mod::Source) src;
        c.dest    = dest;
        c.depth   = juce::jlimit (-1.0f, 1.0f, depthStep);
        c.enabled = true;
        c.muted   = false;
        p.numConnections.store (n + 1, std::memory_order_release);
        stateEpoch.fetch_add (1);
    }
    patchMode.disarm();
}

void TidesAudioProcessor::removeConnection (int index)
{
    auto& p = patch();
    const int n = p.numConnections.load();
    if (index < 0 || index >= n) return;

    // Shift down to keep the active prefix contiguous.
    for (int i = index; i < n - 1; ++i)
        p.connections[(size_t) i] = p.connections[(size_t) (i + 1)];
    p.connections[(size_t) (n - 1)] = {};
    p.numConnections.store (n - 1, std::memory_order_release);
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::toggleConnectionMuted (int index)
{
    auto& p = patch();
    const int n = p.numConnections.load();
    if (index < 0 || index >= n) return;
    p.connections[(size_t) index].muted ^= true;
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::toggleConnectionEnabled (int index)
{
    auto& p = patch();
    const int n = p.numConnections.load();
    if (index < 0 || index >= n) return;
    p.connections[(size_t) index].enabled ^= true;
    stateEpoch.fetch_add (1);
}

// ─── Preset morph ──────────────────────────────────────────────────────
// Build a factory preset into a sacrificial Preset, then copy its group-0
// PatchState into the morph cache slot. Cache lives in presetState so
// per-block lerp is a few hundred float copies — well within budget.
static void buildMorphCache (int presetIdx, tides::state::PatchState& dst)
{
    tides::state::Preset temp;
    tides::state::factory::loadIntoPreset (presetIdx, temp);
    tides::state::factory::copyPatchState (temp.group[0], dst);
}

// User preset → PatchState. Reads the XML file the same way loadUserPreset()
// does but populates a supplied cache slot instead of the live focused group.
static bool buildUserMorphCache (const juce::File& file,
                                 tides::state::PatchState& dst)
{
    if (! file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TidesUserPreset")) return false;
    auto patchTree = root.getChildWithName ("TidesState");
    if (! patchTree.isValid()) return false;
    treeToPatch (patchTree, dst);
    return true;
}

void TidesAudioProcessor::setMorphEndpointFactoryA (int idx)
{
    const int n = tides::state::factory::count();
    if (n <= 0) return;
    const int safe = juce::jlimit (0, n - 1, idx);
    presetState.morphA.store (safe);
    presetState.morphAKind.store ((int) MorphKind::Factory);
    presetState.morphAUser = {};
    buildMorphCache (safe, presetState.morphCacheA);
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::setMorphEndpointFactoryB (int idx)
{
    const int n = tides::state::factory::count();
    if (n <= 0) return;
    const int safe = juce::jlimit (0, n - 1, idx);
    presetState.morphB.store (safe);
    presetState.morphBKind.store ((int) MorphKind::Factory);
    presetState.morphBUser = {};
    buildMorphCache (safe, presetState.morphCacheB);
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::setMorphEndpointUserA (const juce::File& file)
{
    if (! buildUserMorphCache (file, presetState.morphCacheA)) return;
    presetState.morphAKind.store ((int) MorphKind::User);
    presetState.morphAUser = file.getFileNameWithoutExtension();
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::setMorphEndpointUserB (const juce::File& file)
{
    if (! buildUserMorphCache (file, presetState.morphCacheB)) return;
    presetState.morphBKind.store ((int) MorphKind::User);
    presetState.morphBUser = file.getFileNameWithoutExtension();
    stateEpoch.fetch_add (1);
}

// Per-block crossfade. Continuous params + LFO floats + macros lerp.
// Discrete params (wave, env range, switches) snap at 0.5. Mod
// connections stay as the user has them on the focused group, so a
// performer's own routing isn't blown away by the morph.
void TidesAudioProcessor::applyMorphToFocusedGroup()
{
    using namespace tides::params;
    using tides::state::PatchState;

    if (morphParamPtr == nullptr) return;

    // Modulation offset on the morph crossfade: sum the focused group's
    // routes that target Dest::Morph, multiplying each by its source's
    // current live value (from the engine snapshot) and the source macro.
    // Sources can therefore sweep morph in real time.
    const auto& focusedPatch = presetState.group[(size_t) presetState.focused.load()];
    const auto& snap = engine.snapshot();
    float morphMod = 0.0f;
    const int conn = focusedPatch.numConnections.load();
    for (int i = 0; i < conn; ++i)
    {
        const auto& c = focusedPatch.connections[(size_t) i];
        if (c.dest != tides::mod::Dest::Morph || ! c.enabled || c.muted) continue;
        const int s = (int) c.source;
        if (s < 0 || s >= (int) snap.liveSources.size()) continue;
        const float macro = focusedPatch.sourceMacro[(size_t) s].load();
        morphMod += snap.liveSources[(size_t) s].load() * macro * c.depth;
    }
    const float t = juce::jlimit (0.0f, 1.0f, morphParamPtr->load() + morphMod);
    if (t <= 0.0001f) return;   // 0 = pure A = whatever the user has; no-op

    auto& a = presetState.morphCacheA;
    auto& b = presetState.morphCacheB;
    auto& d = presetState.group[(size_t) presetState.focused.load()];

    auto lerpF = [t] (float va, float vb) { return va + (vb - va) * t; };

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto p = static_cast<P> (i);
        d.set (p, lerpF (a.get (p), b.get (p)));
    }

    // Discrete switches & ints snap at 0.5.
    auto pickI = [t] (int va, int vb)  { return t < 0.5f ? va : vb; };
    auto pickB = [t] (bool va, bool vb){ return t < 0.5f ? va : vb; };

    d.envRange    .store (pickI (a.envRange    .load(), b.envRange    .load()));
    d.envHold     .store (pickB (a.envHold     .load(), b.envHold     .load()));
    d.envLegato   .store (pickB (a.envLegato   .load(), b.envLegato   .load()));
    d.envLoop     .store (pickB (a.envLoop     .load(), b.envLoop     .load()));
    d.kbReset     .store (pickB (a.kbReset     .load(), b.kbReset     .load()));
    d.resInvert   .store (pickB (a.resInvert   .load(), b.resInvert   .load()));
    d.resNoteOffset.store(pickI (a.resNoteOffset.load(), b.resNoteOffset.load()));
    d.keyFollowVco.store (pickB (a.keyFollowVco.load(), b.keyFollowVco.load()));
    d.extIn       .store (pickB (a.extIn       .load(), b.extIn       .load()));

    // Unison voices interpolates + rounds; detune/spread are floats.
    const float uvLerp = lerpF ((float) a.unisonVoices.load(),
                                 (float) b.unisonVoices.load());
    d.unisonVoices.store ((int) std::round (uvLerp));
    d.unisonDetune.store (lerpF (a.unisonDetune.load(), b.unisonDetune.load()));
    d.unisonSpread.store (lerpF (a.unisonSpread.load(), b.unisonSpread.load()));

    // LFOs — continuous floats lerp, wave/bipolar/kbReset snap.
    for (int i = 0; i < (int) a.lfo.size(); ++i)
    {
        auto& la = a.lfo[(size_t) i];
        auto& lb = b.lfo[(size_t) i];
        auto& ld = d.lfo[(size_t) i];
        ld.rate      .store (lerpF (la.rate      .load(), lb.rate      .load()));
        ld.randomRate.store (lerpF (la.randomRate.load(), lb.randomRate.load()));
        ld.amplitude .store (lerpF (la.amplitude .load(), lb.amplitude .load()));
        ld.mix       .store (lerpF (la.mix       .load(), lb.mix       .load()));
        ld.smooth    .store (lerpF (la.smooth    .load(), lb.smooth    .load()));
        ld.wave      .store (pickI (la.wave      .load(), lb.wave      .load()));
        ld.bipolar   .store (pickB (la.bipolar   .load(), lb.bipolar   .load()));
        ld.kbReset   .store (pickB (la.kbReset   .load(), lb.kbReset   .load()));
    }

    // Source macros lerp; detached flag snaps.
    for (int i = 0; i < (int) a.sourceMacro.size(); ++i)
    {
        d.sourceMacro   [(size_t) i].store (lerpF (
            a.sourceMacro   [(size_t) i].load(),
            b.sourceMacro   [(size_t) i].load()));
        d.sourceDetached[(size_t) i].store (pickB (
            a.sourceDetached[(size_t) i].load(),
            b.sourceDetached[(size_t) i].load()));
    }
    // Mod-matrix connections are intentionally NOT morphed — the user's
    // performance routings stay in place across the morph.
}

// ─── INIT patch ────────────────────────────────────────────────────────
// Hard-blanks the whole rack: resets every group and focuses G1 with a
// sine-into-resonator starting point, so INIT is a true "from scratch" —
// no stray voice groups from a previous preset still active.
void TidesAudioProcessor::loadInitPatch()
{
    using namespace tides::params;
    pendingFocusSync.store (true);
    pendingVoiceKill.store (true);

    // Force focus to G1 so INIT lands exactly where the user expects.
    presetState.focused.store (0);
    if (auto* fgp = apvts.getParameter ("focused_group"))
        fgp->setValueNotifyingHost (fgp->convertTo0to1 (0.0f));

    const int focused = presetState.focused.load();

    // Wipe every group; only G1 gets audible defaults below. Other groups
    // stay silent with plain "G·N" labels so the group bar clearly shows
    // "only G1 is loaded" on INIT.
    for (int gi = 0; gi < tides::state::kMaxGroups; ++gi)
    {
        presetState.group[(size_t) gi].resetToInit();
        presetState.groupNames[gi]    = juce::String::fromUTF8 ("G·")
                                        + juce::String (gi + 1);
        presetState.groupChannel [gi].store (gi + 1);
        presetState.groupPoly    [gi].store (gi == focused ? 16 : 0);
        presetState.groupPriority[gi].store (0);
    }
    // Restore any user-saved MIDI channel preferences over the INIT defaults.
    applyUserMidiChannels();

    auto& g = presetState.group[(size_t) focused];

    // Sensible neutral starting point.
    g.set (P::ExWave,         0.0f);   // sine
    g.set (P::ExCoarseFreq,   0.5f);   // 0 oct
    g.set (P::ExFineFreq,     0.5f);   // 0 st
    g.set (P::ExWaveNoiseMix, 0.0f);   // pure wave
    g.set (P::ExNoiseSHRate,  0.5f);
    g.set (P::ExLevel,        0.7f);

    g.set (P::EnvOnsetDelay,  0.0f);
    g.set (P::EnvAttack,      0.05f);
    g.set (P::EnvDecay,       0.30f);
    g.set (P::EnvSustain,     0.70f);
    g.set (P::EnvRelease,     0.25f);

    g.set (P::ResInputFilter,    0.80f);
    g.set (P::ResFeedbackFilter, 0.50f);
    g.set (P::ResFeedback,       0.0f);     // clean, no resonator character
    g.set (P::ResFineTune,       0.5f);
    g.set (P::ResGlide,          0.0f);

    g.set (P::OutDryWet,  0.0f);    // pure exciter; turn up for resonator
    g.set (P::OutPanning, 0.5f);
    g.set (P::OutLevel,   0.7f);
    g.set (P::WildTame,   0.0f);    // fully clean
    g.set (P::Master,     0.8f);

    presetState.groupNames[focused]      = "INIT";
    presetState.groupPoly    [focused].store (16);
    presetState.groupPriority[focused].store (0);

    // Mirror the whole rack into baseline so A/B compares against this init.
    {
        tides::state::Preset& src = presetState;
        tides::state::Preset& dst = baselineState;
        for (int gi = 0; gi < tides::state::kMaxGroups; ++gi)
        {
            for (int i = 0; i < tides::params::kNumParams; ++i)
                dst.group[(size_t) gi].set (static_cast<P> (i),
                    src.group[(size_t) gi].get (static_cast<P> (i)));
            dst.groupNames   [gi]     = src.groupNames   [gi];
            dst.groupChannel [gi].store (src.groupChannel [gi].load());
            dst.groupPoly    [gi].store (src.groupPoly    [gi].load());
            dst.groupPriority[gi].store (src.groupPriority[gi].load());
        }
    }
    showingSaved.store (false);

    curPresetIndex .store (0);
    userPresetName  = juce::String();
    initLoaded     .store (true);
    pushFocusedGroupToAPVTS();
}

// ─── Global settings (MIDI channels) ───────────────────────────────────
// Per-group + preset MIDI channel choices are stored globally so a fresh
// plugin instance (or a new host project) starts with the user's last
// picks instead of the INIT defaults. Kept in a tiny XML at
// ~/Library/Application Support/Tides/settings.xml.
juce::File TidesAudioProcessor::settingsFile()
{
    auto dir = juce::File::getSpecialLocation (
                  juce::File::userApplicationDataDirectory)
                  .getChildFile ("Tides");
    if (! dir.exists()) dir.createDirectory();
    return dir.getChildFile ("settings.xml");
}

void TidesAudioProcessor::applyUserMidiChannels()
{
    auto file = settingsFile();
    if (! file.existsAsFile()) return;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TidesSettings")) return;

    auto midi = root.getChildWithName ("MidiChannels");
    if (! midi.isValid()) return;
    for (int g = 0; g < tides::state::kMaxGroups; ++g)
    {
        const juce::Identifier id (juce::String ("g") + juce::String (g));
        if (midi.hasProperty (id))
        {
            const int ch = juce::jlimit (1, 16, (int) midi.getProperty (id));
            presetState.groupChannel[g].store (ch);
        }
    }
    if (midi.hasProperty ("preset"))
    {
        const int ch = juce::jlimit (1, 16, (int) midi.getProperty ("preset"));
        presetState.presetChannel.store (ch);
    }
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::saveUserMidiChannels() const
{
    juce::ValueTree root ("TidesSettings");
    root.setProperty ("version", 1, nullptr);

    juce::ValueTree midi ("MidiChannels");
    for (int g = 0; g < tides::state::kMaxGroups; ++g)
        midi.setProperty (juce::Identifier (juce::String ("g") + juce::String (g)),
                          presetState.groupChannel[g].load(), nullptr);
    midi.setProperty ("preset", presetState.presetChannel.load(), nullptr);
    root.addChild (midi, -1, nullptr);

    auto xml = root.createXml();
    if (xml == nullptr) return;
    xml->writeTo (settingsFile());
}

void TidesAudioProcessor::setGroupMidiChannel (int group, int channel)
{
    if (group < 0 || group >= tides::state::kMaxGroups) return;
    const int ch = juce::jlimit (1, 16, channel);
    if (presetState.groupChannel[group].load() == ch) return;
    presetState.groupChannel[group].store (ch);
    saveUserMidiChannels();
    stateEpoch.fetch_add (1);
}

void TidesAudioProcessor::setPresetMidiChannel (int channel)
{
    const int ch = juce::jlimit (1, 16, channel);
    if (presetState.presetChannel.load() == ch) return;
    presetState.presetChannel.store (ch);
    saveUserMidiChannels();
    stateEpoch.fetch_add (1);
}

// ─── User preset I/O ───────────────────────────────────────────────────
juce::File TidesAudioProcessor::userPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (
                  juce::File::userApplicationDataDirectory)
                  .getChildFile ("Tides")
                  .getChildFile ("Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

bool TidesAudioProcessor::saveCurrentAsUserPreset (const juce::String& name)
{
    auto safeName = name.trim();
    if (safeName.isEmpty()) return false;

    const int focused = presetState.focused.load();
    juce::ValueTree root ("TidesUserPreset");
    root.setProperty ("version", 1,            nullptr);
    root.setProperty ("name",    safeName,     nullptr);
    root.setProperty ("groupName",
                       presetState.groupNames[focused], nullptr);
    root.setProperty ("groupChannel",
                       presetState.groupChannel[focused].load(),  nullptr);
    root.setProperty ("groupPoly",
                       presetState.groupPoly[focused].load(),     nullptr);
    root.setProperty ("groupPriority",
                       presetState.groupPriority[focused].load(), nullptr);
    root.addChild (patchToTree (presetState.group[(size_t) focused]), -1, nullptr);

    auto file = userPresetDirectory()
                  .getChildFile (safeName + ".tides");
    auto xml = root.createXml();
    if (xml == nullptr) return false;
    if (! xml->writeTo (file)) return false;

    userPresetName = safeName;
    curPresetIndex.store (-1);   // disambiguates "loaded user, not factory"
    stateEpoch.fetch_add (1);
    return true;
}

bool TidesAudioProcessor::loadUserPreset (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("TidesUserPreset")) return false;

    // patchToTree() produces a child with type "TidesState" — that's what
    // we serialise into the user preset file.
    auto patchTree = root.getChildWithName ("TidesState");
    if (! patchTree.isValid()) return false;

    pendingFocusSync.store (true);
    pendingVoiceKill.store (true);

    const int focused = presetState.focused.load();
    treeToPatch (patchTree, presetState.group[(size_t) focused]);

    if (root.hasProperty ("groupName"))
        presetState.groupNames[focused] = root.getProperty ("groupName").toString();
    if (root.hasProperty ("groupChannel"))
        presetState.groupChannel [focused].store ((int) root.getProperty ("groupChannel"));
    if (root.hasProperty ("groupPoly"))
        presetState.groupPoly    [focused].store ((int) root.getProperty ("groupPoly"));
    if (root.hasProperty ("groupPriority"))
        presetState.groupPriority[focused].store ((int) root.getProperty ("groupPriority"));

    // Global MIDI channel prefs win over the preset's stored channel.
    applyUserMidiChannels();

    userPresetName = root.hasProperty ("name")
        ? root.getProperty ("name").toString()
        : file.getFileNameWithoutExtension();
    curPresetIndex.store (-1);
    initLoaded.store (false);

    pushFocusedGroupToAPVTS();
    return true;
}

juce::Array<juce::File> TidesAudioProcessor::listUserPresets() const
{
    juce::Array<juce::File> files;
    auto dir = userPresetDirectory();
    if (dir.isDirectory())
    {
        dir.findChildFiles (files, juce::File::findFiles, false, "*.tides");
        std::sort (files.begin(), files.end(),
            [] (const juce::File& a, const juce::File& b)
            { return a.getFileNameWithoutExtension().compareIgnoreCase (
                       b.getFileNameWithoutExtension()) < 0; });
    }
    return files;
}

bool TidesAudioProcessor::userPresetExists (const juce::String& name) const
{
    const auto safeName = name.trim();
    if (safeName.isEmpty()) return false;
    return userPresetDirectory().getChildFile (safeName + ".tides").existsAsFile();
}

bool TidesAudioProcessor::renameUserPreset (const juce::File& file,
                                            const juce::String& newName)
{
    const auto safeName = newName.trim();
    if (safeName.isEmpty()) return false;
    if (! file.existsAsFile()) return false;

    auto dest = userPresetDirectory().getChildFile (safeName + ".tides");
    if (dest == file) return true;            // no-op rename
    if (dest.existsAsFile()) return false;    // refuse to clobber

    const bool wasLoaded = (userPresetName == file.getFileNameWithoutExtension());
    if (! file.moveFileTo (dest)) return false;
    if (wasLoaded) userPresetName = safeName;
    stateEpoch.fetch_add (1);
    return true;
}

bool TidesAudioProcessor::deleteUserPreset (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    const bool wasLoaded = (userPresetName == file.getFileNameWithoutExtension());
    if (! file.deleteFile()) return false;
    if (wasLoaded) userPresetName = juce::String();
    stateEpoch.fetch_add (1);
    return true;
}


void TidesAudioProcessor::loadFactoryPreset (int index, bool keepOtherGroups)
{
    pendingFocusSync.store (true);
    pendingVoiceKill.store (true);   // silence active voices on the next block

    const int safeIdx = juce::jlimit (0, tides::state::factory::count() - 1, index);
    const auto& info  = tides::state::factory::info (safeIdx);

    if (info.multiGroup)
    {
        // LAYERED presets define their own multi-group structure; replace
        // the whole preset. (keepOtherGroups is ignored here — multi-group
        // presets own the rack by definition.)
        tides::state::factory::loadIntoPreset (safeIdx, presetState);
        presetState.focused.store (0);
    }
    else
    {
        // Single-group presets land on the currently-focused group.
        // Default behaviour: every other group gets nuked back to INIT —
        // without this clear, loading on top of a previously-loaded
        // LAYERED preset (e.g. "Stack Lead", which populates G1+G2+G3 as
        // Body/Shimmer/Sub) leaves the layered groups still playing.
        // ⌥-click in BROWSE passes keepOtherGroups=true to skip the
        // clear — used for manually stacking presets across groups.
        const int focused = presetState.focused.load();
        tides::state::factory::loadIntoFocusedGroup (safeIdx, presetState, focused);
        if (! keepOtherGroups)
        {
            for (int gi = 0; gi < tides::state::kMaxGroups; ++gi)
            {
                if (gi == focused) continue;
                presetState.group[(size_t) gi].resetToInit();
                presetState.groupNames[gi]   = juce::String::fromUTF8 ("G·")
                                                + juce::String (gi + 1);
                presetState.groupPoly[gi]   .store (0);
                presetState.groupChannel[gi].store (gi + 1);
                presetState.groupPriority[gi].store (0);
            }
        }
    }

    // Restore the user's globally-saved MIDI channel preferences on top of
    // the preset's own channel choices — MIDI routing is a user preference,
    // not a preset property.
    applyUserMidiChannels();

    // Mirror the live preset into the A/B baseline.
    auto copyPreset = [] (tides::state::Preset& src, tides::state::Preset& dst)
    {
        for (int g = 0; g < tides::state::kMaxGroups; ++g)
        {
            for (int i = 0; i < tides::params::kNumParams; ++i)
                dst.group[(size_t) g].set (
                    static_cast<tides::params::P> (i),
                    src.group[(size_t) g].get (static_cast<tides::params::P> (i)));
            dst.groupChannel [g].store (src.groupChannel [g].load());
            dst.groupPoly    [g].store (src.groupPoly    [g].load());
            dst.groupPriority[g].store (src.groupPriority[g].load());
            dst.groupNames   [g] = src.groupNames   [g];
        }
        dst.focused.store        (src.focused.load());
        dst.presetChannel.store  (src.presetChannel.load());
        dst.presetUnison.store   (src.presetUnison.load());
        dst.presetPriority.store (src.presetPriority.load());
        dst.qualityMode.store    (src.qualityMode.load());
        dst.mpeMode.store        (src.mpeMode.load());
        dst.mpeBendSemis.store   (src.mpeBendSemis.load());
    };
    copyPreset (presetState, baselineState);
    showingSaved.store (false);

    curPresetIndex.store (safeIdx);
    userPresetName = juce::String();   // factory preset → clear "user loaded" name
    initLoaded.store (false);
    pushFocusedGroupToAPVTS();
}

juce::AudioProcessorEditor* TidesAudioProcessor::createEditor()
{
    return new TidesAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TidesAudioProcessor();
}
