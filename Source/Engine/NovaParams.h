#pragma once
// NovaTone - zentrale Parametertabelle (JUCE-frei).
// Single source of truth: Engine, JUCE-Parameter-Layout, Editor und Tests lesen alle hieraus.
// Nur ASCII verwenden (MSVC-Kompatibilitaet).

#include <cstddef>

namespace nova
{
constexpr int kNumInstruments = 12;

enum ParamKind { kFloat = 0, kChoice = 1, kBool = 2 };

// X(Name, id, Anzeigename, min, max, default, step, Einheit, Art, Skew, Auswahlliste)
#define NOVA_PARAM_LIST(X) \
 X(Instrument,   "instrument",    "Instrument",        0.f, 11.f, 8.f, 1.f, "", kChoice, 1.f, \
   "Pick Bass|Finger Bass|Synth Bass - Fast|Synth Bass - Soft|Warm Analog Layer|Digital Layer|Atmospheric Layer|Wide Synth Layer|Concert Grand|Vintage Piano|Muted Grand|Classic Piano") \
 X(InstrumentGain,"instrumentGain","Instrument Level", -24.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(MasterGain,   "masterGain",    "Master",            -36.f, 6.f, -3.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Pan,          "pan",           "Pan",               -1.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(Attack,       "attack",        "Attack",            0.001f, 4.f, 0.002f, 0.001f, " s", kFloat, 0.35f, "") \
 X(Decay,        "decay",         "Decay",             0.005f, 8.f, 3.f, 0.001f, " s", kFloat, 0.35f, "") \
 X(Sustain,      "sustain",       "Sustain",           0.f, 1.f, 1.f, 0.01f, "", kFloat, 1.f, "") \
 X(Release,      "release",       "Release",           0.005f, 8.f, 0.3f, 0.001f, " s", kFloat, 0.35f, "") \
 X(WaveSine,     "waveSine",      "Sine",              0.f, 1.f, 0.15f, 0.01f, "", kFloat, 1.f, "") \
 X(WaveTriangle, "waveTriangle",  "Triangle",          0.f, 1.f, 0.35f, 0.01f, "", kFloat, 1.f, "") \
 X(WaveSaw,      "waveSaw",       "Saw",               0.f, 1.f, 0.5f, 0.01f, "", kFloat, 1.f, "") \
 X(WaveSquare,   "waveSquare",    "Square",            0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(Cutoff,       "cutoff",        "Filter Cutoff",     30.f, 20000.f, 18000.f, 1.f, " Hz", kFloat, 0.25f, "") \
 X(Resonance,    "resonance",     "Resonance",         0.1f, 1.5f, 0.2f, 0.01f, "", kFloat, 1.f, "") \
 X(FilterType,   "filterType",    "Filter Type",       0.f, 2.f, 0.f, 1.f, "", kChoice, 1.f, "Low Pass|High Pass|Band Pass") \
 X(FilterEnv,    "filterEnv",     "Filter Env Amount", -1.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(FilterDecay,  "filterDecay",   "Filter Env Decay",  0.02f, 4.f, 0.4f, 0.001f, " s", kFloat, 0.4f, "") \
 X(KeyTrack,     "keyTrack",      "Key Tracking",      0.f, 1.f, 0.4f, 0.01f, "", kFloat, 1.f, "") \
 X(LfoRate,      "lfoRate",       "LFO Rate",          0.01f, 20.f, 5.f, 0.01f, " Hz", kFloat, 0.4f, "") \
 X(LfoDepth,     "lfoDepth",      "LFO Depth",         0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(LfoTarget,    "lfoTarget",     "LFO Target",        0.f, 2.f, 0.f, 1.f, "", kChoice, 1.f, "Pitch|Filter|Amplitude") \
 X(Tune,         "tune",          "Fine Tune",         -100.f, 100.f, 0.f, 0.1f, " ct", kFloat, 1.f, "") \
 X(UnisonVoices, "unisonVoices",  "Unison Voices",     1.f, 7.f, 1.f, 1.f, "", kFloat, 1.f, "") \
 X(UnisonDetune, "unisonDetune",  "Unison Detune",     0.f, 1.f, 0.3f, 0.01f, "", kFloat, 1.f, "") \
 X(PlayMode,     "playMode",      "Play Mode",         0.f, 2.f, 0.f, 1.f, "", kChoice, 1.f, "Poly|Mono|Legato") \
 X(Glide,        "glide",         "Glide",             0.f, 2.f, 0.08f, 0.001f, " s", kFloat, 0.4f, "") \
 X(BendRange,    "bendRange",     "Bend Range",        0.f, 24.f, 2.f, 1.f, " st", kFloat, 1.f, "") \
 X(VelSens,      "velSens",       "Velocity Sens",     0.f, 1.f, 1.f, 0.01f, "", kFloat, 1.f, "") \
 X(DistType,     "distortionType","Distortion Type",   0.f, 3.f, 0.f, 1.f, "", kChoice, 1.f, "Soft Clip|Hard Clip|Saturation|Overdrive") \
 X(Drive,        "drive",         "Drive",             0.f, 24.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(DistMix,      "distortionMix", "Distortion Mix",    0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(Octave,       "octave",        "Octave Layer",      0.f, 4.f, 2.f, 1.f, "", kChoice, 1.f, "-2 Oct|-1 Oct|Original|+1 Oct|+2 Oct") \
 X(OctaveMix,    "octaveMix",     "Octaver Mix",       0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(StereoWidth,  "stereoWidth",   "Stereo Width",      0.f, 2.f, 1.f, 0.01f, "", kFloat, 1.f, "") \
 X(PanDepth,     "panDepth",      "Dynamic Pan Depth", 0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(PanRate,      "panRate",       "Dynamic Pan Rate",  0.01f, 10.f, 0.2f, 0.01f, " Hz", kFloat, 0.4f, "") \
 X(EqMode,       "eqMode",        "EQ Mode",           0.f, 1.f, 0.f, 1.f, "", kChoice, 1.f, "Modern|Classic") \
 X(EqLow,        "eqLow",         "EQ Low Shelf",      -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(EqLowMid,     "eqLowMid",      "EQ Low Mid",        -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(EqHighMid,    "eqHighMid",     "EQ High Mid",       -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(EqHigh,       "eqHigh",        "EQ High Shelf",     -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn1,         "dynBand1",      "Dynamic Band 1 (60 Hz)",   -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn2,         "dynBand2",      "Dynamic Band 2 (120 Hz)",  -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn3,         "dynBand3",      "Dynamic Band 3 (250 Hz)",  -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn4,         "dynBand4",      "Dynamic Band 4 (500 Hz)",  -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn5,         "dynBand5",      "Dynamic Band 5 (1 kHz)",   -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn6,         "dynBand6",      "Dynamic Band 6 (2.5 kHz)", -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn7,         "dynBand7",      "Dynamic Band 7 (5.5 kHz)", -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(Dyn8,         "dynBand8",      "Dynamic Band 8 (12 kHz)",  -12.f, 12.f, 0.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(DynAmount,    "dynamicAmount", "Dynamic EQ Amount", 0.f, 1.f, 0.5f, 0.01f, "", kFloat, 1.f, "") \
 X(CompMode,     "compMode",      "Compressor Mode",   0.f, 2.f, 2.f, 1.f, "", kChoice, 1.f, "Vintage|Tube|Modern") \
 X(CompThreshold,"compThreshold", "Comp Threshold",    -60.f, 0.f, -18.f, 0.1f, " dB", kFloat, 1.f, "") \
 X(CompRatio,    "compRatio",     "Comp Ratio",        1.f, 20.f, 2.f, 0.1f, ":1", kFloat, 0.5f, "") \
 X(CompAttack,   "compAttack",    "Comp Attack",       0.1f, 100.f, 10.f, 0.1f, " ms", kFloat, 0.4f, "") \
 X(CompRelease,  "compRelease",   "Comp Release",      10.f, 1000.f, 120.f, 1.f, " ms", kFloat, 0.4f, "") \
 X(CompMix,      "compMix",       "Comp Mix",          0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(ReverbType,   "reverbType",    "Reverb Type",       0.f, 7.f, 5.f, 1.f, "", kChoice, 1.f, \
   "Small Plate|Medium Plate|Large Plate|Small Room|Medium Room|Large Room|Church|Cathedral") \
 X(ReverbMix,    "reverbMix",     "Reverb Mix",        0.f, 1.f, 0.12f, 0.01f, "", kFloat, 1.f, "") \
 X(ReverbSize,   "reverbSize",    "Reverb Decay",      0.f, 1.f, 0.55f, 0.01f, "", kFloat, 1.f, "") \
 X(ReverbDamp,   "reverbDamp",    "Reverb Damping",    0.f, 1.f, 0.4f, 0.01f, "", kFloat, 1.f, "") \
 X(DelayTime,    "delayTime",     "Delay Time",        0.01f, 2.f, 0.35f, 0.001f, " s", kFloat, 0.5f, "") \
 X(DelayFeedback,"delayFeedback", "Delay Feedback",    0.f, 0.92f, 0.25f, 0.01f, "", kFloat, 1.f, "") \
 X(DelayMix,     "delayMix",      "Delay Mix",         0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(ChorusRate,   "chorusRate",    "Chorus Rate",       0.05f, 8.f, 0.25f, 0.01f, " Hz", kFloat, 0.5f, "") \
 X(ChorusDepth,  "chorusDepth",   "Chorus Depth",      0.f, 1.f, 0.25f, 0.01f, "", kFloat, 1.f, "") \
 X(ChorusMix,    "chorusMix",     "Chorus Mix",        0.f, 1.f, 0.f, 0.01f, "", kFloat, 1.f, "") \
 X(Limiter,      "limiter",       "Limiter",           0.f, 1.f, 1.f, 1.f, "", kBool, 1.f, "")

enum ParamId : int
{
#define NOVA_X(n, ...) p##n,
    NOVA_PARAM_LIST(NOVA_X)
#undef NOVA_X
    pCount
};

struct ParamInfo
{
    const char* id;
    const char* name;
    float minValue, maxValue, defaultValue, step;
    const char* unit;
    int kind;
    float skew;
    const char* choices; // '|' getrennt
};

inline const ParamInfo* paramTable()
{
    static const ParamInfo table[] = {
#define NOVA_X(n, id, name, mn, mx, df, st, unit, kind, skew, ch) { id, name, mn, mx, df, st, unit, kind, skew, ch },
        NOVA_PARAM_LIST(NOVA_X)
#undef NOVA_X
    };
    return table;
}

inline const char* const* instrumentNames()
{
    static const char* const n[kNumInstruments] = {
        "Pick Bass", "Finger Bass", "Synth Bass - Fast", "Synth Bass - Soft",
        "Warm Analog Layer", "Digital Layer", "Atmospheric Layer", "Wide Synth Layer",
        "Concert Grand", "Vintage Piano", "Muted Grand", "Classic Piano" };
    return n;
}

inline const char* const* sampleFolderNames()
{
    static const char* const n[kNumInstruments] = {
        "Bass_Pick", "Bass_Finger", "Bass_SynthFast", "Bass_SynthSoft",
        "Synth_WarmLayer", "Synth_DigitalLayer", "Synth_AtmosphericLayer", "Synth_WideLayer",
        "Piano_Concert", "Piano_Vintage", "Piano_MutedGrand", "Piano_Classic" };
    return n;
}
} // namespace nova
