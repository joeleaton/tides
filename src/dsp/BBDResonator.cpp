#include "dsp/BBDResonator.h"
#include "dsp/ParamMap.h"
#include <cmath>
#include <algorithm>

namespace tides::dsp
{
    static constexpr float kPi = 3.14159265358979f;

    void BBDResonator::Biquad::design (double fc, double sr)
    {
        const double w0 = 2.0 * kPi * fc / sr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double Q  = 0.70710678;          // Butterworth
        const double al = sw / (2.0 * Q);
        const double a0 = 1.0 + al;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - al) / a0);
        z1 = z2 = 0.0f;
    }

    void BBDResonator::prepare (double sr)
    {
        sampleRate = sr;
        osRate     = sr * kOS;

        int need = 1;
        while (need < (int) (osRate / 12.0)) need <<= 1;
        delayBuf.assign((size_t) need, 0.0f);
        mask = need - 1;

        inputLP1.prepare(osRate); inputLP2.prepare(osRate);
        feedbackLP.prepare(osRate); bbdLossLP.prepare(osRate);
        decim.design(0.45 * sr, osRate);

        driftPeriod = std::max(1, (int) (osRate * 0.03)); // ~30 ms steps
        envRelCoeff = std::exp(-1.0f / (0.3f * (float) sampleRate));
        softLen = std::max(1, (int) (0.006 * sampleRate)); // ~6 ms de-energise
        bbdLossLP.setCutoff(lossCutHz);
        reset();
    }

    void BBDResonator::reset()
    {
        std::fill(delayBuf.begin(), delayBuf.end(), 0.0f);
        writePos = 0;
        curDelay = (float) (osRate / std::max(20.0f, targetHz));
        inputLP1.reset(); inputLP2.reset();
        feedbackLP.reset(); bbdLossLP.reset();
        decim.z1 = decim.z2 = 0.0f;
        prevExc = 0.0f;
        outEnv  = 0.0f;
        softCtr = 0;
        curSoftGain = 1.0f;
        driftCur = driftTarget = 0.0f;
        driftCounter = driftPeriod;
    }

    void BBDResonator::requestSoftReset() noexcept
    {
        softCtr = softLen;   // ramp the loop down, clear when silent
    }

    void BBDResonator::setVarianceSeed (uint32_t seed) noexcept
    {
        rng = seed * 2654435761u + 0x9E3779B9u;
        staticSeedNorm = white();                       // fixed [-1,1] / voice
    }

    void BBDResonator::updateFromState (const state::ResolvedParams& s) noexcept
    {
        using namespace tides::params;
        using namespace tides::dsp::map;

        const float ic = inputFilterHz(s.get(P::ResInputFilter));
        inputLP1.setCutoff(ic);
        inputLP2.setCutoff(ic);

        fbCutHz = feedbackFilterHz(s.get(P::ResFeedbackFilter));
        feedbackLP.setCutoff(fbCutHz);

        // Perceptual taper: a recursive comb's peak height is very nonlinear
        // in loop gain (flat until ~0.9 then explodes). The expo spreads the
        // musical region across more of the knob; top reaches self-osc.
        feedbackGain = 1.10f * std::pow(s.get(P::ResFeedback), 0.6f);

        // Wild/Tame: one knob over the analogue-instability stack. Tame =
        // clean & mathematical, Wild = noisy, drifting, hotter saturator.
        // Saturator-drive ceiling lowered (0.7..1.2 rather than 0.7..1.4)
        // so max wild stays controllable — the loop no longer crosses unity
        // gain at high feedback, which previously caused notes to keep
        // self-oscillating after release. Drift / noise / detune unchanged.
        const float w = s.get(P::WildTame);
        const float staticCents = staticSeedNorm * w * 6.0f;
        varStaticMul    = std::pow(2.0f, -staticCents / 1200.0f);
        varDriftDepth   = w * 4.0f;                           // ±4 cents max
        noiseSeedScale  = 1.0e-6f * std::pow(100.0f, w);      // 1e-6..1e-4
        satDrive        = 0.7f + 0.5f * w;                    // 0.7..1.2
        dryWet       = s.get(P::OutDryWet);
        invert       = s.resInvert;

        // The BBD clock cannot change infinitely fast, so even at Glide = 0
        // the delay length must slew over a short minimum (~4 ms). This is
        // still perceptually instant but prevents a read-pointer step in
        // the feedback loop (which high feedback turns into a click).
        const float glideS  = glideSeconds(s.get(P::ResGlide));
        const float maxCoef = 1.0f - std::exp(-1.0f / (0.004f * (float) osRate));
        const float usrCoef = glideS <= 0.0f
                            ? 1.0f
                            : 1.0f - std::exp(-1.0f / (glideS * (float) osRate));
        glideCoeff = std::min(maxCoef, usrCoef);
    }

    // Pitch compensation. We compensate as if the Feedback Filter were
    // wide open (reference cutoff) plus the fixed BBD-loss filter. This
    // keeps keyboard tracking accurate, but as the user closes the Feedback
    // Filter its growing group delay is left *uncompensated*, so the loop
    // locks lower -> the manual's audible downward pitch droop (emergent,
    // not scripted). f0 in Hz, result in osr samples.
    float BBDResonator::phaseDelaySamples (float f0) const noexcept
    {
        constexpr float kFbRefHz = 18000.0f;   // "filter open" reference
        const float twoPiF = 2.0f * kPi * std::max(f0, 1.0f);
        const float tFb   = std::atan(f0 / kFbRefHz)  / twoPiF;
        const float tLoss = std::atan(f0 / lossCutHz) / twoPiF;
        return (tFb + tLoss) * (float) osRate;
    }

    inline float BBDResonator::innerLoop (float x) noexcept
    {
        const float inF = inputLP2.processLP(inputLP1.processLP(x));

        // 4-point Lagrange (3rd order) fractional read.
        const float rp = (float) writePos - curDelay;
        int   i1 = (int) std::floor(rp);
        const float d = rp - (float) i1;
        const float ym1 = delayBuf[(size_t) ((i1 - 1) & mask)];
        const float y0  = delayBuf[(size_t) ( i1      & mask)];
        const float y1  = delayBuf[(size_t) ((i1 + 1) & mask)];
        const float y2  = delayBuf[(size_t) ((i1 + 2) & mask)];
        const float cm1 = -d * (d - 1.0f) * (d - 2.0f) / 6.0f;
        const float c0  = (d + 1.0f) * (d - 1.0f) * (d - 2.0f) * 0.5f;
        const float c1  = -(d + 1.0f) * d * (d - 2.0f) * 0.5f;
        const float c2  = (d + 1.0f) * d * (d - 1.0f) / 6.0f;
        float delayed = cm1 * ym1 + c0 * y0 + c1 * y1 + c2 * y2;

        // In-loop filtering: 6 dB/oct Feedback Filter, then fixed per-pass
        // BBD HF loss, then polarity. The filters' group delay (not a rule)
        // is what drifts the locked pitch as they close.
        float fb = bbdLossLP.processLP(feedbackLP.processLP(delayed));
        if (invert) fb = -fb;

        const float loopIn = inF + fb * feedbackGain + white() * noiseSeedScale;
        const float sat    = Saturator::process(loopIn, satDrive);

        delayBuf[(size_t) writePos] = sat * curSoftGain; // de-energise on steal
        writePos = (writePos + 1) & mask;

        return inF + (sat - inF) * dryWet;     // dry = filtered exciter
    }

    float BBDResonator::process (float exciterIn) noexcept
    {
        // Slow per-voice drift (random walk, smoothed).
        if (--driftCounter <= 0) { driftTarget = white(); driftCounter = driftPeriod; }
        driftCur += (driftTarget - driftCur) * 0.0009f;

        // 2^e for tiny e (a few cents) ≈ 1 + ln2·e — drift is sub-audio so
        // the linear approximation is exact to the ear and avoids a pow().
        const float e = -(driftCur * varDriftDepth) / 1200.0f;
        const float driftMul = 1.0f + 0.6931472f * e;

        float targetDelay = (float) (osRate / std::max(20.0f, targetHz));
        targetDelay *= varStaticMul * driftMul;
        targetDelay -= cachedPhaseDelay;                // pitch compensation
        targetDelay  = std::clamp(targetDelay, 4.0f, (float) mask - 4.0f);

        curDelay += (targetDelay - curDelay) * glideCoeff;

        curSoftGain = softCtr > 0 ? (float) softCtr / (float) softLen : 1.0f;

        float out = 0.0f;
        for (int s = 0; s < kOS; ++s)
        {
            const float frac = (float) (s + 1) / (float) kOS;
            const float up   = prevExc + (exciterIn - prevExc) * frac;
            out = decim.process(innerLoop(up));         // decimate by kOS
        }
        prevExc = exciterIn;
        out *= curSoftGain;

        if (softCtr > 0 && --softCtr == 0)
        {
            std::fill(delayBuf.begin(), delayBuf.end(), 0.0f); // silent now
            feedbackLP.reset(); bbdLossLP.reset();
            decim.z1 = decim.z2 = 0.0f;
        }

        const float a = std::abs(out);
        outEnv = a > outEnv ? a : outEnv * envRelCoeff;
        return out;
    }
}
