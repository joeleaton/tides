#pragma once
#include <JuceHeader.h>
#include <array>
#include "engine/Voice.h"
#include "state/Preset.h"
#include "vis/EngineSnapshot.h"

// Multitimbral engine over a shared voice pool. Each MIDI channel routes to
// the voice group(s) assigned that channel; each group draws up to its
// polyphony budget from the shared pool using its own sound (PatchState).
namespace tides::engine
{
    class SynthEngine
    {
    public:
        static constexpr int kNumVoices = 16;

        void prepare (double sampleRate, int maxBlock);
        void reset();

        void processBlock (juce::AudioBuffer<float>& audio,
                           juce::MidiBuffer& midi,
                           state::Preset& preset,
                           const float* extMono, int extLen);

        vis::EngineSnapshot& snapshot() noexcept { return snap; }

    private:
        void handleMidi (const juce::MidiMessage&, state::Preset&);
        void noteOnGroup (int group, int n, float v, state::Preset&);
        void noteOff     (int n, int channel, state::Preset&);
        int  pickGroupForPreset (int n, state::Preset&);
        Voice* pickVoiceForGroup (int group, int n, state::Preset&);

        std::array<Voice, kNumVoices> voices;
        double   sampleRate { 48000.0 };
        uint64_t stamp { 0 };
        int      rrVoice[state::kMaxGroups] {};
        uint64_t groupLastStamp[state::kMaxGroups] {};
        int      presetRRGroup { 0 };
        vis::EngineSnapshot snap;
        float    peakEnv { 0.0f };
        float    peakReleaseCoeff { 0.9999f };
    };
}
