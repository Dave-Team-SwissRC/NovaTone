#pragma once
#include "NovaCommon.h"

namespace nova
{
// Cytomic/TPT State-Variable-Filter (stabil bei Modulation)
struct SvfCoefs
{
    float g = 0.1f, k = 1.4f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void set (float fc, float q, float sr)
    {
        fc = clampf (fc, 15.0f, sr * 0.45f);
        g = std::tan (kPi * fc / sr);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
};

struct SvfState
{
    float s1 = 0.0f, s2 = 0.0f;
    inline void process (const SvfCoefs& c, float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - s2;
        const float v1 = c.a1 * s1 + c.a2 * v3;
        const float v2 = s2 + c.a2 * s1 + c.a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        bp = v1; lp = v2; hp = v0 - c.k * v1 - v2;
    }
    void reset() { s1 = s2 = 0.0f; }
};

// Biquad (double) fuer EQ-Baender, RBJ-Cookbook
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline float process (float x)
    {
        const double in = x;
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return (float) out;
    }
    void reset() { z1 = z2 = 0; }

    void setCoefs (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static double clampFreq (double f, double sr) { return std::min (std::max (f, 10.0), sr * 0.45); }

    void setPeak (double sr, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (f, sr) / sr;
        const double al = std::sin (w0) / (2.0 * std::max (0.1, q)), c = std::cos (w0);
        setCoefs (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void setLowShelf (double sr, double f, double gainDb, double q = 0.707)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (f, sr) / sr;
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        setCoefs (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                  (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void setHighShelf (double sr, double f, double gainDb, double q = 0.707)
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (f, sr) / sr;
        const double c = std::cos (w0), al = std::sin (w0) / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        setCoefs (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                  (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    void setBandpass (double sr, double f, double q) // 0 dB Spitzenverstaerkung
    {
        const double w0 = 2.0 * 3.14159265358979323846 * clampFreq (f, sr) / sr;
        const double al = std::sin (w0) / (2.0 * std::max (0.1, q)), c = std::cos (w0);
        setCoefs (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
};
} // namespace nova
