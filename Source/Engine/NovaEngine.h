#pragma once
// NovaTone Engine - komplett JUCE-frei, header-only (C++17).
// Wird von der JUCE-Huelle (PluginProcessor) und vom Offline-Test (Tests/engine_test.cpp) benutzt.
#include "NovaFx.h"
#include "NovaParams.h"
#include "NovaPresets.h"
#include "NovaVoice.h"
#include <array>
#include <cstring>
#include <vector>

namespace nova
{
class Engine
{
public:
    static constexpr int kVoices = 64;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v.prepare (sr);
        drive.prepare (sr); eq.prepare (sr); dyn.prepare (sr); comp.prepare (sr);
        chorus.prepare (sr); delay.prepare (sr); reverb.prepare (sr); limiter.prepare (sr);
        for (int i = 0; i < pCount; ++i) P[i] = paramTable()[i].defaultValue;
        sInst.setTime (0.02f, sr); sMaster.setTime (0.02f, sr); sPan.setTime (0.02f, sr); sWidth.setTime (0.02f, sr);
        sComp.setTime (0.03f, sr); sRev.setTime (0.03f, sr); sDelay.setTime (0.03f, sr); sChorus.setTime (0.03f, sr);
        updateTargets(); snapSmoothers();
        reset();
    }

    int latencySamples() const { return Drive::kLatency + limiter.latency(); }

    void reset()
    {
        for (auto& v : voices) v.active = false;
        heldKeys.clear(); pedal = false; bend = 0; modWheel = 0; cc74 = 0; volCC = 1.0f; exprCC = 1.0f;
        tickLeft = 0; lfoPh = 0; panPh = 0; ageCounter = 0;
        drive.reset(); eq.reset(); dyn.reset(); comp.reset(); chorus.reset(); delay.reset(); reverb.reset(); limiter.reset();
    }

    void setParams (const float* p)
    {
        std::memcpy (P, p, sizeof (float) * (size_t) pCount);
        updateTargets();
    }
    void setSampleBank (const SampleBank* b) { bank = b; for (auto& v : voices) if (v.active) v.kill(); }

    // ------------------------------------------------------------ MIDI
    void noteOn (int note, float vel)
    {
        const int inst = (int) (P[pInstrument] + 0.5f);
        const int mode = (int) (P[pPlayMode] + 0.5f);
        const bool monoMode = mode != 0 && (inst <= 7 || bank != nullptr);
        vel = clampf (vel, 0.01f, 1.0f);

        if (monoMode)
        {
            removeKey (note); heldKeys.push_back (note);
            lastVel = vel;
            bool legatoHandled = false;
            if (mode == 2)
                for (auto& v : voices)
                    if (v.active && v.mono && v.keyDown && !v.isReleasing()) { v.retarget (note); legatoHandled = true; }
            if (!legatoHandled) startMono (note, vel);
            return;
        }
        spawn (note, 0, vel, 1.0f, inst, false);
        spawnOctave (note, vel, inst, false);
    }

    void noteOff (int note)
    {
        const int mode = (int) (P[pPlayMode] + 0.5f);
        bool anyMono = false;
        for (auto& v : voices) if (v.active && v.mono) anyMono = true;
        if (anyMono || (mode != 0 && !heldKeys.empty()))
        {
            removeKey (note);
            if (!heldKeys.empty())
            {
                const int back = heldKeys.back();
                if (mode == 2) { for (auto& v : voices) if (v.active && v.mono && !v.isReleasing()) v.retarget (back); }
                else if (mode == 1) startMono (back, lastVel);
                return;
            }
            for (auto& v : voices) if (v.active && v.mono && v.keyDown) releaseVoice (v);
            return;
        }
        for (auto& v : voices)
            if (v.active && !v.mono && v.keyDown && v.key == note) releaseVoice (v);
    }

    void pitchBend (float norm) { bend = clampf (norm, -1.0f, 1.0f); }

    void controller (int cc, int value)
    {
        const float n = (float) value / 127.0f;
        switch (cc)
        {
            case 1:  modWheel = n; break;
            case 7:  volCC = n * n; break;
            case 11: exprCC = n * n; break;
            case 74: cc74 = (n - 0.5f) * 2.0f; break;
            case 64:
                if (value >= 64) pedal = true;
                else { pedal = false; for (auto& v : voices) if (v.active && v.pedal) { v.pedal = false; v.release(); } }
                break;
            case 120: case 123: allNotesOff(); break;
            default: break;
        }
    }

    void allNotesOff()
    {
        heldKeys.clear();
        for (auto& v : voices) if (v.active) { v.pedal = false; v.release(); }
    }

    // ------------------------------------------------------------ Audio
    void render (float* L, float* R, int n)
    {
        int done = 0;
        while (done < n)
        {
            if (tickLeft == 0) { controlTick(); tickLeft = kCtrlRate; }
            const int m = std::min (n - done, tickLeft);
            float* l = L + done; float* r = R + done;
            for (int i = 0; i < m; ++i) l[i] = r[i] = 0.0f;
            for (auto& v : voices) if (v.active) v.render (l, r, m, shared, rng);
            for (int i = 0; i < m; ++i) fxSample (l[i], r[i]);
            tickLeft -= m; done += m;
        }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) if (v.active) ++c; return c; }
    float lastGainReductionDb() const { return comp.gainReductionDb(); }
    float dynEqGain (int band) const { return dyn.currentGain (band); }
    const float* params() const { return P; }

private:
    float sr = 44100.0f;
    float P[pCount] {};
    std::array<Voice, kVoices> voices;
    std::vector<int> heldKeys;
    const SampleBank* bank = nullptr;
    Rng rng;
    Shared shared;
    bool pedal = false;
    float bend = 0, modWheel = 0, cc74 = 0, volCC = 1, exprCC = 1, lastVel = 1;
    int tickLeft = 0;
    float lfoPh = 0, panPh = 0;
    uint64_t ageCounter = 0;

    Drive drive; Eq4 eq; DynEq8 dyn; Compressor comp; Chorus chorus; StereoDelay delay; Reverb reverb; Limiter limiter;
    Smooth sInst, sMaster, sPan, sWidth, sComp, sRev, sDelay, sChorus;
    float eqGains[4] {}, dynGains[8] {};

    void removeKey (int k) { heldKeys.erase (std::remove (heldKeys.begin(), heldKeys.end(), k), heldKeys.end()); }

    void releaseVoice (Voice& v)
    {
        if (pedal) { v.pedal = true; v.keyDown = false; }
        else v.release();
    }

    Voice* allocVoice()
    {
        for (auto& v : voices) if (!v.active) return &v;
        Voice* best = nullptr;
        for (auto& v : voices) if (v.isReleasing() && (!best || v.age < best->age)) best = &v;
        if (!best) { best = &voices[0]; for (auto& v : voices) if (v.age < best->age) best = &v; }
        return best;
    }

    void spawn (int note, int off, float vel, float lvl, int inst, bool mono)
    {
        const int eff = note + off;
        if (eff < 0 || eff > 127) return;
        Voice* v = allocVoice();
        refreshShared();
        v->start (shared, note, off, vel, lvl, inst, mono, ++ageCounter, rng);
    }

    void spawnOctave (int note, float vel, int inst, bool mono)
    {
        const int oi = (int) (P[pOctave] + 0.5f);
        const float mix = P[pOctaveMix];
        if (oi != 2 && mix > 0.001f) spawn (note, (oi - 2) * 12, vel, mix, inst, mono);
    }

    void startMono (int note, float vel)
    {
        const int inst = (int) (P[pInstrument] + 0.5f);
        for (auto& v : voices) if (v.active && v.mono) v.kill();
        spawn (note, 0, vel, 1.0f, inst, true);
        spawnOctave (note, vel, inst, true);
    }

    void refreshShared()
    {
        shared.sr = sr; shared.P = P; shared.bend = bend * P[pBendRange];
        shared.modWheel = modWheel; shared.cc74 = cc74; shared.bank = bank;
    }

    void controlTick()
    {
        lfoPh += P[pLfoRate] * (float) kCtrlRate / sr;
        if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        shared.lfo = std::sin (kTwoPi * lfoPh);
        refreshShared();
        for (auto& v : voices) if (v.active) v.control (shared, true);
    }

    void updateTargets()
    {
        sInst.t = dbToGain (P[pInstrumentGain]);
        sMaster.t = dbToGain (P[pMasterGain]);
        sPan.t = P[pPan];
        sWidth.t = P[pStereoWidth];
        sComp.t = P[pCompMix];
        sRev.t = P[pReverbMix];
        sDelay.t = P[pDelayMix];
        sChorus.t = P[pChorusMix];
        eqGains[0] = P[pEqLow]; eqGains[1] = P[pEqLowMid]; eqGains[2] = P[pEqHighMid]; eqGains[3] = P[pEqHigh];
        for (int i = 0; i < 8; ++i) dynGains[i] = P[pDyn1 + i];
        eq.set ((int) (P[pEqMode] + 0.5f), eqGains);
        reverb.set ((int) (P[pReverbType] + 0.5f), P[pReverbSize], P[pReverbDamp]);
    }
    void snapSmoothers()
    {
        sInst.reset (sInst.t); sMaster.reset (sMaster.t); sPan.reset (sPan.t); sWidth.reset (sWidth.t);
        sComp.reset (sComp.t); sRev.reset (sRev.t); sDelay.reset (sDelay.t); sChorus.reset (sChorus.t);
    }

    inline void fxSample (float& l, float& r)
    {
        const float ig = sInst.next() * volCC * exprCC;
        l *= ig; r *= ig;

        drive.process (l, r, P[pDrive], (int) (P[pDistType] + 0.5f), P[pDistMix]);
        eq.process (l, r);
        dyn.process (l, r, dynGains, P[pDynAmount]);
        comp.process (l, r, (int) (P[pCompMode] + 0.5f), P[pCompThreshold], P[pCompRatio], P[pCompAttack], P[pCompRelease], sComp.next());
        const float cm = sChorus.next();
        if (cm > 1.0e-4f || sChorus.t > 1.0e-4f) chorus.process (l, r, P[pChorusRate], P[pChorusDepth], cm);
        const float dm = sDelay.next();
        if (dm > 1.0e-4f || sDelay.t > 1.0e-4f) delay.process (l, r, P[pDelayTime], P[pDelayFeedback], dm);
        reverb.process (l, r, sRev.next());

        // Breite (Mid/Side)
        const float w = sWidth.next();
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        l = mid + side; r = mid - side;

        // Pan (Balance, Mittelstellung = unveraendert) + dynamisches Pan
        float pan = sPan.next();
        if (P[pPanDepth] > 0.0005f)
        {
            panPh += P[pPanRate] / sr; if (panPh >= 1.0f) panPh -= 1.0f;
            pan = clampf (pan + P[pPanDepth] * std::sin (kTwoPi * panPh), -1.0f, 1.0f);
        }
        if (pan != 0.0f)
        {
            const float th = (pan + 1.0f) * kPi * 0.25f;
            l *= std::cos (th) * 1.41421356f; r *= std::sin (th) * 1.41421356f;
        }

        const float mg = sMaster.next();
        l = sanitize (l * mg); r = sanitize (r * mg);
        limiter.process (l, r, P[pLimiter] > 0.5f, 0.966f);
    }
};
} // namespace nova
