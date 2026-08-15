#pragma once
#include <array>
#include <atomic>
#include "state/ParameterIDs.h"
#include "mod/ModDefs.h"

// One voice-group's worth of sound state. For the INIT milestone there is a
// single implicit voice group spanning all six voices; the structure is
// already shaped so multitimbral voice-groups become an array of these later.
//
// Storage is normalised (0..1) and atomic: the message/UI thread writes,
// the audio thread reads. DSP modules own the normalised->engineering
// mapping (kept next to the code that depends on the curve).

namespace tides::state
{
    enum class EnvRange : int { Short = 0, Medium = 1, Long = 2 };

    // Per-LFO controls (4 per voice). MIDI-sync / single-shot / cross-mod
    // are deferred; these cover free-run shape + S&H random behaviour.
    struct LfoParams
    {
        std::atomic<float> rate       { 0.5f };  // 0..1 piecewise → 0.005..1 Hz then 1..50 Hz (0.5 = 1 Hz)
        std::atomic<float> randomRate { 0.0f };  // 0 = S&H stopped
        std::atomic<float> amplitude  { 1.0f };
        std::atomic<float> mix        { 1.0f };  // 1 = periodic only
        std::atomic<float> smooth     { 0.0f };  // 0..1 S&H slew amount
        std::atomic<int>   wave       { 0 };     // 0 sin,1 tri,2 ramp,3 sqr
        std::atomic<bool>  bipolar    { true };
        std::atomic<bool>  kbReset    { false };
    };

    class PatchState
    {
    public:
        PatchState() { resetToInit(); }

        void resetToInit();

        float  get (params::P p) const noexcept
        {
            return values[static_cast<size_t>(p)].load(std::memory_order_relaxed);
        }
        void   set (params::P p, float normalised) noexcept
        {
            values[static_cast<size_t>(p)].store(normalised, std::memory_order_relaxed);
        }

        // ---- discrete switches / quantised controls ----
        std::atomic<int>  envRange   { (int) EnvRange::Medium };
        std::atomic<bool> envHold    { false };
        std::atomic<bool> envLegato  { false };
        std::atomic<bool> envLoop    { false };
        std::atomic<bool> kbReset    { false };
        std::atomic<bool> resInvert  { false };
        std::atomic<int>  resNoteOffset { 0 };  // -12..+12 semitones
        std::atomic<bool> keyFollowVco { true };// INIT: VCO tracks keyboard
        std::atomic<bool> extIn        { false };// feed external audio to VCO

        // Unison: stack N voices per note, detuned and spread across the
        // stereo field (the manual's "comprehensive unison control" and the
        // per-voice "stereo spread"). voices == 1 => unison off.
        std::atomic<int>   unisonVoices { 1 };
        std::atomic<float> unisonDetune { 0.3f };
        std::atomic<float> unisonSpread { 0.5f };

        std::array<LfoParams, 4> lfo {};

        // Global performance sources (per-channel refinement comes with the
        // multitimbral MIDI layer; global is fine for INIT).
        std::atomic<float> modwheel   { 0.0f };
        std::atomic<float> aftertouch { 0.0f };

        // ---- modulation matrix ----
        std::array<mod::Connection, mod::kMaxConnections> connections {};
        std::atomic<int>  numConnections { 0 };
        std::array<std::atomic<float>, mod::kNumSources> sourceMacro {}; // -1..+1
        std::array<std::atomic<bool>,  mod::kNumSources> sourceDetached {};

    private:
        std::array<std::atomic<float>, params::kNumParams> values {};
    };
}
