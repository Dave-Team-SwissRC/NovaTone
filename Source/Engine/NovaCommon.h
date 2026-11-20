#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nova
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float dbToGain (float db) { return std::exp (db * 0.11512925464970229f); }
inline float gainToDb (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float midiToHz (float n) { return 440.0f * std::exp2 ((n - 69.0f) / 12.0f); }
inline float sanitize (float x) { return std::isfinite (x) ? x : 0.0f; }

struct Rng
{
    uint32_t s = 0x12345678u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi()  { return uni() * 2.0f - 1.0f; }                            // -1..1
};

// Einpol-Glaettung pro Sample
struct Smooth
{
    float v = 0.0f, t = 0.0f, c = 0.002f;
    void setTime (float seconds, float sr) { c = 1.0f - std::exp (-1.0f / std::max (1.0f, seconds * sr)); }
    void reset (float x) { v = t = x; }
    inline float next() { v += (t - v) * c; return v; }
    bool settled() const { return std::fabs (t - v) < 1.0e-5f; }
};
} // namespace nova
