#pragma once
#include <array>
#include <atomic>
#include <JuceHeader.h>
#include "state/PatchState.h"

// A Preset = up to 6 independent voice groups. Each group is a full
// PatchState (sound + mod matrix + unison) plus routing: its MIDI channel
// and a polyphony budget drawn dynamically from the shared voice pool
// (the hardware fixes voices to groups; this emulation allocates from a
// shared pool instead - simpler and not hardware-limited).
//
// Default is non-regressive: group 0 = full poly on MIDI channel 1, the
// other groups silent (poly 0) until the user enables them. The "focused"
// group is the one the editor edits.
namespace tides::state
{
    constexpr int kMaxGroups = 6;

    // Voice-allocation strategies (manual order). Used at the group level
    // (which voice) and the preset level (which group, on the preset chan).
    enum class Priority : int
    {
        FirstAvailSteal = 0,   // first free, else steal LRU  (default)
        FirstAvailNoSteal = 1, // first free, else ignore note
        RoundRobinSteal = 2,   // next in rotation, else steal LRU
        LeastRecentlyUsed = 3, // always oldest
        ReuseSameNote = 4      // reuse same-pitch if playing, else fallback
    };
    constexpr int kNumPriority = 5;

    // Global render-quality switch (preset-level setting). Trades CPU for
    // headroom: Eco caps the active voice pool so heavy patches stay
    // responsive on lower-power machines; HQ removes any artificial cap.
    enum class Quality : int
    {
        Eco = 0,   // active voice cap = 4
        Std = 1,   // active voice cap = 10 (default)
        HQ  = 2,   // active voice cap = 16 (full pool)
    };
    constexpr int kNumQuality = 3;

    struct Preset
    {
        std::array<PatchState, kMaxGroups> group;
        std::atomic<int> groupChannel[kMaxGroups];
        std::atomic<int> groupPoly[kMaxGroups];     // 0 = group silent
        std::atomic<int> groupPriority[kMaxGroups]; // Priority per group
        // Display name per group. UI thread only — the audio engine never
        // reads these, so no atomics required. Editing happens via the
        // GroupBar chip's inline TextEditor and via preset loads.
        juce::String     groupNames[kMaxGroups];
        std::atomic<int> focused { 0 };

        // Preset (master) MIDI channel: notes here go to all groups
        // (presetUnison) or to one group chosen by presetPriority.
        std::atomic<int>  presetChannel  { 16 };
        std::atomic<bool> presetUnison   { false };
        std::atomic<int>  presetPriority { 0 };
        std::atomic<int>  qualityMode    { (int) Quality::Std };

        // MPE (MIDI Polyphonic Expression). When enabled, the plugin is one
        // big MPE lower zone: master channel 1 + member channels 2-15. Each
        // note arrives on its own member channel; pitch bend / channel
        // pressure / CC74 on that channel apply only to that note's voice.
        // Default bend range follows the MPE spec (±48 semitones).
        std::atomic<bool>  mpeMode        { false };
        std::atomic<float> mpeBendSemis   { 48.0f };

        // Preset morph — crossfade the focused group between two preset
        // endpoints (A and B). Morph position lives in APVTS so it
        // automates / CC-maps. Endpoints can be a factory preset (by index)
        // or a user preset (by filename). The engine caches them as full
        // PatchStates on assignment so per-block interpolation is just a
        // lerp, not a full preset build.
        std::atomic<int>  morphA     { 0 };  // factory preset index (when Kind=Factory)
        std::atomic<int>  morphB     { 1 };
        std::atomic<int>  morphAKind { 0 };  // 0 = Factory, 1 = User
        std::atomic<int>  morphBKind { 0 };
        juce::String      morphAUser;         // basename of user preset (when Kind=User)
        juce::String      morphBUser;
        PatchState        morphCacheA;
        PatchState        morphCacheB;

        Preset() { initRouting(); }

        void initRouting() noexcept
        {
            // Default group names — plain "G·N" labels so users don't mistake
            // fancy factory-style names on silent groups as "already loaded"
            // presets. Presets can override them per-group; the user can also
            // rename via the GroupBar chip.
            for (int i = 0; i < kMaxGroups; ++i)
            {
                groupChannel[i].store(i + 1);              // ch 1..6
                groupPoly[i].store(i == 0 ? 16 : 0);       // only g0 audible
                groupPriority[i].store(0);
                groupNames[i] = juce::String::fromUTF8 ("G·")
                                  + juce::String (i + 1);
            }
            focused.store(0);
            presetChannel.store(16);
            presetUnison.store(false);
            presetPriority.store(0);
            qualityMode.store((int) Quality::Std);
            mpeMode.store(false);
            mpeBendSemis.store(48.0f);
        }

        // Active-voice cap implied by the current quality mode.
        int qualityVoiceCap() const noexcept
        {
            switch ((Quality) qualityMode.load (std::memory_order_relaxed))
            {
                case Quality::Eco: return 4;
                case Quality::Std: return 10;
                case Quality::HQ:  return 16;
            }
            return 16;
        }

        PatchState& focusedGroup() noexcept
        {
            return group[(size_t) focused.load(std::memory_order_relaxed)];
        }
    };
}
