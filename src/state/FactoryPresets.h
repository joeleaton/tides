#pragma once
#include <vector>
#include "state/Preset.h"

// Factory preset library — ships with the plugin so users have a usable
// palette out of the box, and each preset showcases at least one technique
// the BBD resonator engine is good at.
//
// Each PresetInfo is a fully-typed pointer pair: category + display name +
// terse tag string + a free function that populates a Preset& from scratch.
// The implementation lives in FactoryPresets.cpp so the heavy "what does
// every param do" knowledge is concentrated in one file.

namespace tides::state::factory
{
    struct PresetInfo
    {
        const char* category;   // e.g. "PADS"
        const char* name;       // e.g. "Atrium"
        const char* tags;       // e.g. "ambient · drift · 6-group"
        void (*build)(Preset&); // mutates in place
        bool multiGroup;        // true → preset configures groups 0..N-1
                                // false → single-group, should load into the focused group
    };

    // Full library, in display order (category groups stay contiguous).
    const std::vector<PresetInfo>& library();

    // Look up by index — clamps to valid range.
    void loadIntoPreset (int index, Preset& dst);

    // Build the preset's group-0 worth of state into `dstGroup`, leaving
    // every other group in `dst` untouched. Used for loading single-group
    // presets into the focused group without nuking the rest of the rack.
    void loadIntoFocusedGroup (int index, Preset& dst, int dstGroup);

    // Deep-copy every UI-visible field of one PatchState into another.
    // Used by the morph machinery to populate endpoint caches.
    void copyPatchState (const PatchState& src, PatchState& dst);

    int  count();
    const PresetInfo& info (int index);
}
