#pragma once
#include "state/ResolvedParams.h"

// Per-voice mono -> stereo: equal-power Panning + per-voice Level.
// (Master is applied once at the engine sum.)
//
// Parameter updates arrive once per block from `update()`. Without
// per-sample smoothing, fast LFO modulation on the LEVEL destination
// produces audible clicks at every block boundary where the target jumps
// — the user hears that as crackle. We hold a per-sample smoother on
// level + pan that ramps toward the new targets over ~1 ms, removing the
// discontinuity while keeping square-wave tremolo crisp.
namespace tides::dsp
{
    class OutputStage
    {
    public:
        void prepare (double sampleRate) noexcept;

        // panOff (unison spread) is added to the base Panning position.
        void update (const state::ResolvedParams& s, float panOff) noexcept;

        inline void process (float mono, float& l, float& r) noexcept
        {
            level += (targetLevel - level) * smoothCoef;
            panL  += (targetPanL  - panL ) * smoothCoef;
            panR  += (targetPanR  - panR ) * smoothCoef;
            const float g = mono * level;
            l = g * panL;
            r = g * panR;
        }

    private:
        float level { 0.8f },  targetLevel { 0.8f };
        float panL  { 0.7071f }, targetPanL { 0.7071f };
        float panR  { 0.7071f }, targetPanR { 0.7071f };
        float smoothCoef { 0.05f };   // overwritten in prepare()
    };
}
