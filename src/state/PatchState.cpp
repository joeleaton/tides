#include "state/PatchState.h"

namespace tides::state
{
    void PatchState::resetToInit()
    {
        for (int i = 0; i < params::kNumParams; ++i)
            values[static_cast<size_t>(i)].store(
                params::descriptor(static_cast<params::P>(i)).def,
                std::memory_order_relaxed);

        envRange.store((int) EnvRange::Medium);
        envHold.store(false);
        envLegato.store(false);
        envLoop.store(false);
        kbReset.store(false);
        resInvert.store(false);
        resNoteOffset.store(0);
        keyFollowVco.store(true);
        extIn.store(false);

        modwheel.store(0.0f);
        aftertouch.store(0.0f);
        unisonVoices.store(1);
        unisonDetune.store(0.3f);
        unisonSpread.store(0.5f);
        for (auto& L : lfo)
        {
            L.rate.store(0.25f);  L.randomRate.store(0.0f);
            L.amplitude.store(1.0f); L.mix.store(1.0f);
            L.wave.store(0); L.bipolar.store(true);
            L.smooth.store(0.0f); L.kbReset.store(false);
        }

        numConnections.store(0);
        // Source-macro attenuverter default = 0 (12 o'clock = no contribution).
        // The player explicitly dials each source in by turning its macro up.
        // The matrix multiplies source × macro × depth, so a fresh routing
        // stays silent until the macro is moved off centre — intentional, so
        // performance gestures don't surprise the user.
        for (int s = 0; s < mod::kNumSources; ++s)
        {
            sourceMacro[static_cast<size_t>(s)].store(0.0f);
            sourceDetached[static_cast<size_t>(s)].store(false);
        }
        for (auto& c : connections) c = {};
    }
}
