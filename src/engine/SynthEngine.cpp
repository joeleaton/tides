#include "engine/SynthEngine.h"

namespace tides::engine
{
    using P = state::Priority;

    void SynthEngine::prepare (double sr, int /*maxBlock*/)
    {
        sampleRate = sr;
        for (int i = 0; i < kNumVoices; ++i) voices[(size_t) i].prepare(sr, i);
        stamp = 0;
        for (int g = 0; g < state::kMaxGroups; ++g) { rrVoice[g] = 0; groupLastStamp[g] = 0; }
        presetRRGroup = 0;
        peakEnv = 0.0f;
        peakReleaseCoeff = std::exp(-1.0f / (0.25f * (float) sr));   // ~250 ms
    }

    void SynthEngine::reset()
    {
        for (auto& v : voices) v.kill();
    }

    // Pick a voice for `group` honouring that group's Priority mode.
    // Polyphony is budgeted on *held* notes; ring-out tails do not consume
    // a slot and are the first thing reclaimed when the pool is tight.
    Voice* SynthEngine::pickVoiceForGroup (int g, int n, state::Preset& pr)
    {
        const auto mode = (P) pr.groupPriority[g].load();
        const int  poly = pr.groupPoly[g].load();

        if (mode == P::ReuseSameNote)
            for (auto& v : voices)
                if (v.isActive() && v.group() == g && v.note() == n)
                    return &v;                       // retrigger same pitch

        int held = 0;
        for (auto& v : voices) if (v.isHeld() && v.group() == g) ++held;
        const bool room = held < poly;

        auto oldestRingIn = [&] (int grp) -> Voice*
        {
            Voice* r = nullptr;
            for (auto& v : voices)
                if (v.isRinging() && (grp < 0 || v.group() == grp)
                    && (r == nullptr || v.age() < r->age())) r = &v;
            return r;
        };

        if (room)
        {
            // 1. a truly idle slot (round-robin aware)
            if (mode == P::RoundRobinSteal)
            {
                for (int s = 0; s < kNumVoices; ++s)
                {
                    const int i = (rrVoice[g] + s) % kNumVoices;
                    if (! voices[(size_t) i].isActive())
                    {
                        rrVoice[g] = (i + 1) % kNumVoices;
                        voices[(size_t) i].setGroup(g);
                        return &voices[(size_t) i];
                    }
                }
            }
            else
            {
                for (auto& v : voices)
                    if (! v.isActive()) { v.setGroup(g); return &v; }
            }
            // 2. no idle slot: reclaim a ring-out tail (this group first).
            //    Reuse is a clean retrigger - no resonator reset (the BBD
            //    keeps running, pitch slews, exciter env restarts): so the
            //    steal is click-free, matching the hardware's behaviour.
            Voice* r = oldestRingIn(g);
            if (r == nullptr) r = oldestRingIn(-1);
            if (r != nullptr) { r->setGroup(g); return r; }
            // 3. pool full of held voices (other groups): steal global oldest
            if (mode == P::FirstAvailNoSteal) return nullptr;
            Voice* o = nullptr;
            for (auto& v : voices)
                if (o == nullptr || v.age() < o->age()) o = &v;
            if (o != nullptr) o->setGroup(g);
            return o;
        }

        // At poly: this is the note-steal case for this group.
        if (mode == P::FirstAvailNoSteal) return nullptr;   // ignore note
        Voice* oldestHeld = nullptr;
        for (auto& v : voices)
            if (v.isHeld() && v.group() == g
                && (oldestHeld == nullptr || v.age() < oldestHeld->age()))
                oldestHeld = &v;
        if (oldestHeld != nullptr) oldestHeld->setGroup(g);
        return oldestHeld;
    }

    void SynthEngine::noteOnGroup (int g, int n, float vel, state::Preset& pr)
    {
        auto& patch = pr.group[(size_t) g];
        const int uni = juce::jlimit(1, kNumVoices, patch.unisonVoices.load());
        const float detSemis = patch.unisonDetune.load() * 1.0f;
        const float spread   = patch.unisonSpread.load() * 0.5f;

        for (int k = 0; k < uni; ++k)
        {
            auto* v = pickVoiceForGroup(g, n, pr);
            if (v == nullptr) break;
            const float t = (uni == 1) ? 0.0f
                          : ((float) k / (float) (uni - 1)) - 0.5f;
            v->setUnison(t * 2.0f * detSemis, t * 2.0f * spread);
            v->setStartStamp(++stamp);
            v->setMpe (false, 0);   // legacy channel-routed voice — not MPE
            v->noteOn(n, vel, patch);
        }
        groupLastStamp[g] = ++stamp;
    }

    // Choose ONE group for a note on the preset channel (preset Priority).
    int SynthEngine::pickGroupForPreset (int n, state::Preset& pr)
    {
        const auto mode = (P) pr.presetPriority.load();

        auto eligible = [&] (int g) { return pr.groupPoly[g].load() > 0; };

        if (mode == P::ReuseSameNote)
            for (auto& v : voices)
                if (v.isActive() && v.note() == n && eligible(v.group()))
                    return v.group();

        if (mode == P::RoundRobinSteal)
        {
            for (int s = 0; s < state::kMaxGroups; ++s)
            {
                const int g = (presetRRGroup + s) % state::kMaxGroups;
                if (eligible(g)) { presetRRGroup = (g + 1) % state::kMaxGroups; return g; }
            }
            return -1;
        }

        if (mode == P::LeastRecentlyUsed)
        {
            int best = -1; uint64_t bestStamp = ~0ull;
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (eligible(g) && groupLastStamp[g] < bestStamp)
                { bestStamp = groupLastStamp[g]; best = g; }
            return best;
        }

        // First-available (with/without steal): first group with capacity,
        // else first eligible group (steal happens inside the group).
        int firstEligible = -1;
        for (int g = 0; g < state::kMaxGroups; ++g)
        {
            if (! eligible(g)) continue;
            if (firstEligible < 0) firstEligible = g;
            int active = 0;
            for (auto& v : voices) if (v.isActive() && v.group() == g) ++active;
            if (active < pr.groupPoly[g].load()) return g;
        }
        if (mode == P::FirstAvailNoSteal) return -1;
        return firstEligible;
    }

    void SynthEngine::noteOff (int n, int channel, state::Preset& pr)
    {
        const bool isPreset = (channel == pr.presetChannel.load());
        for (auto& v : voices)
        {
            if (! v.isActive() || v.note() != n) continue;
            if (isPreset || pr.groupChannel[v.group()].load() == channel)
                v.noteOff(pr.group[(size_t) v.group()]);
        }
    }

    void SynthEngine::handleMidi (const juce::MidiMessage& m, state::Preset& pr)
    {
        // ── MPE lower-zone (master ch 1, member channels 2..15) ────────
        // When MPE is enabled, member channels behave per-note: each voice
        // tracks its birth channel and consumes pitch bend / channel
        // pressure / CC74 only on that channel. The focused group's sound
        // is used; group routing tables are bypassed.
        const bool mpe = pr.mpeMode.load (std::memory_order_relaxed);
        if (mpe)
        {
            const int ch = m.getChannel();
            const bool member = (ch >= 2 && ch <= 15);
            const bool master = (ch == 1);

            if (m.isNoteOn() && member)
            {
                const int focused = pr.focused.load();
                const int prevPoly = pr.groupPoly[focused].load();
                if (prevPoly > 0)
                {
                    auto* v = pickVoiceForGroup (focused, m.getNoteNumber(), pr);
                    if (v != nullptr)
                    {
                        v->setMpe (true, ch);
                        v->noteOn (m.getNoteNumber(), m.getFloatVelocity(),
                                   pr.group[(size_t) focused]);
                    }
                }
                return;
            }
            if (m.isNoteOff() && member)
            {
                const int nn = m.getNoteNumber();
                for (auto& v : voices)
                    if (v.isActive() && v.isMpe() && v.channel() == ch && v.note() == nn)
                        v.noteOff (pr.group[(size_t) v.group()]);
                return;
            }
            if (m.isPitchWheel() && member)
            {
                // -8192..+8191 mapped to ±mpeBendSemis (MPE default ±48).
                const float norm = (m.getPitchWheelValue() - 8192) / 8192.0f;
                const float semis = norm * pr.mpeBendSemis.load();
                for (auto& v : voices)
                    if (v.isActive() && v.isMpe() && v.channel() == ch)
                        v.setNoteBendSemis (semis);
                return;
            }
            if (m.isChannelPressure() && member)
            {
                const float v01 = m.getChannelPressureValue() / 127.0f;
                for (auto& v : voices)
                    if (v.isActive() && v.isMpe() && v.channel() == ch)
                        v.setNoteAftertouch (v01);
                return;
            }
            // CC74 → timbre (MPE Y axis)
            if (m.isController() && m.getControllerNumber() == 74 && member)
            {
                const float v01 = m.getControllerValue() / 127.0f;
                for (auto& v : voices)
                    if (v.isActive() && v.isMpe() && v.channel() == ch)
                        v.setNoteTimbre (v01);
                return;
            }
            // CC1 on master channel → set ALL active MPE voices' modwheel.
            if (m.isController() && m.getControllerNumber() == 1 && master)
            {
                const float v01 = m.getControllerValue() / 127.0f;
                for (auto& v : voices)
                    if (v.isActive() && v.isMpe()) v.setNoteTimbre (v01);
                return;
            }
            if (m.isAllNotesOff() || m.isAllSoundOff())
            {
                for (auto& v : voices) v.kill();
                return;
            }
            // Anything else on MPE channels — ignore.
            return;
        }

        // ── Non-MPE (legacy) channel-routed behaviour ──────────────────
        if (m.isNoteOn())
        {
            const int ch = m.getChannel();
            const int nn = m.getNoteNumber();
            const float vv = m.getFloatVelocity();

            if (ch == pr.presetChannel.load())
            {
                if (pr.presetUnison.load())
                {
                    for (int g = 0; g < state::kMaxGroups; ++g)
                        if (pr.groupPoly[g].load() > 0) noteOnGroup(g, nn, vv, pr);
                }
                else
                {
                    const int g = pickGroupForPreset(nn, pr);
                    if (g >= 0) noteOnGroup(g, nn, vv, pr);
                }
            }
            else
            {
                for (int g = 0; g < state::kMaxGroups; ++g)
                    if (pr.groupChannel[g].load() == ch && pr.groupPoly[g].load() > 0)
                        noteOnGroup(g, nn, vv, pr);
            }
        }
        else if (m.isNoteOff())
            noteOff(m.getNoteNumber(), m.getChannel(), pr);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) v.kill();
        else if (m.isController() && m.getControllerNumber() == 1)
        {
            const int ch = m.getChannel();
            const bool pres = (ch == pr.presetChannel.load());
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (pres || pr.groupChannel[g].load() == ch)
                    pr.group[(size_t) g].modwheel.store (m.getControllerValue() / 127.0f);
        }
        else if (m.isChannelPressure())
        {
            const int ch = m.getChannel();
            const bool pres = (ch == pr.presetChannel.load());
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (pres || pr.groupChannel[g].load() == ch)
                    pr.group[(size_t) g].aftertouch.store (m.getChannelPressureValue() / 127.0f);
        }
        else if (m.isAftertouch())
        {
            const int ch = m.getChannel();
            const bool pres = (ch == pr.presetChannel.load());
            for (int g = 0; g < state::kMaxGroups; ++g)
                if (pres || pr.groupChannel[g].load() == ch)
                    pr.group[(size_t) g].aftertouch.store (m.getAfterTouchValue() / 127.0f);
        }
    }

    void SynthEngine::processBlock (juce::AudioBuffer<float>& audio,
                                    juce::MidiBuffer& midi,
                                    state::Preset& preset,
                                    const float* extMono, int extLen)
    {
        const int numSamples = audio.getNumSamples();
        audio.clear();

        for (auto& v : voices)
            if (v.isActive())
                v.updateFromState(preset.group[(size_t) v.group()]);

        auto* L = audio.getWritePointer(0);
        auto* R = audio.getWritePointer(audio.getNumChannels() > 1 ? 1 : 0);

        auto it = midi.cbegin();
        const auto end = midi.cend();

        for (int i = 0; i < numSamples; ++i)
        {
            while (it != end && (*it).samplePosition <= i)
            {
                handleMidi ((*it).getMessage(), preset);
                ++it;
            }

            const float ext = (extMono != nullptr && i < extLen) ? extMono[i] : 0.0f;
            float sl = 0.0f, sr = 0.0f;

            // Quality switch: cap how many active voices we render this
            // sample. The cap walks down the voice array in order, so newly-
            // stolen voices (which the allocator places at the front of the
            // active pool) always get rendered.
            const int activeCap = preset.qualityVoiceCap();
            int rendered = 0;
            for (auto& v : voices)
            {
                if (! v.isActive()) continue;
                if (rendered >= activeCap) break;
                v.setExternalInput(ext);     // ignored unless that group's Ext In is on
                float l, r;
                v.renderSample(preset.group[(size_t) v.group()], l, r);
                sl += l; sr += r;
                ++rendered;
            }
            L[i] += sl;
            if (R != L) R[i] += sr;

            const float mono = (sl + sr) * 0.5f;
            snap.pushScope (mono);
            const float a = std::abs (mono);
            peakEnv = a > peakEnv ? a : peakEnv * peakReleaseCoeff;
        }

        snap.masterPeak.store (peakEnv, std::memory_order_relaxed);

        // Per-voice snapshot (control-rate).
        for (int i = 0; i < kNumVoices; ++i)
        {
            const auto& v = voices[(size_t) i];
            auto& vs = snap.voices[(size_t) i];
            vs.group .store (v.group(),  std::memory_order_relaxed);
            vs.note  .store (v.note(),   std::memory_order_relaxed);
            vs.age   .store (v.age(),    std::memory_order_relaxed);
            const float en = v.resonatorEnergy();
            vs.energy.store (en, std::memory_order_relaxed);
            vs.state .store (v.isHeld()    ? vis::EngineSnapshot::Held
                           : v.isRinging() ? vis::EngineSnapshot::Ringing
                           :                 vis::EngineSnapshot::Idle,
                             std::memory_order_relaxed);
            // A ringing voice with substantial loop energy is sustaining
            // itself rather than decaying - the loop has ignited.
            vs.selfOsc.store (v.isActive() && ! v.isHeld() && en > 0.1f,
                              std::memory_order_relaxed);
        }

        // Publish live mod-source values from the most-recently-started
        // active voice. Used by Knob::updateModRibbons to animate the dot
        // on each mod ribbon. If no voice is active, fall back to whatever
        // we last published — that way the dot lingers in place between
        // notes instead of snapping to zero.
        const Voice* primary = nullptr;
        uint64_t bestStamp = 0;
        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            if (primary == nullptr || v.age() >= bestStamp)
            { primary = &v; bestStamp = v.age(); }
        }
        if (primary != nullptr)
        {
            const auto& srcs = primary->lastSources();
            const int n = (int) std::min ((size_t) snap.liveSources.size(), srcs.size());
            for (int i = 0; i < n; ++i)
                snap.liveSources[(size_t) i].store (srcs[(size_t) i],
                                                     std::memory_order_relaxed);
        }
    }
}
