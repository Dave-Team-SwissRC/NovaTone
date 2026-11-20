#pragma once
#include "NovaCommon.h"
#include "NovaFilters.h"
#include "NovaParams.h"
#include <array>
#include <vector>

namespace nova
{
constexpr int kCtrlRate = 16; // Samples pro Kontroll-Tick

struct SampleData
{
    std::vector<float> l, r; // r leer = mono
    double rate = 44100.0;
    int root = 60;
};

struct SampleBank
{
    std::array<std::vector<SampleData>, kNumInstruments> inst;
    const SampleData* find (int instrument, int note) const
    {
        const auto& v = inst[(size_t) instrument];
        const SampleData* best = nullptr;
        int bd = 1000;
        for (auto& s : v)
        {
            const int d = std::abs (s.root - note);
            if (d < bd) { bd = d; best = &s; }
        }
        return best;
    }
};

// Zustand, der fuer alle Stimmen gilt (pro Kontroll-Tick gesetzt)
struct Shared
{
    float sr = 44100.0f;
    const float* P = nullptr;
    float bend = 0.0f;     // Halbtoene
    float lfo = 0.0f;      // -1..1
    float modWheel = 0.0f; // 0..1
    float cc74 = 0.0f;     // -1..1
    const SampleBank* bank = nullptr;
};

namespace detail
{
inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float fastSin01 (float ph) // sin(2*pi*ph), ph in [0,1)
{
    const float x = (ph - 0.5f) * kTwoPi;                   // -pi..pi (entspricht sin(2*pi*ph - pi))
    const float y = 1.27323954f * x - 0.405284735f * x * std::fabs (x);
    return -(0.225f * (y * std::fabs (y) - y) + y);         // Vorzeichen: sin(a - pi) = -sin(a)
}
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}
inline float at (const std::vector<float>& v, int i)
{
    const int n = (int) v.size();
    return v[(size_t) (i < 0 ? 0 : (i >= n ? n - 1 : i))];
}
} // namespace detail

// Klangprofile der Synth-Instrumente (Index 2..7)
struct SynthProfile
{
    float sub, fm, noise, minAttack, minRelease, cutScale, fenvAdd, fenvDecayScale, drift;
    int minUnison;
    float minDetune;
};
inline const SynthProfile& synthProfile (int inst)
{
    static const SynthProfile p[6] = {
        // sub   fm    noise minAtt minRel cutSc fenvAdd fdScale drift unison detune
        { 0.45f, 0.0f, 0.0f, 0.001f, 0.05f, 1.0f, 0.35f, 0.5f, 0.0f, 1, 0.0f },   // 2 fast
        { 0.30f, 0.0f, 0.0f, 0.012f, 0.12f, 0.6f, 0.10f, 1.0f, 0.0f, 1, 0.0f },   // 3 soft
        { 0.0f, 0.0f, 0.0f, 0.010f, 0.30f, 0.8f, 0.0f, 1.0f, 4.0f, 3, 0.25f },    // 4 warm analog
        { 0.0f, 1.6f, 0.0f, 0.005f, 0.20f, 1.0f, 0.0f, 1.0f, 0.0f, 2, 0.15f },    // 5 digital
        { 0.0f, 0.0f, 0.06f, 0.250f, 1.50f, 0.8f, 0.0f, 1.0f, 6.0f, 4, 0.40f },   // 6 atmospheric
        { 0.0f, 0.0f, 0.0f, 0.005f, 0.30f, 1.0f, 0.0f, 1.0f, 3.0f, 7, 0.35f } };  // 7 wide
    return p[std::max (2, std::min (7, inst)) - 2];
}

struct PianoProfile { float bright, decayScale, inharm, hammer, even, detune; };
inline const PianoProfile& pianoProfile (int inst)
{
    static const PianoProfile p[4] = {
        { 1.00f, 1.00f, 1.0f, 1.0f, 0.0f, 1.0f },   // 8 Concert
        { 0.60f, 0.55f, 1.6f, 0.6f, 0.7f, 1.4f },   // 9 Vintage
        { 0.30f, 0.25f, 1.0f, 0.3f, 0.2f, 0.8f },   // 10 Muted
        { 0.80f, 0.80f, 1.3f, 0.8f, 0.1f, 1.2f } }; // 11 Classic
    return p[std::max (8, std::min (11, inst)) - 8];
}

class Voice
{
public:
    enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };
    enum class Kind : uint8_t { Synth, Pluck, Piano, Sample };

    static constexpr int kMaxPartials = 64;
    static constexpr int kMaxBeat = 16;

    bool active = false, keyDown = false, pedal = false, mono = false, companion = false;
    int key = 60, offset = 0, instrument = 0;
    uint64_t age = 0;
    Stage stage = Stage::Off;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        int sz = 1024;
        while ((float) sz < sampleRate / 14.0f + 16.0f) sz <<= 1;
        ks.assign ((size_t) sz, 0.0f);
        ksMask = sz - 1;
        active = false;
    }

    void start (const Shared& s, int key_, int off, float vel, float lvl, int inst, bool mono_, uint64_t age_, Rng& rng)
    {
        const float* P = s.P;
        vrng.s = rng.next() | 1u;
        active = true; keyDown = true; pedal = false; mono = mono_; companion = off != 0;
        key = key_; offset = off; instrument = inst; age = age_;
        velocity = vel; level = lvl;
        stage = Stage::Attack; env = 0.0f; fenv = 1.0f; fastRel = false;
        curNote = (float) (key + offset);
        fl[0].reset(); fl[1].reset(); fl[2].reset(); fl[3].reset();
        outEnv = 1.0f; samplesAlive = 0;

        const SampleData* sd = s.bank != nullptr ? s.bank->find (inst, key + offset) : nullptr;
        const float pitch0 = curNote + P[pTune] / 100.0f + s.bend;
        if (sd != nullptr)       { kind = Kind::Sample; sample = sd; samplePos = 0.0; }
        else if (inst <= 1)      { kind = Kind::Pluck; initPluck (s, pitch0, vrng); }
        else if (inst >= 8)      { kind = Kind::Piano; initPiano (s, pitch0, vrng); }
        else                     { kind = Kind::Synth; initSynth (s, vrng); }
        control (s, false);
    }

    void release()
    {
        keyDown = false;
        if (active && stage != Stage::Off && stage != Stage::Release) stage = Stage::Release;
    }
    void kill() { fastRel = true; keyDown = false; if (active) stage = Stage::Release; }
    void retarget (int newKey) { key = newKey; }
    bool isReleasing() const { return stage == Stage::Release; }

    // Pro Kontroll-Tick aufrufen (advance = Glide/Filterhuellkurve weiterschalten)
    void control (const Shared& s, bool advance)
    {
        const float* P = s.P;
        const float target = (float) (key + offset);
        if (advance)
        {
            if (mono && P[pGlide] > 0.0005f)
                curNote += (target - curNote) * (1.0f - std::exp (-(float) kCtrlRate / (s.sr * std::max (0.001f, P[pGlide]) / 3.0f)));
            else
                curNote = target;
        }
        const bool isSynth = kind == Kind::Synth;
        const SynthProfile* prof = isSynth ? &synthProfile (instrument) : nullptr;

        const float lfoDepth = clampf (P[pLfoDepth] + s.modWheel, 0.0f, 1.0f);
        const int lfoTarget = (int) (P[pLfoTarget] + 0.5f);
        float pitch = curNote + P[pTune] / 100.0f + s.bend;
        if (lfoTarget == 0) pitch += s.lfo * lfoDepth * 0.35f;
        const float freq = midiToHz (pitch);

        // --- Huellkurven-Koeffizienten
        const float att = std::max ({ P[pAttack], prof ? prof->minAttack : 0.001f, 0.001f });
        aInc = 1.0f / (att * s.sr);
        dC = std::exp (-6.9078f / (std::max (0.005f, P[pDecay]) * s.sr));
        sus = P[pSustain];
        const float rel = std::max (P[pRelease], prof ? prof->minRelease : 0.005f);
        rC = std::exp (-6.9078f / ((fastRel ? 0.004f : rel) * s.sr));
        const float fdec = std::max (0.02f, P[pFilterDecay] * (prof ? prof->fenvDecayScale : 1.0f));
        if (advance) fenv *= std::exp (-6.9078f * (float) kCtrlRate / (fdec * s.sr));

        // --- Filter
        const float amt = clampf (P[pFilterEnv] + (prof ? prof->fenvAdd : 0.0f), -1.0f, 1.0f);
        float oct = amt * 6.0f * fenv + P[pKeyTrack] * (pitch - 60.0f) / 12.0f + s.cc74 * 2.0f;
        if (lfoTarget == 1) oct += s.lfo * lfoDepth * 2.0f;
        if (isSynth) oct += (velocity - 1.0f) * P[pVelSens] * 1.5f;
        const float cutoff = clampf (P[pCutoff] * (prof ? prof->cutScale : 1.0f) * std::exp2 (oct), 20.0f, s.sr * 0.45f);
        const float res = P[pResonance];
        const float q = clampf (0.5f + res * res * 8.0f, 0.5f, 20.0f);
        filterType = (int) (P[pFilterType] + 0.5f);
        if (filterType == 2) { c2.set (cutoff, std::max (0.7f, q * 0.8f), s.sr); }
        else { c1.set (cutoff, 0.54f, s.sr); c2.set (cutoff, q, s.sr); }

        // --- Amp
        const float vs = P[pVelSens];
        ampGain = ((1.0f - vs) + vs * std::pow (velocity, 1.3f)) * level;
        ampMod = lfoTarget == 2 ? 1.0f - lfoDepth * 0.5f * (1.0f + s.lfo) : 1.0f;

        // --- Klangerzeuger-spezifisch
        switch (kind)
        {
            case Kind::Synth:
            {
                nUni = (int) clampf (std::floor (P[pUnisonVoices] + 0.5f), (float) prof->minUnison, 7.0f);
                const float det = std::max (P[pUnisonDetune], prof->minDetune) * 50.0f; // Cent
                const float wsum = P[pWaveSine] + P[pWaveTriangle] + P[pWaveSaw] + P[pWaveSquare];
                const float inv = wsum > 1.0e-4f ? 1.0f / wsum : 0.0f;
                wSine = wsum > 1.0e-4f ? P[pWaveSine] * inv : 1.0f;
                wTri = P[pWaveTriangle] * inv; wSaw = P[pWaveSaw] * inv; wSq = P[pWaveSquare] * inv;
                const float uniGain = 1.0f / std::sqrt ((float) nUni);
                for (int u = 0; u < nUni; ++u)
                {
                    const float pos = nUni > 1 ? ((float) u / (float) (nUni - 1)) * 2.0f - 1.0f : 0.0f;
                    const float cents = pos * det + drift[u];
                    inc[u] = freq * std::exp2 (cents / 1200.0f) / s.sr;
                    const float pan = pos * 0.85f * (nUni > 1 ? 1.0f : 0.0f);
                    const float th = (pan + 1.0f) * kPi * 0.25f;
                    uL[u] = std::cos (th) * 1.41421356f * uniGain;
                    uR[u] = std::sin (th) * 1.41421356f * uniGain;
                }
                subInc = freq * 0.5f / s.sr;
                fmInc = freq / s.sr;
                break;
            }
            case Kind::Pluck:
            {
                loopLen = clampf (s.sr / freq, 4.0f, (float) ksMask - 4.0f);
                const float t60 = clampf ((instrument == 0 ? 2.6f : 3.6f) * std::exp2 (-(pitch - 28.0f) / 30.0f), 0.35f, 8.0f);
                ksGain = std::pow (10.0f, -3.0f * (loopLen / s.sr) / t60);
                break;
            }
            case Kind::Piano:
            {
                const float ratio = std::exp2 ((pitch - startPitch) / 12.0f);
                if (std::fabs (ratio - lastRatio) > 1.0e-5f)
                {
                    lastRatio = ratio;
                    for (int k = 0; k < nPart; ++k)
                    {
                        const float w = kTwoPi * pf[k] * ratio / s.sr;
                        pcr[k] = std::cos (w); psr[k] = std::sin (w);
                        if (k < nBeat) { const float w2 = w * (1.0f + beatDelta); qcr[k] = std::cos (w2); qsr[k] = std::sin (w2); }
                    }
                }
                break;
            }
            case Kind::Sample:
                sampleRatio = std::exp2 ((pitch - (float) sample->root) / 12.0f) * (float) sample->rate / s.sr;
                break;
        }
    }

    // Rendert n <= kCtrlRate Samples und addiert in L/R
    void render (float* L, float* R, int n, const Shared& s, Rng&)
    {
        Rng& rng = vrng;   // eigener Zufallsgenerator pro Stimme (blockgroessen-unabhaengig)
        (void) s;
        for (int i = 0; i < n; ++i)
        {
            // Huellkurve
            switch (stage)
            {
                case Stage::Attack:  env += aInc; if (env >= 1.0f) { env = 1.0f; stage = Stage::Decay; } break;
                case Stage::Decay:   env = sus + (env - sus) * dC; if (std::fabs (env - sus) < 1.0e-4f) { env = sus; stage = Stage::Sustain; } break;
                case Stage::Sustain: env = sus; break;
                case Stage::Release: env *= rC; if (env < 1.0e-4f) { active = false; stage = Stage::Off; return; } break;
                case Stage::Off:     active = false; return;
            }

            float xl = 0.0f, xr = 0.0f;
            bool stereo = false;
            switch (kind)
            {
                case Kind::Synth:  stereo = true; synthSample (xl, xr, rng); break;
                case Kind::Pluck:  xl = xr = pluckSample (rng); break;
                case Kind::Piano:  xl = xr = pianoSample (rng); break;
                case Kind::Sample: stereo = sampleSample (xl, xr); if (!active) return; break;
            }

            // Filter
            float yl, yr;
            filterSample (0, xl, yl);
            if (stereo) filterSample (2, xr, yr); else yr = yl;

            const float g = env * ampGain * ampMod;
            const float oL = yl * g * pl, oR = yr * g * pr;
            L[i] += oL; R[i] += oR;

            // Selbst beenden, wenn Modell verklungen ist
            const float a = std::fabs (oL) + std::fabs (oR);
            outEnv = std::max (a, outEnv * 0.9997f);
            ++samplesAlive;
            if (kind == Kind::Piano && (samplesAlive & 255) == 0) renormPiano();
            if (samplesAlive > 4096 && outEnv < 2.0e-6f && kind != Kind::Synth) { active = false; stage = Stage::Off; return; }
        }
    }

private:
    float sr = 44100.0f;
    // gemeinsame Zustaende
    float velocity = 1.0f, level = 1.0f, curNote = 60.0f, env = 0.0f, fenv = 1.0f;
    float aInc = 0.01f, dC = 0.999f, rC = 0.999f, sus = 1.0f, ampGain = 1.0f, ampMod = 1.0f;
    float pl = 1.0f, pr = 1.0f;
    bool fastRel = false;
    Kind kind = Kind::Synth;
    int filterType = 0;
    SvfCoefs c1, c2;
    SvfState fl[4];
    float outEnv = 1.0f;
    int samplesAlive = 0;
    Rng vrng;

    // Synth
    int nUni = 1;
    float ph[7] {}, inc[7] {}, uL[7] {}, uR[7] {}, drift[7] {};
    float wSine = 1, wTri = 0, wSaw = 0, wSq = 0;
    float subPh = 0, subInc = 0, fmPh = 0, fmModPh = 0, fmInc = 0;

    // Pluck
    std::vector<float> ks;
    int ksMask = 1023, ksWr = 0;
    float loopLen = 100.0f, ksB = 0.25f, ksGain = 0.99f, ksPrev = 0.0f;
    float plSub = 0.0f, plSubC = 0.9999f, plSubPh = 0.0f;

    // Piano
    int nPart = 0, nBeat = 0;
    float pianoGain = 0.35f, pluckGain = 0.5f;
    float startPitch = 60.0f, lastRatio = 1.0f, beatDelta = 0.001f;
    float pf[kMaxPartials] {}, pc[kMaxPartials] {}, ps[kMaxPartials] {}, pcr[kMaxPartials] {}, psr[kMaxPartials] {};
    float pa1[kMaxPartials] {}, pa2[kMaxPartials] {}, pd1[kMaxPartials] {}, pd2[kMaxPartials] {};
    float qc[kMaxBeat] {}, qs[kMaxBeat] {}, qcr[kMaxBeat] {}, qsr[kMaxBeat] {};
    float hamEnv = 0.0f, hamC = 0.99f, hamAmp = 0.0f, hamLp = 0.0f, hamK = 0.3f;

    // Sample
    const SampleData* sample = nullptr;
    double samplePos = 0.0;
    float sampleRatio = 1.0f;

    // ------------------------------------------------------------------
    void initSynth (const Shared& s, Rng& rng)
    {
        const SynthProfile& pr_ = synthProfile (instrument);
        const int u = (int) clampf (std::floor (s.P[pUnisonVoices] + 0.5f), (float) pr_.minUnison, 7.0f);
        for (int i = 0; i < 7; ++i)
        {
            ph[i] = u > 1 ? rng.uni() : 0.0f;
            drift[i] = rng.bi() * pr_.drift;
        }
        subPh = fmPh = fmModPh = 0.0f;
        pl = pr = 1.0f;
    }

    void initPluck (const Shared& s, float pitch, Rng& rng)
    {
        const bool pick = instrument == 0;
        ksB = pick ? 0.20f : 0.38f;
        const float f0 = midiToHz (pitch);
        const float L = clampf (s.sr / f0, 4.0f, (float) ksMask - 4.0f);
        const int len = (int) std::ceil (L) + 2;
        const float pp = pick ? 0.12f : 0.27f;
        std::vector<float>& b = ks;
        std::fill (b.begin(), b.end(), 0.0f);
        const float noiseAmt = (pick ? 0.28f : 0.05f) * (0.4f + 0.6f * velocity);
        for (int i = 0; i < len; ++i)
        {
            const float x = (float) i / (float) len;
            b[(size_t) i] = (x < pp ? x / pp : (1.0f - x) / (1.0f - pp)) + noiseAmt * rng.bi();
        }
        const int passes = pick ? 1 : 4 - (int) (velocity * 2.0f);   // weicheres Anzupfen bei Finger/leise
        for (int p = 0; p < passes; ++p)
        {
            float prev = b[(size_t) len - 1];
            for (int i = 0; i < len; ++i)
            {
                const float cur = b[(size_t) i], nxt = b[(size_t) ((i + 1) % len)];
                b[(size_t) i] = 0.25f * prev + 0.5f * cur + 0.25f * nxt;
                prev = cur;
            }
        }
        float mean = 0.0f;
        for (int i = 0; i < len; ++i) mean += b[(size_t) i];
        mean /= (float) len;
        float peak = 1.0e-6f;
        for (int i = 0; i < len; ++i) { b[(size_t) i] -= mean; peak = std::max (peak, std::fabs (b[(size_t) i])); }
        for (int i = 0; i < len; ++i) b[(size_t) i] /= peak;
        ksWr = len; ksPrev = 0.0f;
        plSub = pick ? 0.12f : 0.28f; plSubPh = 0.0f;
        plSubC = std::exp (-6.9078f / (2.0f * s.sr));
        pl = pr = 1.0f;
        pluckGain = pick ? 0.75f : 0.6f;
        loopLen = L;
    }

    void initPiano (const Shared& s, float pitch, Rng& rng)
    {
        const PianoProfile& pp = pianoProfile (instrument);
        const float f0 = midiToHz (pitch);
        const float note = pitch;
        startPitch = pitch; lastRatio = 1.0f;
        const float B = clampf (1.0e-4f * std::exp2 ((note - 48.0f) / 10.0f), 5.0e-5f, 0.02f) * pp.inharm;
        const float fmax = std::min (12000.0f, s.sr * 0.45f);
        nPart = 0;
        for (int k = 1; k <= kMaxPartials; ++k)
        {
            const float fk = (float) k * f0 * std::sqrt (1.0f + B * (float) k * (float) k);
            if (fk > fmax) break;
            pf[nPart++] = fk;
        }
        if (nPart == 0) { nPart = 1; pf[0] = f0; }
        nBeat = std::min (nPart, kMaxBeat);

        const float a = 0.9f + (1.0f - velocity) * 0.9f + (1.0f - pp.bright) * 1.1f;
        float amp[kMaxPartials], sum = 0.0f;
        for (int k = 0; k < nPart; ++k)
        {
            const float kk = (float) (k + 1);
            float v = std::pow (kk, -a) * (0.35f + 0.65f * std::fabs (std::sin (kPi * kk * 0.125f)));
            if (k == 1) v *= 1.0f + pp.even;
            if (k >= 4) v *= 1.0f - 0.4f * pp.even;
            amp[k] = v; sum += v;
        }
        const float t60_1 = clampf (24.0f * std::exp2 (-(note - 21.0f) / 22.0f), 1.0f, 24.0f) * pp.decayScale;
        beatDelta = clampf (1.2f / f0, 0.0003f, 0.003f) * pp.detune;
        for (int k = 0; k < nPart; ++k)
        {
            const float v = amp[k] * 1.4f / sum;
            const float t60 = t60_1 / (1.0f + 0.22f * (float) k);
            pa1[k] = v * 0.55f; pa2[k] = v * 0.45f;
            pd1[k] = std::exp (-6.9078f / (t60 * 0.18f * s.sr));
            pd2[k] = std::exp (-6.9078f / (t60 * s.sr));
            pc[k] = 1.0f; ps[k] = 0.0f;
            const float w = kTwoPi * pf[k] / s.sr;
            pcr[k] = std::cos (w); psr[k] = std::sin (w);
            if (k < nBeat)
            {
                qc[k] = 1.0f; qs[k] = 0.0f;
                const float w2 = w * (1.0f + beatDelta);
                qcr[k] = std::cos (w2); qsr[k] = std::sin (w2);
            }
        }
        pianoGain = 0.35f * (1.0f - 0.3f * clampf ((pitch - 60.0f) / 24.0f, 0.0f, 1.0f));
        hamEnv = 1.0f;
        hamC = std::exp (-6.9078f / (0.012f * s.sr));
        hamAmp = pp.hammer * velocity * 0.10f * (0.5f + 0.5f * pp.bright);
        hamK = 1.0f - std::exp (-kTwoPi * (1500.0f + 4000.0f * velocity * pp.bright) / s.sr);
        hamLp = 0.0f;
        const float pan = clampf ((note - 60.0f) / 60.0f, -1.0f, 1.0f) * 0.35f;
        const float th = (pan + 1.0f) * kPi * 0.25f;
        pl = std::cos (th) * 1.41421356f; pr = std::sin (th) * 1.41421356f;
        (void) rng;
    }

    void renormPiano()
    {
        for (int k = 0; k < nPart; ++k)
        {
            const float f = 1.5f - 0.5f * (pc[k] * pc[k] + ps[k] * ps[k]);
            pc[k] *= f; ps[k] *= f;
        }
        for (int k = 0; k < nBeat; ++k)
        {
            const float f = 1.5f - 0.5f * (qc[k] * qc[k] + qs[k] * qs[k]);
            qc[k] *= f; qs[k] *= f;
        }
    }

    // ------------------------------------------------------------------
    inline void synthSample (float& l, float& r, Rng& rng)
    {
        for (int u = 0; u < nUni; ++u)
        {
            float p = ph[u] + inc[u];
            if (p >= 1.0f) p -= 1.0f;
            ph[u] = p;
            const float dt = inc[u];
            float o = 0.0f;
            if (wSine > 0.0f) o += wSine * detail::fastSin01 (p);
            if (wTri > 0.0f)  o += wTri * (4.0f * std::fabs (p - 0.5f) - 1.0f) * -1.0f;
            if (wSaw > 0.0f)  o += wSaw * (2.0f * p - 1.0f - detail::polyBlep (p, dt));
            if (wSq > 0.0f)
            {
                float p2 = p + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                o += wSq * ((p < 0.5f ? 1.0f : -1.0f) + detail::polyBlep (p, dt) - detail::polyBlep (p2, dt));
            }
            l += o * uL[u];
            r += o * uR[u];
        }
        const SynthProfile& pr_ = synthProfile (instrument);
        if (pr_.sub > 0.0f)
        {
            subPh += subInc; if (subPh >= 1.0f) subPh -= 1.0f;
            const float sbs = pr_.sub * std::sin (kTwoPi * subPh);
            l += sbs; r += sbs;
        }
        if (pr_.fm > 0.0f)
        {
            fmPh += fmInc; if (fmPh >= 1.0f) fmPh -= 1.0f;
            fmModPh += fmInc * 3.5f; if (fmModPh >= 1.0f) fmModPh -= std::floor (fmModPh);
            const float idx = pr_.fm * (0.25f + 0.75f * fenv);
            const float v = 0.4f * std::sin (kTwoPi * fmPh + idx * std::sin (kTwoPi * fmModPh));
            l += v; r += v;
        }
        if (pr_.noise > 0.0f)
        {
            const float n = pr_.noise * rng.bi();
            l += n; r += n;
        }
        l *= 0.45f; r *= 0.45f;
    }

    inline float pluckSample (Rng&)
    {
        const float D = loopLen - ksB;
        const int di = (int) D;
        const float fr = D - (float) di;
        const float x = ks[(size_t) ((ksWr - di) & ksMask)] * (1.0f - fr) + ks[(size_t) ((ksWr - di - 1) & ksMask)] * fr;
        const float y = (1.0f - ksB) * x + ksB * ksPrev;
        ksPrev = x;
        ks[(size_t) ksWr] = y * ksGain;
        ksWr = (ksWr + 1) & ksMask;
        // Sub-Sinus fuer Koerper
        plSubPh += kTwoPi / loopLen;                                   // gleiche Tonhoehe wie Saite
        plSubPh = plSubPh > kTwoPi ? plSubPh - kTwoPi : plSubPh;
        plSub *= plSubC;
        return (y * 0.55f + std::sin (plSubPh) * plSub) * pluckGain;
    }

    inline float pianoSample (Rng& rng)
    {
        float out = 0.0f;
        for (int k = 0; k < nPart; ++k)
        {
            float v = ps[k];
            if (k < nBeat) { v += 0.85f * qs[k]; const float nc = qc[k] * qcr[k] - qs[k] * qsr[k]; qs[k] = qs[k] * qcr[k] + qc[k] * qsr[k]; qc[k] = nc; }
            out += (pa1[k] + pa2[k]) * v;
            const float nc = pc[k] * pcr[k] - ps[k] * psr[k];
            ps[k] = ps[k] * pcr[k] + pc[k] * psr[k]; pc[k] = nc;
            pa1[k] *= pd1[k]; pa2[k] *= pd2[k];
        }
        hamEnv *= hamC;
        hamLp += hamK * (rng.bi() - hamLp);
        return out * pianoGain + hamLp * hamEnv * hamAmp;
    }

    inline bool sampleSample (float& l, float& r)
    {
        const int n = (int) sample->l.size();
        const int i = (int) samplePos;
        if (i >= n - 1) { active = false; stage = Stage::Off; return false; }
        const float t = (float) (samplePos - (double) i);
        l = detail::hermite (detail::at (sample->l, i - 1), sample->l[(size_t) i], detail::at (sample->l, i + 1), detail::at (sample->l, i + 2), t);
        if (sample->r.empty()) r = l;
        else r = detail::hermite (detail::at (sample->r, i - 1), sample->r[(size_t) i], detail::at (sample->r, i + 1), detail::at (sample->r, i + 2), t);
        samplePos += (double) sampleRatio;
        l *= 0.7f; r *= 0.7f;
        return true;
    }

    inline void filterSample (int slot, float x, float& y)
    {
        float lp, bp, hp;
        if (filterType == 2)
        {
            fl[slot].process (c2, x, lp, bp, hp);
            y = bp * c2.k * 1.0f;
            return;
        }
        fl[slot].process (c1, x, lp, bp, hp);
        const float a = filterType == 0 ? lp : hp;
        fl[slot + 1].process (c2, a, lp, bp, hp);
        y = filterType == 0 ? lp : hp;
    }
};
} // namespace nova
