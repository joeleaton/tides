#pragma once
#include <JuceHeader.h>
#include "engine/SynthEngine.h"
#include "state/PatchState.h"
#include "state/Preset.h"
#include "mod/ModDefs.h"

namespace tides::mod
{
    // Cross-pane state for patch-mode assignment. The Modulation pane arms a
    // source tile; while armed, every modulatable Knob/AtomicKnob paints the
    // "selecting" outline and treats the next click as an "assign to me"
    // command instead of a value change. ChangeBroadcaster pings all
    // listeners (knobs + the modulation pane's own banner) on arm/disarm.
    class PatchModeState : public juce::ChangeBroadcaster
    {
    public:
        void arm (int src)
        {
            armedSrc.store (src);
            sendChangeMessage();
        }
        void disarm()
        {
            if (armedSrc.exchange (-1) >= 0)
                sendChangeMessage();
        }
        bool isArmed() const { return armedSrc.load() >= 0; }
        int  armedSource() const { return armedSrc.load(); }

    private:
        std::atomic<int> armedSrc { -1 };
    };
}

class TidesAudioProcessor : public juce::AudioProcessor
{
public:
    TidesAudioProcessor();
    ~TidesAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tides"; }
    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int  getNumPrograms() override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "INIT"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // patch() == the focused voice group (what the editor edits).
    tides::state::PatchState& patch() noexcept { return presetState.focusedGroup(); }
    tides::state::Preset&     preset() noexcept { return presetState; }
    tides::state::Preset&     baseline() noexcept { return baselineState; }
    tides::vis::EngineSnapshot& snapshot() noexcept { return engine.snapshot(); }
    juce::MidiKeyboardState   keyboardState;

    // A/B compare (matches the manual's RELOAD semantics): pressing once
    // swaps the live preset with the saved baseline; pressing again
    // restores the pre-press edits. UI observes showingSaved for the
    // chip's lit state.
    void toggleCompare();
    std::atomic<bool> showingSaved { false };

    // Bumped on setStateInformation/focus-switch so an open editor can
    // refresh its non-APVTS controls (APVTS-attached ones update themselves).
    std::atomic<int> stateEpoch { 0 };

    // Host-automation surface. The focused voice group's params are exposed
    // here; non-focused groups remain in PatchState and are loaded into
    // APVTS on focus switch / preset restore (focused-group facade).
    juce::AudioProcessorValueTreeState apvts;

    // Patch-mode (click-to-assign) cross-pane state. See PatchModeState.
    tides::mod::PatchModeState patchMode;

    // Add or update a routing from the currently-armed source to `dest`.
    // Defaults to +50% depth; if a connection already exists between this
    // source/dest pair its depth bumps by the same amount (signed). Does
    // nothing if patch mode is not armed.
    void assignConnection (tides::mod::Dest dest, float depthStep = 0.5f);
    void removeConnection (int index);
    void toggleConnectionMuted   (int index);
    void toggleConnectionEnabled (int index);

    // ─── Factory preset library ────────────────────────────────────────
    // Loads a preset from the factory library by index, updates the
    // currently-selected-preset state, and triggers an APVTS resync so the
    // editor's host-automation surface mirrors the new sound. Names + tags
    // come from src/state/FactoryPresets.h.
    // keepOtherGroups = layer the preset onto the focused group without
    // touching the other groups (used by ⌥-click in BROWSE to stack
    // multiple presets manually). LAYERED multi-group presets ignore the
    // flag — they own the whole rack by definition.
    void loadFactoryPreset (int index, bool keepOtherGroups = false);
    int  currentPresetIndex() const noexcept { return curPresetIndex.load(); }
    std::atomic<int> curPresetIndex { 0 };

    // True INIT — sine exciter at unity into a clean resonator, default
    // envelope, full Dry/Wet, no mod connections, neutral macros. Loads
    // into the focused group only (other groups preserved).
    void loadInitPatch();
    bool isInitLoaded() const noexcept { return initLoaded.load(); }
    std::atomic<bool> initLoaded { false };

    // Preset morph — pick two presets as endpoints (A / B). The morph slider
    // then crossfades between them on the focused group's PatchState. Setting
    // an endpoint rebuilds its cached PatchState. Endpoints can be either a
    // factory preset (by index) or a user preset (by File).
    enum class MorphKind : int { Factory = 0, User = 1 };
    void setMorphEndpointFactoryA (int presetIdx);
    void setMorphEndpointFactoryB (int presetIdx);
    void setMorphEndpointUserA    (const juce::File& userFile);
    void setMorphEndpointUserB    (const juce::File& userFile);
    int  morphEndpointA() const noexcept { return presetState.morphA.load(); }
    int  morphEndpointB() const noexcept { return presetState.morphB.load(); }
    MorphKind morphKindA() const noexcept {
        return (MorphKind) presetState.morphAKind.load();
    }
    MorphKind morphKindB() const noexcept {
        return (MorphKind) presetState.morphBKind.load();
    }
    juce::String morphUserA() const { return presetState.morphAUser; }
    juce::String morphUserB() const { return presetState.morphBUser; }
    float morphValue() const noexcept {
        return morphParamPtr != nullptr ? morphParamPtr->load() : 0.0f;
    }

    // ─── Global MIDI-channel preferences ────────────────────────────────
    // Per-group and preset MIDI channels are user preferences that persist
    // across sessions (independently of the host's preset save). Stored in
    // ~/Library/Application Support/Tides/settings.xml.
    static juce::File settingsFile();
    void  setGroupMidiChannel  (int group, int channel);   // 1..16
    void  setPresetMidiChannel (int channel);              // 1..16
    void  applyUserMidiChannels();                          // → presetState
    void  saveUserMidiChannels() const;

    // User presets — saved per-focused-group XML in
    // ~/Library/Application Support/Tides/Presets/.
    static juce::File userPresetDirectory();
    bool  saveCurrentAsUserPreset (const juce::String& name);   // overwrites
    // User presets always land on the focused group only and leave
    // other groups intact — so they're inherently layer-friendly with
    // no modifier needed.
    bool  loadUserPreset (const juce::File& file);
    juce::Array<juce::File> listUserPresets() const;
    juce::String currentUserPresetName() const { return userPresetName; }
    bool  userPresetExists (const juce::String& name) const;
    bool  renameUserPreset (const juce::File& file, const juce::String& newName);
    bool  deleteUserPreset (const juce::File& file);
private:
    juce::String userPresetName;
public:

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushFocusedGroupToAPVTS();
    void routeParamCCs (juce::MidiBuffer& midi);

    tides::state::Preset       presetState;
    tides::state::Preset       baselineState;   // saved version for A/B
    tides::state::Preset       editState;       // pre-A/B-press edits
    tides::engine::SynthEngine engine;
    std::vector<float>         extMono;   // mono mix of the Ext In bus
    std::atomic<bool>          pendingFocusSync { false };
public:
    // Set by preset loads to ask the audio thread to silence every voice on
    // the next block — fixes notes that would otherwise hang against the
    // freshly-loaded group routing.
    std::atomic<bool>          pendingVoiceKill { false };
private:
    std::array<std::atomic<float>*, tides::params::kNumParams> contParamPtr {};
    std::array<std::atomic<float>*, 9> macroParamPtr {};
    std::atomic<float>*        focusedParamPtr { nullptr };
    std::atomic<float>*        morphParamPtr   { nullptr };

    void applyMorphToFocusedGroup();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TidesAudioProcessor)
};
