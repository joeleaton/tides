#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>

// Lock-free engine introspection for the UI. Audio thread writes; UI thread
// reads on a timer. Designed to be allocation-free on the audio thread and
// tearing-tolerant on the read side (UI visuals don't need strict
// consistency, just current-ish state).
namespace tides::vis
{
    class EngineSnapshot
    {
    public:
        static constexpr int kNumVoices = 16;
        static constexpr int kScopeSize = 4096;          // power of two

        enum VoiceState : uint8_t { Idle = 0, Held = 1, Ringing = 2 };

        struct Voice
        {
            std::atomic<int>      group   { 0 };
            std::atomic<int>      note    { -1 };
            std::atomic<uint8_t>  state   { Idle };
            std::atomic<uint64_t> age     { 0 };
            std::atomic<float>    energy  { 0.0f };
            std::atomic<bool>     selfOsc { false };
        };

        std::array<Voice, kNumVoices> voices;
        std::atomic<float> masterPeak { 0.0f };
        std::atomic<float> extInPeak  { 0.0f };

        // Live modulation-source values for the most-recently-started
        // active voice. The UI uses these to animate mod-ribbon markers on
        // destination knobs — i.e. show the spot the source is *currently*
        // pushing the parameter to, not just the static depth envelope.
        // Indexed by mod::Source enum (Envelope, Lfo1..4, Modwheel,
        // Velocity, Aftertouch, KeyFollow = 9 entries).
        static constexpr int kNumModSources = 9;
        std::array<std::atomic<float>, kNumModSources> liveSources {};

        // ---- master scope ring (audio thread writes one sample at a time) ----
        inline void pushScope (float s) noexcept
        {
            const int w = writeIdx.load (std::memory_order_relaxed);
            scope[(size_t) (w & (kScopeSize - 1))] = s;
            writeIdx.store (w + 1, std::memory_order_release);
        }

        // Copy the last `count` samples (newest at the end) into dest.
        // Tearing-tolerant: a sample may be torn during a write but for
        // visualisation this is invisible.
        int readLatest (float* dest, int count) const noexcept
        {
            const int n = std::min (count, kScopeSize);
            const int w = writeIdx.load (std::memory_order_acquire);
            for (int i = 0; i < n; ++i)
                dest[i] = scope[(size_t) ((w - n + i) & (kScopeSize - 1))];
            return n;
        }

    private:
        std::array<float, kScopeSize> scope {};
        std::atomic<int> writeIdx { 0 };
    };
}
