#pragma once
#include <array>
#include <algorithm>
#include "state/ParameterIDs.h"
#include "mod/ModDefs.h"
#include "mod/ModMatrix.h"
#include "state/PatchState.h"

// Per-voice, per-block resolved parameter view: base value (from PatchState)
// plus the modulation-matrix offset for that parameter's destination,
// clamped to [0,1]. DSP modules consume this instead of PatchState so they
// stay oblivious to the modulation system.
namespace tides::state
{
    // Map a parameter to its modulation destination (-1 if not modulatable).
    inline int destForParam (params::P p) noexcept
    {
        using P = params::P;
        using D = mod::Dest;
        switch (p)
        {
            case P::ExWave:           return (int) D::ExciterWave;
            case P::ExNoiseSHRate:    return (int) D::ExciterNoiseSHRate;
            case P::ExWaveNoiseMix:   return (int) D::ExciterWaveNoiseMix;
            case P::ExCoarseFreq:     return (int) D::ExciterFrequency;
            case P::ExFineFreq:       return (int) D::ExciterFrequency;
            case P::ExLevel:          return (int) D::ExciterLevel;
            case P::EnvOnsetDelay:    return (int) D::EnvOnsetDelay;
            case P::EnvAttack:        return (int) D::EnvAttack;
            case P::EnvDecay:         return (int) D::EnvDecay;
            case P::ResInputFilter:   return (int) D::ResInputFilter;
            case P::ResFeedbackFilter:return (int) D::ResFeedbackFilter;
            case P::ResFeedback:      return (int) D::ResFeedback;
            case P::ResGlide:         return (int) D::ResGlide;
            case P::ResFineTune:      return (int) D::ResFineTune;
            case P::OutDryWet:        return (int) D::ResDryWet;
            case P::OutPanning:       return (int) D::ResPanning;
            case P::OutLevel:         return (int) D::ResLevel;
            case P::WildTame:         return (int) D::WildTame;
            case P::EnvSustain:
            case P::EnvRelease:
            case P::Master:
            case P::NumParams:        return -1;   // not modulatable
        }
        return -1;
    }

    struct ResolvedParams
    {
        std::array<float, params::kNumParams> v {};
        int  envRange { 1 };
        bool envHold { false }, envLegato { false }, envLoop { false };
        bool kbReset { false }, resInvert { false }, keyFollowVco { true };
        bool extIn { false };
        int  resNoteOffset { 0 };

        float get (params::P p) const noexcept { return v[(size_t) p]; }

        void build (const PatchState& s, const dsp::ModMatrix& mm) noexcept
        {
            for (int i = 0; i < params::kNumParams; ++i)
            {
                const auto p = static_cast<params::P>(i);
                float val = s.get(p);
                const int d = destForParam(p);
                if (d >= 0) val += mm.offset(static_cast<mod::Dest>(d));
                v[(size_t) i] = std::clamp(val, 0.0f, 1.0f);
            }
            envRange      = s.envRange.load(std::memory_order_relaxed);
            envHold       = s.envHold.load(std::memory_order_relaxed);
            envLegato     = s.envLegato.load(std::memory_order_relaxed);
            envLoop       = s.envLoop.load(std::memory_order_relaxed);
            kbReset       = s.kbReset.load(std::memory_order_relaxed);
            resInvert     = s.resInvert.load(std::memory_order_relaxed);
            keyFollowVco  = s.keyFollowVco.load(std::memory_order_relaxed);
            extIn         = s.extIn.load(std::memory_order_relaxed);
            resNoteOffset = s.resNoteOffset.load(std::memory_order_relaxed);
        }
    };
}
