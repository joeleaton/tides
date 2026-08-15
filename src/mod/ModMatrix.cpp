#include "mod/ModMatrix.h"

namespace tides::dsp
{
    void ModMatrix::evaluate (const state::PatchState& s,
                              const std::array<float, mod::kNumSources>& srcVals) noexcept
    {
        offsets.fill(0.0f);

        const int n = s.numConnections.load(std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const auto& c = s.connections[(size_t) i];
            if (! c.enabled || c.muted) continue;

            const size_t si = static_cast<size_t>(c.source);
            // Detached source: macro becomes a static manual value.
            const float sv = s.sourceDetached[si].load(std::memory_order_relaxed)
                           ? 1.0f
                           : srcVals[si];
            const float macro = s.sourceMacro[si].load(std::memory_order_relaxed);

            offsets[static_cast<size_t>(c.dest)] += sv * macro * c.depth;
        }
    }
}
