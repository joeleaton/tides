#pragma once
#include <array>
#include "mod/ModDefs.h"
#include "state/PatchState.h"

// Evaluates 9 sources x 32 destinations into a per-destination offset buffer.
// SKELETON: structure + source-macro/detach handling in place; INIT has no
// connections so output is zero. Source value gathering (env/LFO/perf) and
// unipolar/bipolar application land with the modulation stage.
namespace tides::dsp
{
    class ModMatrix
    {
    public:
        void clear() noexcept { offsets.fill(0.0f); }

        // sourceValues indexed by mod::Source (bipolar/unipolar already
        // normalised by the caller). Applies source macro then accumulates.
        void evaluate (const state::PatchState& s,
                       const std::array<float, mod::kNumSources>& sourceValues) noexcept;

        float offset (mod::Dest d) const noexcept
        {
            return offsets[static_cast<size_t>(d)];
        }

    private:
        std::array<float, mod::kNumDests> offsets {};
    };
}
