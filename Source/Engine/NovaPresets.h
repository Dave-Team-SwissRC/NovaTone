#pragma once
#include "NovaParams.h"
#include <algorithm>
#include <utility>
#include <vector>

namespace nova
{
struct PresetDef
{
    const char* name;
    std::vector<std::pair<int, float>> overrides;
};

inline const std::vector<PresetDef>& presets()
{
    static const std::vector<PresetDef> list = {
        { "Init - Concert Grand", {} },
        { "Studio Grand", { { pInstrument, 8 }, { pReverbMix, 0.18f }, { pReverbType, 4 }, { pCompMix, 0.35f }, { pCompThreshold, -22 }, { pCompRatio, 2.5f } } },
        { "Vintage Keys", { { pInstrument, 9 }, { pChorusMix, 0.22f }, { pChorusDepth, 0.3f }, { pReverbMix, 0.14f }, { pEqHighMid, -1.5f }, { pEqLow, 1.5f } } },
        { "Felt Piano", { { pInstrument, 10 }, { pReverbMix, 0.2f }, { pReverbType, 3 }, { pRelease, 0.2f }, { pEqHigh, -2 } } },
        { "Classic Upright", { { pInstrument, 11 }, { pReverbMix, 0.1f }, { pReverbType, 3 }, { pEqLowMid, 1.0f } } },
        { "Church Piano", { { pInstrument, 8 }, { pReverbType, 6 }, { pReverbMix, 0.38f }, { pReverbSize, 0.9f }, { pRelease, 0.5f } } },
        { "Deep Pick Bass", { { pInstrument, 0 }, { pPlayMode, 1 }, { pDrive, 4 }, { pDistMix, 0.2f }, { pDistType, 2 }, { pCompMix, 0.7f }, { pCompThreshold, -20 }, { pCompRatio, 4 }, { pReverbMix, 0.03f }, { pRelease, 0.15f } } },
        { "Soft Finger Bass", { { pInstrument, 1 }, { pPlayMode, 2 }, { pGlide, 0.05f }, { pCompMix, 0.5f }, { pCompThreshold, -18 }, { pReverbMix, 0.02f }, { pRelease, 0.2f }, { pEqLow, 2 } } },
        { "Fast Synth Bass", { { pInstrument, 2 }, { pPlayMode, 1 }, { pCutoff, 600 }, { pResonance, 0.45f }, { pFilterEnv, 0.6f }, { pFilterDecay, 0.18f },
                                { pDecay, 0.3f }, { pSustain, 0.8f }, { pWaveSaw, 0.7f }, { pWaveSquare, 0.3f }, { pWaveTriangle, 0 }, { pWaveSine, 0 }, { pDrive, 5 }, { pDistMix, 0.25f }, { pDistType, 3 }, { pReverbMix, 0.02f } } },
        { "Soft Synth Bass", { { pInstrument, 3 }, { pPlayMode, 2 }, { pGlide, 0.1f }, { pCutoff, 900 }, { pResonance, 0.3f }, { pFilterEnv, 0.25f }, { pFilterDecay, 0.5f },
                                { pWaveSine, 0.3f }, { pWaveTriangle, 0.5f }, { pWaveSaw, 0.3f }, { pReverbMix, 0.04f } } },
        { "Warm Analog Pad", { { pInstrument, 4 }, { pAttack, 0.6f }, { pDecay, 2 }, { pSustain, 0.8f }, { pRelease, 1.8f }, { pCutoff, 3500 }, { pResonance, 0.25f }, { pFilterEnv, 0.2f },
                                { pFilterDecay, 1.5f }, { pUnisonVoices, 5 }, { pUnisonDetune, 0.35f }, { pChorusMix, 0.25f }, { pReverbMix, 0.28f } } },
        { "Glass Digital Layer", { { pInstrument, 5 }, { pAttack, 0.02f }, { pDecay, 1.5f }, { pSustain, 0.6f }, { pRelease, 0.9f }, { pCutoff, 9000 }, { pUnisonVoices, 2 },
                                    { pChorusMix, 0.18f }, { pDelayMix, 0.15f }, { pDelayTime, 0.375f }, { pReverbMix, 0.22f } } },
        { "Atmospheric Wash", { { pInstrument, 6 }, { pAttack, 1.2f }, { pDecay, 3 }, { pSustain, 0.85f }, { pRelease, 3.5f }, { pCutoff, 4500 }, { pLfoTarget, 1 }, { pLfoRate, 0.15f },
                                 { pLfoDepth, 0.25f }, { pUnisonVoices, 4 }, { pReverbType, 7 }, { pReverbMix, 0.4f }, { pReverbSize, 0.85f }, { pChorusMix, 0.2f } } },
        { "Wide Synth Stack", { { pInstrument, 7 }, { pAttack, 0.01f }, { pDecay, 1 }, { pSustain, 0.75f }, { pRelease, 0.6f }, { pCutoff, 6500 }, { pFilterEnv, 0.3f }, { pFilterDecay, 0.6f },
                                 { pUnisonVoices, 7 }, { pUnisonDetune, 0.5f }, { pStereoWidth, 1.4f }, { pDelayMix, 0.14f }, { pReverbMix, 0.2f } } },
        { "Octave Pluck Bass", { { pInstrument, 0 }, { pOctave, 3 }, { pOctaveMix, 0.35f }, { pPlayMode, 1 }, { pCompMix, 0.5f }, { pReverbMix, 0.05f } } },
        { "Piano + Pad Layer", { { pInstrument, 8 }, { pOctave, 3 }, { pOctaveMix, 0.2f }, { pChorusMix, 0.15f }, { pReverbType, 6 }, { pReverbMix, 0.3f }, { pReverbSize, 0.8f } } },
    };
    return list;
}

inline int presetCount() { return (int) presets().size(); }
inline const char* presetName (int i) { return presets()[(size_t) std::max (0, std::min (presetCount() - 1, i))].name; }

// Alle Parameter: erst Defaults, dann Overrides des Presets
inline void presetValues (int index, float* out)
{
    const ParamInfo* t = paramTable();
    for (int i = 0; i < pCount; ++i)
        out[i] = t[i].defaultValue;
    for (auto& o : presets()[(size_t) std::max (0, std::min (presetCount() - 1, index))].overrides)
        out[o.first] = o.second;
}
} // namespace nova
