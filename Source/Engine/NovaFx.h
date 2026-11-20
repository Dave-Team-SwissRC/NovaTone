#pragma once
#include "NovaCommon.h"
#include "NovaFilters.h"
#include <vector>

namespace nova
{
inline int nextPow2 (int v) { int p = 1; while (p < v) p <<= 1; return p; }

// ---------------------------------------------------------------- 2x Oversampling (Halfband-FIR, linearphasig)
struct HalfbandFir
{
    static constexpr int N = 31;
    float h[N] {}, hist[N] {};
    int pos = 0;
    HalfbandFir()
    {
        double sum = 0;
        double hd[N];
        for (int n = 0; n < N; ++n)
        {
            const double m = n - (N - 1) / 2;
            const double x = m * 0.5;
            const double sinc = x == 0.0 ? 1.0 : std::sin (3.14159265358979 * x) / (3.14159265358979 * x);
            const double w = 0.42 - 0.5 * std::cos (2 * 3.14159265358979 * n / (N - 1)) + 0.08 * std::cos (4 * 3.14159265358979 * n / (N - 1));
            hd[n] = 0.5 * sinc * w; sum += hd[n];
        }
        for (int n = 0; n < N; ++n) h[n] = (float) (hd[n] / sum);
    }
    inline float process (float x)
    {
        hist[pos] = x;
        float acc = 0.0f;
        int idx = pos;
        for (int k = 0; k < N; ++k) { acc += h[k] * hist[idx]; idx = idx == 0 ? N - 1 : idx - 1; }
        pos = pos + 1 == N ? 0 : pos + 1;
        return acc;
    }
    void reset() { for (auto& v : hist) v = 0; pos = 0; }
};

// ---------------------------------------------------------------- Drive
class Drive
{
public:
    static constexpr int kLatency = 15; // Samples @ Basisrate (per Test verifiziert)

    void prepare (float) { reset(); }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { up[c].reset(); down[c].reset(); dcx[c] = dcy[c] = 0; for (auto& d : dly[c]) d = 0; }
        wr = 0; mixS.reset (0);
    }

    inline void process (float& l, float& r, float driveDb, int type, float mixTarget)
    {
        mixS.t = mixTarget;
        const float mix = mixS.next();
        float in[2] = { l, r };
        // Dry-Pfad immer um die Latenz verzoegert
        float dry[2];
        for (int c = 0; c < 2; ++c)
        {
            dly[c][wr] = in[c];
            dry[c] = dly[c][(wr + kDly - (kLatency - 0)) % kDly];
        }
        wr = (wr + 1) % kDly;

        if (mix < 1.0e-4f && mixTarget < 1.0e-4f) { l = dry[0]; r = dry[1]; return; }

        const float g = dbToGain (driveDb);
        const float comp = 1.0f / std::max (1.0f, std::sqrt (g));
        float wet[2];
        for (int c = 0; c < 2; ++c)
        {
            const float u0 = up[c].process (in[c]) * 2.0f;
            const float u1 = up[c].process (0.0f) * 2.0f;
            const float a = shape (u0 * g, type) * comp;
            const float b = shape (u1 * g, type) * comp;
            down[c].process (a);
            float y = down[c].process (b);
            // DC-Blocker (Overdrive erzeugt Offset)
            const float o = y - dcx[c] + 0.995f * dcy[c];
            dcx[c] = y; dcy[c] = o;
            wet[c] = o;
        }
        l = dry[0] * (1.0f - mix) + wet[0] * mix;
        r = dry[1] * (1.0f - mix) + wet[1] * mix;
    }

private:
    static constexpr int kDly = 64;
    HalfbandFir up[2], down[2];
    float dcx[2] {}, dcy[2] {};
    float dly[2][kDly] {};
    int wr = 0;
    Smooth mixS;

    static inline float shape (float x, int type)
    {
        switch (type)
        {
            case 0: return x / (1.0f + std::fabs (x));
            case 1: return clampf (x, -1.0f, 1.0f);
            case 2: return std::tanh (x);
            default: return std::tanh (x + 0.25f * x * x * 0.5f + 0.2f) - std::tanh (0.2f);
        }
    }
};

// ---------------------------------------------------------------- 4-Band EQ (echte Shelves)
class Eq4
{
public:
    void prepare (float s) { sr = s; for (auto& v : last) v = 1.0e9f; lastMode = -1; reset(); }
    void reset() { for (auto& b : bq) for (auto& c : b) c.reset(); }

    void set (int mode, const float* g)
    {
        const double q = mode == 0 ? 1.0 : 0.6;
        for (int i = 0; i < 4; ++i)
        {
            if (g[i] == last[i] && mode == lastMode) continue;
            last[i] = g[i];
            active[i] = std::fabs (g[i]) > 0.01f;
            for (int c = 0; c < 2; ++c)
            {
                if (i == 0) bq[i][c].setLowShelf (sr, 110.0, g[i]);
                else if (i == 3) bq[i][c].setHighShelf (sr, 9500.0, g[i]);
                else bq[i][c].setPeak (sr, i == 1 ? 430.0 : 2200.0, q, g[i]);
            }
        }
        lastMode = mode;
    }
    inline void process (float& l, float& r)
    {
        for (int i = 0; i < 4; ++i)
            if (active[i]) { l = bq[i][0].process (l); r = bq[i][1].process (r); }
    }
private:
    float sr = 44100.0f, last[4] {};
    int lastMode = -1;
    bool active[4] {};
    Biquad bq[4][2];
};

// ---------------------------------------------------------------- Dynamischer 8-Band-EQ
class DynEq8
{
public:
    static constexpr int N = 8;
    void prepare (float s)
    {
        sr = s;
        static const double F[N] = { 60, 120, 250, 500, 1000, 2500, 5500, 12000 };
        for (int i = 0; i < N; ++i)
        {
            freq[i] = F[i];
            det[i].setBandpass (sr, F[i], 1.1);
            env[i] = 0; eff[i] = 0; applied[i] = 1.0e9f;
        }
        ca = 1.0f - std::exp (-1.0f / (0.003f * sr));
        cr = 1.0f - std::exp (-1.0f / (0.060f * sr));
        reset();
    }
    void reset() { for (int i = 0; i < N; ++i) { det[i].reset(); pk[i][0].reset(); pk[i][1].reset(); env[i] = 0; } tick = 0; }

    inline void process (float& l, float& r, const float* gains, float amount)
    {
        const float m = 0.5f * (l + r);
        const bool doTick = (++tick & 31) == 0;
        for (int i = 0; i < N; ++i)
        {
            const float G = gains[i];
            if (std::fabs (G) < 0.05f && std::fabs (eff[i]) < 0.05f) continue;
            const float a = std::fabs (det[i].process (m));
            env[i] += (a > env[i] ? ca : cr) * (a - env[i]);
            if (doTick)
            {
                const float over = clampf ((gainToDb (env[i]) + 38.0f) / 18.0f, 0.0f, 1.0f);
                const float k = G > 0 ? (1.0f - amount) + amount * (1.0f - over) : (1.0f - amount) + amount * over;
                eff[i] += (G * k - eff[i]) * 0.25f;
                if (std::fabs (eff[i] - applied[i]) > 0.02f)
                {
                    applied[i] = eff[i];
                    pk[i][0].setPeak (sr, freq[i], 1.1, eff[i]);
                    pk[i][1].setCoefs (pk[i][0].b0, pk[i][0].b1, pk[i][0].b2, 1.0, pk[i][0].a1, pk[i][0].a2);
                }
            }
            if (applied[i] < 1.0e8f) { l = pk[i][0].process (l); r = pk[i][1].process (r); }
        }
    }
    float currentGain (int i) const { return eff[i]; }
private:
    float sr = 44100.0f, ca = 0.01f, cr = 0.001f;
    double freq[N] {};
    Biquad det[N], pk[N][2];
    float env[N] {}, eff[N] {}, applied[N] {};
    int tick = 0;
};

// ---------------------------------------------------------------- Kompressor
class Compressor
{
public:
    void prepare (float s) { sr = s; grDb = 0; }
    void reset() { grDb = 0; }
    inline void process (float& l, float& r, int mode, float thr, float ratio, float attMs, float relMs, float mix)
    {
        if (mix < 0.0005f && grDb > -0.01f) { return; }
        const float kneeW = mode == 0 ? 9.0f : (mode == 1 ? 6.0f : 2.0f);
        const float am = mode == 0 ? 2.0f : 1.0f, rm = mode == 0 ? 1.5f : 1.0f;
        const float lvl = gainToDb (std::max (std::fabs (l), std::fabs (r)));
        const float over = lvl - thr;
        float out;
        if (2.0f * over < -kneeW) out = lvl;
        else if (2.0f * std::fabs (over) <= kneeW) out = lvl + (1.0f / ratio - 1.0f) * (over + kneeW * 0.5f) * (over + kneeW * 0.5f) / (2.0f * kneeW);
        else out = thr + over / ratio;
        const float target = out - lvl;
        const float ms = target < grDb ? attMs * am : relMs * rm;
        grDb += (1.0f - std::exp (-1.0f / (std::max (0.05f, ms) * 0.001f * sr))) * (target - grDb);
        const float makeup = -0.5f * thr * (1.0f - 1.0f / ratio);
        const float gain = dbToGain (grDb + makeup);
        float cl = l * gain, cr_ = r * gain;
        if (mode == 1)
        {
            cl = std::tanh (1.15f * cl + 0.06f) / 1.15f - 0.0521f;
            cr_ = std::tanh (1.15f * cr_ + 0.06f) / 1.15f - 0.0521f;
        }
        l = l * (1.0f - mix) + cl * mix;
        r = r * (1.0f - mix) + cr_ * mix;
    }
    float gainReductionDb() const { return grDb; }
private:
    float sr = 44100.0f, grDb = 0.0f;
};

// ---------------------------------------------------------------- Stereo-Chorus
class Chorus
{
public:
    void prepare (float s)
    {
        sr = s;
        size = nextPow2 ((int) (0.06f * s) + 8);
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        wr = 0; ph = 0;
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); }
    inline void process (float& l, float& r, float rate, float depth, float mix)
    {
        buf[0][(size_t) wr] = l; buf[1][(size_t) wr] = r;
        ph += rate / sr; if (ph >= 1.0f) ph -= 1.0f;
        float w[2];
        for (int c = 0; c < 2; ++c)
        {
            const float lfo = std::sin (kTwoPi * (ph + 0.25f * (float) c));
            const float d = (0.012f + depth * 0.008f * lfo) * sr;
            const float rp = (float) wr - d;
            int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
            const float a = buf[c][(size_t) (i0 & (size - 1))], b = buf[c][(size_t) ((i0 + 1) & (size - 1))];
            w[c] = a + (b - a) * fr;
        }
        wr = (wr + 1) & (size - 1);
        l = l * (1.0f - 0.3f * mix) + w[0] * mix;
        r = r * (1.0f - 0.3f * mix) + w[1] * mix;
    }
private:
    float sr = 44100.0f, ph = 0.0f;
    int size = 4096, wr = 0;
    std::vector<float> buf[2];
};

// ---------------------------------------------------------------- Stereo-Delay (geglaetteter Zeitparameter)
class StereoDelay
{
public:
    void prepare (float s)
    {
        sr = s; size = nextPow2 ((int) (2.2f * s));
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        wr = 0; lp[0] = lp[1] = 0; tS.setTime (0.08f, s); tS.reset (0.35f * s);
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); lp[0] = lp[1] = 0; }
    inline void process (float& l, float& r, float timeSec, float fb, float mix)
    {
        tS.t = clampf (timeSec, 0.005f, 2.0f) * sr;
        const float d = tS.next();
        const float rp = (float) wr - d;
        const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
        float o[2];
        for (int c = 0; c < 2; ++c)
        {
            const float a = buf[c][(size_t) (i0 & (size - 1))], b = buf[c][(size_t) ((i0 + 1) & (size - 1))];
            o[c] = a + (b - a) * fr;
            lp[c] += 0.45f * (o[c] - lp[c]);
        }
        buf[0][(size_t) wr] = sanitize (l + fb * lp[0]);
        buf[1][(size_t) wr] = sanitize (r + fb * lp[1]);
        wr = (wr + 1) & (size - 1);
        l += o[0] * mix; r += o[1] * mix;
    }
private:
    float sr = 44100.0f, lp[2] {};
    int size = 4096, wr = 0;
    std::vector<float> buf[2];
    Smooth tS;
};

// ---------------------------------------------------------------- FDN-Hall (8 Delaylinien, Householder-Matrix)
class Reverb
{
public:
    static constexpr int N = 8;
    struct Type { float scale, t60, preMs, damp; };

    void prepare (float s)
    {
        sr = s;
        const int sz = nextPow2 ((int) (0.17f * s) + 64);
        for (int i = 0; i < N; ++i)
        {
            lines[i].buf.assign ((size_t) sz, 0.0f); lines[i].mask = sz - 1;
            lines[i].ph = 0.13f * (float) i; lines[i].inc = (0.11f + 0.037f * (float) i) / s;
        }
        const int pz = nextPow2 ((int) (0.06f * s) + 8);
        for (auto& p : pre) { p.assign ((size_t) pz, 0.0f); }
        preMask = pz - 1;
        static const int apLen[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i)
        {
            const int len = std::max (8, (int) ((float) apLen[i] * s / 44100.0f));
            ap[i].buf.assign ((size_t) len, 0.0f); ap[i].idx = 0;
        }
        typeIdx = -1; reset();
    }
    void reset()
    {
        for (auto& l : lines) { std::fill (l.buf.begin(), l.buf.end(), 0.0f); l.lp = 0; l.w = 0; }
        for (auto& p : pre) std::fill (p.begin(), p.end(), 0.0f);
        for (auto& a : ap) std::fill (a.buf.begin(), a.buf.end(), 0.0f);
        preW = 0;
    }

    static Type typeParams (int t)
    {
        static const Type T[8] = {
            { 0.60f, 1.2f, 0.0f, 0.25f }, { 0.80f, 1.8f, 0.0f, 0.30f }, { 1.00f, 2.8f, 0.0f, 0.30f },
            { 0.55f, 0.5f, 3.0f, 0.50f }, { 0.80f, 0.9f, 6.0f, 0.50f }, { 1.10f, 1.5f, 10.0f, 0.45f },
            { 1.50f, 3.8f, 18.0f, 0.45f }, { 1.90f, 6.5f, 28.0f, 0.40f } };
        return T[std::max (0, std::min (7, t))];
    }

    void set (int type, float decay01, float damp01)
    {
        static const float baseMs[N] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
        const Type T = typeParams (type);
        if (type != typeIdx)
        {
            typeIdx = type;
            for (int i = 0; i < N; ++i) lines[i].len = baseMs[i] * 0.001f * sr * T.scale;
            preSamples = (int) (T.preMs * 0.001f * sr);
            reset();
        }
        const float t60 = T.t60 * (0.5f + decay01);
        for (int i = 0; i < N; ++i) lines[i].g = std::pow (10.0f, -3.0f * (lines[i].len / sr) / t60);
        damp = clampf (T.damp * 0.5f + damp01 * 0.6f, 0.02f, 0.95f);
    }

    inline void process (float& l, float& r, float mix)
    {
        if (mix < 1.0e-4f && silent) return;
        // Eingang: Vorverzoegerung + Diffusion
        pre[0][(size_t) preW] = l; pre[1][(size_t) preW] = r;
        const float inL = pre[0][(size_t) ((preW - preSamples) & preMask)];
        const float inR = pre[1][(size_t) ((preW - preSamples) & preMask)];
        preW = (preW + 1) & preMask;
        const float dL = ap[1].process (ap[0].process (inL, 0.6f), 0.6f);
        const float dR = ap[3].process (ap[2].process (inR, 0.6f), 0.6f);

        float y[N], v[N], sum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            Line& ln = lines[i];
            ln.ph += ln.inc; if (ln.ph >= 1.0f) ln.ph -= 1.0f;
            const float d = ln.len + std::sin (kTwoPi * ln.ph) * 0.0004f * sr;
            const float rp = (float) ln.w - d;
            const int i0 = (int) std::floor (rp); const float fr = rp - (float) i0;
            const float a = ln.buf[(size_t) (i0 & ln.mask)], b = ln.buf[(size_t) ((i0 + 1) & ln.mask)];
            y[i] = a + (b - a) * fr;
            ln.lp = (1.0f - damp) * y[i] + damp * ln.lp;
            v[i] = ln.lp * ln.g;
            sum += v[i];
        }
        const float h = sum * 0.25f;
        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            Line& ln = lines[i];
            ln.buf[(size_t) ln.w] = sanitize (v[i] - h + ((i & 1) ? dR : dL) * 0.5f);
            ln.w = (ln.w + 1) & ln.mask;
            if (i & 1) oR += (i & 2 ? -y[i] : y[i]); else oL += (i & 2 ? -y[i] : y[i]);
        }
        oL *= 0.55f; oR *= 0.55f;
        silent = std::fabs (oL) + std::fabs (oR) < 1.0e-7f && std::fabs (l) + std::fabs (r) < 1.0e-7f;
        l = l * (1.0f - 0.25f * mix) + oL * mix;
        r = r * (1.0f - 0.25f * mix) + oR * mix;
    }
private:
    struct Line { std::vector<float> buf; int mask = 0, w = 0; float lp = 0, ph = 0, inc = 0, len = 100, g = 0.9f; };
    struct Ap
    {
        std::vector<float> buf; int idx = 0;
        inline float process (float x, float g)
        {
            const float d = buf[(size_t) idx];
            const float y = -g * x + d;
            buf[(size_t) idx] = x + g * y;
            idx = idx + 1 == (int) buf.size() ? 0 : idx + 1;
            return y;
        }
    };
    float sr = 44100.0f, damp = 0.4f;
    Line lines[N];
    Ap ap[4];
    std::vector<float> pre[2];
    int preMask = 0, preW = 0, preSamples = 0, typeIdx = -1;
    bool silent = true;
};

// ---------------------------------------------------------------- Lookahead-Limiter (mathematisch garantierte Obergrenze)
class Limiter
{
public:
    int latency() const { return N - 1; }
    void prepare (float s)
    {
        sr = s; N = std::max (8, (int) (0.0015f * s));
        gbuf.assign ((size_t) N, 1.0f); mbuf.assign ((size_t) N, 1.0f);
        sig[0].assign ((size_t) N, 0.0f); sig[1].assign ((size_t) N, 0.0f);
        idx = 0; gRel = 1.0f; sum = (double) N;
        relC = 1.0f - std::exp (-1.0f / (0.08f * s));
    }
    void reset() { prepare (sr); }
    inline void process (float& l, float& r, bool on, float ceiling)
    {
        const float peak = std::max (std::fabs (l), std::fabs (r));
        float target = (on && peak > ceiling) ? ceiling / peak : 1.0f;
        gRel = std::min (target, gRel + (1.0f - gRel) * relC);   // sofortiger Attack, langsamer Release
        gbuf[(size_t) idx] = gRel;
        float m = 1.0f;
        for (int k = 0; k < N; ++k) m = std::min (m, gbuf[(size_t) k]);
        sum += (double) m - (double) mbuf[(size_t) idx]; mbuf[(size_t) idx] = m;
        const float g = (float) (sum / (double) N);
        // Signal um N-1 Samples verzoegert lesen
        const int rd = (idx + 1) % N;           // aeltester Eintrag = N-1 Samples zurueck
        const float dl = sig[0][(size_t) rd], dr = sig[1][(size_t) rd];
        sig[0][(size_t) idx] = l; sig[1][(size_t) idx] = r;
        idx = (idx + 1) % N;
        l = dl * g; r = dr * g;
        if (on) { l = clampf (l, -ceiling, ceiling); r = clampf (r, -ceiling, ceiling); } // faengt nur Rundungsreste ab
        if (sum < 0.0) sum = 0.0;
    }
private:
    float sr = 44100.0f, gRel = 1.0f, relC = 0.001f;
    double sum = 0.0;
    int N = 72, idx = 0;
    std::vector<float> gbuf, mbuf, sig[2];
};
} // namespace nova
