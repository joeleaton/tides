#pragma once
#include <array>
#include <cstdint>

// Modulation source / destination identities.
// IDs deliberately match the ECHON 6 manual's "Modulation Source Selection"
// and "Modulation Destination Parameter Selection" tables so that future
// SysEx / MIDI-CC parity is a straight mapping (no translation layer).

namespace tides::mod
{
    // ---- Sources (manual: 0..8) -------------------------------------------
    enum class Source : uint8_t
    {
        Envelope   = 0,
        Lfo1       = 1,
        Lfo2       = 2,
        Lfo3       = 3,
        Lfo4       = 4,
        Modwheel   = 5,
        Velocity   = 6,
        Aftertouch = 7,
        KeyFollow  = 8,
        Count      = 9
    };

    // ---- Destinations (manual: 0..31) -------------------------------------
    enum class Dest : uint8_t
    {
        ExciterWave        = 0,   // Wave / Ext-In morph
        ExciterNoiseSHRate = 1,
        ExciterWaveNoiseMix= 2,
        ExciterFrequency   = 3,   // Coarse + Fine share one destination
        ExciterLevel       = 4,
        EnvOnsetDelay      = 5,
        EnvAttack          = 6,
        EnvDecay           = 7,
        ResInputFilter     = 8,
        ResFeedbackFilter  = 9,
        ResFeedback        = 10,
        ResGlide           = 11,
        ResFineTune        = 12,
        ResDryWet          = 13,
        ResPanning         = 14,
        ResLevel           = 15,
        Lfo1Rate           = 16,
        Lfo1Mix            = 17,
        Lfo1Amplitude      = 18,
        Lfo1RandomRate     = 19,
        Lfo2Rate           = 20,
        Lfo2Mix            = 21,
        Lfo2Amplitude      = 22,
        Lfo2RandomRate     = 23,
        Lfo3Rate           = 24,
        Lfo3Mix            = 25,
        Lfo3Amplitude      = 26,
        Lfo3RandomRate     = 27,
        Lfo4Rate           = 28,
        Lfo4Mix            = 29,
        Lfo4Amplitude      = 30,
        Lfo4RandomRate     = 31,
        // Global character macro — affects drift / noise / detune / saturator.
        // Not in the manual's destination chart but a natural addition for
        // performance routings ("modwheel pushes the patch wild").
        WildTame           = 32,
        // Preset-morph crossfade position. The processor evaluates the live
        // morph offset before lerping the focused group's params, so any
        // source can sweep the patch from endpoint A to endpoint B.
        Morph              = 33,
        Count              = 34
    };

    constexpr int kNumSources = static_cast<int>(Source::Count);
    constexpr int kNumDests   = static_cast<int>(Dest::Count);

    // One routing in the matrix. depth is the per-connection amount at
    // SOURCE-macro unity; polarity folded into sign of depth. The source
    // macro (attenuverter, -1..+1) is applied separately, per source.
    //
    // depthDir is a UI-state field for the click-to-bump cycle: each
    // re-click of an armed source on the same destination steps the depth
    // by depthStep in the current direction. When it hits +1 or -1 the
    // direction flips, so repeat clicks walk the full range
    //     0 → +0.5 → +1 → +0.5 → 0 → −0.5 → −1 → −0.5 → 0 → +0.5 …
    // (triangle traversal), rather than saturating at the first boundary.
    struct Connection
    {
        Source  source { Source::Envelope };
        Dest    dest   { Dest::ExciterFrequency };
        float   depth  { 0.0f };   // -1..+1
        bool    enabled{ false };
        bool    muted  { false };
        int8_t  depthDir { 1 };    // +1 ascending, -1 descending
    };

    // 9x32 is the documented ceiling; realistically sparse. A fixed pool
    // keeps the audio thread allocation-free.
    constexpr int kMaxConnections = kNumSources * kNumDests;
}
