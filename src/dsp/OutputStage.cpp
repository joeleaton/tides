#include "dsp/OutputStage.h"
#include <cmath>

namespace tides::dsp
{
    void OutputStage::prepare (double sampleRate) noexcept
    {
        // ~1 ms one-pole time constant — fast enough for crisp square-wave
        // tremolo, slow enough to remove the inter-block step discontinuity.
        constexpr float tauSec = 0.001f;
        smoothCoef = 1.0f - std::exp (-1.0f / (tauSec * (float) sampleRate));
    }

    void OutputStage::update (const state::ResolvedParams& s, float panOff) noexcept
    {
        targetLevel = s.get(params::P::OutLevel) * s.get(params::P::Master);
        float pan = s.get(params::P::OutPanning) + panOff; // 0..1, .5 = centre
        pan = pan < 0.0f ? 0.0f : pan > 1.0f ? 1.0f : pan;
        const float a = pan * 1.5707963f;                  // equal-power
        targetPanL = std::cos(a);
        targetPanR = std::sin(a);
    }
}
