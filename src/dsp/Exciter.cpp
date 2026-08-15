#include "dsp/Exciter.h"
#include "dsp/ParamMap.h"

namespace tides::dsp
{
    void Exciter::prepare (double sr)
    {
        vco.prepare(sr);
        noise.prepare(sr);
        env.prepare(sr);
    }

    void Exciter::noteOn (bool legato, bool kbReset) noexcept
    {
        // KB Reset off (default, per manual) => VCO free-runs; the note-on
        // phase is whatever it was, so a sine no longer slams the co-tuned
        // resonator identically every note (organic, no attack click).
        vco.reset(kbReset);
        env.noteOn(legato);
    }

    void Exciter::updateFromState (const state::ResolvedParams& s) noexcept
    {
        using namespace tides::params;
        using namespace tides::dsp::map;

        vco.setMorph(s.get(P::ExWave));
        vco.setExternalEnabled(s.extIn);
        noise.setRateHz(noiseShRateHz(s.get(P::ExNoiseSHRate)));
        waveNoiseMix = s.get(P::ExWaveNoiseMix);
        level        = s.get(P::ExLevel);

        const int range = s.envRange;
        env.setTimes(onsetDelaySeconds(s.get(P::EnvOnsetDelay)),
                     stageSeconds(s.get(P::EnvAttack),  range),
                     stageSeconds(s.get(P::EnvDecay),   range),
                     s.get(P::EnvSustain),
                     stageSeconds(s.get(P::EnvRelease), range));
        env.setLoop(s.envLoop);
    }
}
