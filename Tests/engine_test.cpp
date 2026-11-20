// Offline-Test der NovaTone-Engine (ohne JUCE). Build: siehe Tests/run_tests.sh
#include "../Source/Engine/NovaEngine.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#if defined(__SSE__)
 #include <xmmintrin.h>
 #include <pmmintrin.h>
#endif

using namespace nova;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

struct Stereo { std::vector<float> l, r; };

static void setDefaults (float* p) { for (int i = 0; i < pCount; ++i) p[i] = paramTable()[i].defaultValue; }

static void dump (const std::string& name, const Stereo& s)
{
    std::string path = "/tmp/nova/" + name + ".f32";
    FILE* f = std::fopen (path.c_str(), "wb");
    if (!f) return;
    for (size_t i = 0; i < s.l.size(); ++i) { float v[2] = { s.l[i], s.r[i] }; std::fwrite (v, sizeof (float), 2, f); }
    std::fclose (f);
}

static float peakOf (const Stereo& s, size_t a = 0, size_t b = (size_t) -1)
{
    float p = 0; b = std::min (b, s.l.size());
    for (size_t i = a; i < b; ++i) p = std::max ({ p, std::fabs (s.l[i]), std::fabs (s.r[i]) });
    return p;
}
static bool allFinite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i) if (!std::isfinite (s.l[i]) || !std::isfinite (s.r[i])) return false;
    return true;
}

// Einfache Notenliste: {sampleStart, note, vel, isOn}
struct Ev { int at; int type; int a; int b; }; // type 0 on, 1 off, 2 bend(b=-8192..8191), 3 cc
static Stereo run (Engine& e, const std::vector<Ev>& evs, int total, int block)
{
    Stereo out; out.l.assign ((size_t) total, 0.f); out.r.assign ((size_t) total, 0.f);
    size_t ei = 0; int pos = 0;
    while (pos < total)
    {
        const int n = std::min (block, total - pos);
        int done = 0;
        while (done < n)
        {
            int next = n;
            while (ei < evs.size() && evs[ei].at < pos + done) ++ei;
            if (ei < evs.size() && evs[ei].at < pos + n) next = evs[ei].at - pos;
            if (next > done) { e.render (out.l.data() + pos + done, out.r.data() + pos + done, next - done); done = next; }
            while (ei < evs.size() && evs[ei].at == pos + done)
            {
                const Ev& v = evs[ei++];
                if (v.type == 0) e.noteOn (v.a, v.b / 127.0f);
                else if (v.type == 1) e.noteOff (v.a);
                else if (v.type == 2) e.pitchBend ((float) v.b / 8192.0f);
                else e.controller (v.a, v.b);
            }
            if (done >= n) break;
        }
        pos += n;
    }
    return out;
}

static Stereo note (int inst, int n, int vel, float hold, float total, float sr, const std::vector<std::pair<int, float>>& ov = {})
{
    Engine e; e.prepare (sr);
    float P[pCount]; setDefaults (P); P[pInstrument] = (float) inst;
    for (auto& o : ov) P[o.first] = o.second;
    e.setParams (P);
    return run (e, { { 0, 0, n, vel }, { (int) (hold * sr), 1, n, 0 } }, (int) (total * sr), 512);
}

int main()
{
#if defined(__SSE__)
    _mm_setcsr (_mm_getcsr() | 0x8040);
#endif
    std::printf ("== 1. Parametertabelle\n");
    {
        std::set<std::string> ids;
        const ParamInfo* t = paramTable();
        for (int i = 0; i < pCount; ++i)
        {
            CHECK (ids.insert (t[i].id).second, "doppelte ID %s", t[i].id);
            CHECK (t[i].defaultValue >= t[i].minValue && t[i].defaultValue <= t[i].maxValue, "Default ausserhalb: %s", t[i].id);
            if (t[i].kind == kChoice) { int c = 1; for (const char* p = t[i].choices; *p; ++p) c += *p == '|'; CHECK ((int) t[i].maxValue + 1 == c, "Choice-Anzahl %s: %d vs %d", t[i].id, (int) t[i].maxValue + 1, c); }
        }
        std::printf ("  %d Parameter, %d Presets\n", (int) pCount, presetCount());
        for (int p = 0; p < presetCount(); ++p)
        {
            float v[pCount]; presetValues (p, v);
            for (int i = 0; i < pCount; ++i) CHECK (v[i] >= t[i].minValue - 1e-4f && v[i] <= t[i].maxValue + 1e-4f, "Preset '%s': %s=%g ausserhalb", presetName (p), t[i].id, v[i]);
        }
    }

    std::printf ("== 2. Alle Instrumente x Noten x Velocities: Pegel + NaN\n");
    {
        std::printf ("  %-22s %5s %4s %8s %8s\n", "Instrument", "Note", "Vel", "Peak dB", "Tail dB");
        for (int inst = 0; inst < kNumInstruments; ++inst)
            for (int n : { 36, 60, 84 })
                for (int vel : { 40, 120 })
                {
                    Stereo s = note (inst, n, vel, 1.0f, 2.5f, 48000);
                    CHECK (allFinite (s), "NaN/Inf %s n=%d", instrumentNames()[inst], n);
                    const float pk = peakOf (s), tail = peakOf (s, 100000);
                    std::printf ("  %-22s %5d %4d %8.1f %8.1f\n", instrumentNames()[inst], n, vel, gainToDb (pk), gainToDb (tail));
                    CHECK (pk <= 1.0f, "Clipping");
                    CHECK (pk > 0.005f, "zu leise: %s n=%d v=%d", instrumentNames()[inst], n, vel);
                    if (vel == 120 && n == 60) dump (std::string ("inst") + std::to_string (inst) + "_n60_v120", s);
                    if (vel == 120 && n == 36) dump (std::string ("inst") + std::to_string (inst) + "_n36_v120", s);
                    if (vel == 120 && n == 84) dump (std::string ("inst") + std::to_string (inst) + "_n84_v120", s);
                    if (vel == 40  && n == 60) dump (std::string ("inst") + std::to_string (inst) + "_n60_v40", s);
                }
    }

    std::printf ("== 3. Blockgroessen-Unabhaengigkeit (1, 7, 64, 512, 4096)\n");
    {
        std::vector<Ev> evs;
        const int sr = 48000;
        for (int i = 0; i < 40; ++i) { evs.push_back ({ i * 2000 + (i * 37) % 300, 0, 40 + (i * 7) % 40, 50 + (i * 13) % 70 }); evs.push_back ({ i * 2000 + 1500 + (i * 11) % 200, 1, 40 + (i * 7) % 40, 0 }); }
        std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
        for (int inst : { 0, 4, 8 })
        {
            float P[pCount]; setDefaults (P); P[pInstrument] = (float) inst; P[pReverbMix] = 0.3f; P[pChorusMix] = 0.3f; P[pDelayMix] = 0.2f; P[pDistMix] = 0.5f; P[pDrive] = 6; P[pLfoDepth] = 0.5f; P[pCompMix] = 1;
            Stereo ref; double worst = 0;
            for (int blk : { 4096, 512, 64, 7, 1 })
            {
                Engine e; e.prepare (sr); e.setParams (P);
                Stereo s = run (e, evs, 90000, blk);
                if (blk == 4096) ref = s;
                else for (size_t i = 0; i < s.l.size(); ++i) worst = std::max (worst, (double) std::fabs (s.l[i] - ref.l[i]));
            }
            std::printf ("  %-18s max. Abweichung: %.2e\n", instrumentNames()[inst], worst);
            CHECK (worst < 1e-4, "blockgroessenabhaengig (%g)", worst);
        }
    }

    std::printf ("== 4. Limiter-Garantie bei extremem Pegel\n");
    {
        float worst = 0;
        for (int inst : { 2, 7, 8 })
        {
            Engine e; e.prepare (48000);
            float P[pCount]; setDefaults (P); P[pInstrument] = (float) inst; P[pInstrumentGain] = 12; P[pMasterGain] = 6; P[pDrive] = 24; P[pDistMix] = 1; P[pUnisonVoices] = 7;
            e.setParams (P);
            std::vector<Ev> evs; for (int n = 30; n < 90; n += 3) evs.push_back ({ 0, 0, n, 127 });
            Stereo s = run (e, evs, 48000, 256);
            worst = std::max (worst, peakOf (s));
            CHECK (allFinite (s), "NaN");
        }
        std::printf ("  max. Ausgangspegel: %.3f (%.2f dBFS), Ceiling 0.966\n", worst, gainToDb (worst));
        CHECK (worst <= 0.9661f, "Limiter ueberschritten");
    }

    std::printf ("== 5. Stress: 200 Noten + Pedal + Bend + Zufalls-CC, 20 s, Echtzeit-Faktor\n");
    {
        Engine e; e.prepare (48000);
        float P[pCount]; setDefaults (P); P[pReverbMix] = 0.3f; P[pChorusMix] = 0.3f; P[pDelayMix] = 0.2f; P[pCompMix] = 1; P[pDyn3] = -6; P[pDyn6] = 6; P[pEqLow] = 3;
        Rng r; r.s = 99;
        std::vector<Ev> evs;
        for (int i = 0; i < 200; ++i)
        {
            const int at = (int) (r.uni() * 900000), nn = 24 + (int) (r.uni() * 72);
            evs.push_back ({ at, 0, nn, 20 + (int) (r.uni() * 107) });
            evs.push_back ({ at + 2000 + (int) (r.uni() * 90000), 1, nn, 0 });
        }
        for (int i = 0; i < 40; ++i) { evs.push_back ({ (int) (r.uni() * 900000), 2, 0, (int) ((r.uni() - 0.5f) * 16000) }); evs.push_back ({ (int) (r.uni() * 900000), 3, 64, r.uni() > 0.5f ? 127 : 0 }); evs.push_back ({ (int) (r.uni() * 900000), 3, 1, (int) (r.uni() * 127) }); }
        std::sort (evs.begin(), evs.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
        for (int inst : { 0, 4, 7, 8 })
        {
            P[pInstrument] = (float) inst; e.reset(); e.setParams (P);
            auto t0 = std::chrono::steady_clock::now();
            Stereo s = run (e, evs, 960000, 256);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-20s render %.2fs fuer 20s Audio -> %.1fx Echtzeit, Peak %.2f\n", instrumentNames()[inst], sec, 20.0 / sec, peakOf (s));
            CHECK (allFinite (s), "NaN im Stress"); CHECK (peakOf (s) <= 0.9661f, "Peak");
        }
    }

    std::printf ("== 6. Mono/Legato + Glide, Pitchbend, Sustain-Pedal, Oktav-Layer\n");
    {
        Stereo a = note (2, 40, 100, 0.5f, 1.0f, 48000, { { pPlayMode, 2 }, { pGlide, 0.2f } });
        dump ("mono_check", a);
        Engine e; e.prepare (48000);
        float P[pCount]; setDefaults (P); P[pInstrument] = 3; P[pPlayMode] = 2; P[pGlide] = 0.3f; e.setParams (P);
        Stereo g = run (e, { { 0, 0, 40, 100 }, { 24000, 0, 52, 100 }, { 48000, 1, 52, 0 }, { 60000, 1, 40, 0 } }, 96000, 256);
        dump ("legato_glide", g);
        std::printf ("  Aktive Stimmen nach Legato-Lauf: %d\n", e.activeVoices());
        Stereo p = note (0, 45, 100, 0.2f, 1.0f, 48000);
        Engine e2; e2.prepare (48000); float P2[pCount]; setDefaults (P2); P2[pInstrument] = 8; e2.setParams (P2);
        Stereo ped = run (e2, { { 0, 3, 64, 127 }, { 100, 0, 60, 100 }, { 12000, 1, 60, 0 }, { 60000, 3, 64, 0 } }, 96000, 256);
        const float before = peakOf (ped, 20000, 40000), after = peakOf (ped, 90000, 96000);
        std::printf ("  Pedal: Pegel waehrend Pedal %.1f dB, nach Loslassen (Release) %.1f dB\n", gainToDb (before), gainToDb (after));
        CHECK (before > 0.01f, "Pedal haelt Note nicht"); CHECK (after < before * 0.1f, "Pedal-Release greift nicht");
        Stereo ob = note (0, 45, 100, 0.5f, 1.0f, 48000, { { pOctave, 4 }, { pOctaveMix, 0.5f } });
        dump ("octave_up2", ob); dump ("pick_a2_ref", p);
        Stereo bd; { Engine e3; e3.prepare (48000); float P3[pCount]; setDefaults (P3); P3[pInstrument] = 8; P3[pBendRange] = 12; e3.setParams (P3);
          bd = run (e3, { { 0, 0, 60, 100 }, { 12000, 2, 0, 8191 } }, 48000, 256); dump ("bend_up_octave", bd); }
    }

    std::printf ("== 7. Effekte einzeln: Latenz Drive, Reverb-Abklingzeit, Kompressor, Dyn-EQ\n");
    {
        Drive d; d.prepare (48000);
        int firstPeak = -1; float pk = 0;
        for (int i = 0; i < 200; ++i) { float l = i == 20 ? 1.0f : 0.0f, r = l; d.process (l, r, 0, 2, 1.0f); if (std::fabs (l) > pk) { pk = std::fabs (l); firstPeak = i - 20; } }
        std::printf ("  Drive-Impuls: Maximum nach %d Samples (deklarierte Latenz %d), Betrag %.3f\n", firstPeak, Drive::kLatency, pk);
        Drive d2; d2.prepare (48000); int dryPeak = -1; pk = 0;
        for (int i = 0; i < 200; ++i) { float l = i == 20 ? 1.0f : 0.0f, r = l; d2.process (l, r, 0, 2, 0.0f); if (std::fabs (l) > pk) { pk = std::fabs (l); dryPeak = i - 20; } }
        std::printf ("  Dry-Pfad: Maximum nach %d Samples\n", dryPeak);
        CHECK (firstPeak == Drive::kLatency && dryPeak == Drive::kLatency, "Latenz Drive/Dry ungleich: wet %d dry %d", firstPeak, dryPeak);

        // Reverb RT60: Impuls, Energie-Abfall
        for (int t : { 0, 3, 5, 7 })
        {
            Reverb rv; rv.prepare (48000); rv.set (t, 0.5f, 0.4f);
            std::vector<float> o; for (int i = 0; i < 48000 * 10; ++i) { float l = i == 0 ? 1.f : 0.f, r = l; rv.process (l, r, 1.0f); o.push_back (l); }
            auto rms = [&] (int a, int b) { double s = 0; for (int i = a; i < b; ++i) s += o[(size_t) i] * o[(size_t) i]; return std::sqrt (s / (b - a)); };
            const double e1 = rms (24000, 28000), e2 = rms (24000 + 48000, 28000 + 48000);
            std::printf ("  Reverb Typ %d: RMS bei 0.5s %.1f dB, bei 1.5s %.1f dB, Ende(9s) %.1f dB\n", t, 20 * std::log10 (e1 + 1e-12), 20 * std::log10 (e2 + 1e-12), 20 * std::log10 (rms (48000 * 9, 48000 * 9 + 4000) + 1e-12));
            for (float v : o) CHECK (std::isfinite (v), "Reverb NaN");
        }
        // Kompressor: Sinus 0 dBFS-> GR
        Compressor c; c.prepare (48000);
        float last = 0;
        for (int i = 0; i < 48000; ++i) { float l = 0.5f * std::sin (0.05f * (float) i), r = l; c.process (l, r, 2, -20.0f, 4.0f, 5.0f, 100.0f, 1.0f); last = c.gainReductionDb(); }
        std::printf ("  Kompressor: -6 dBFS Sinus, Thr -20, Ratio 4 -> GR %.2f dB (erwartet ca. -10.5)\n", last);
        CHECK (std::fabs (last + 10.5f) < 1.0f, "GR unerwartet");
        // Dyn-EQ: leiser vs. lauter 1 kHz-Ton, Band 5 +9 dB
        for (float amp : { 0.003f, 0.3f })
        {
            DynEq8 q; q.prepare (48000); float g[8] = { 0, 0, 0, 0, 9, 0, 0, 0 };
            for (int i = 0; i < 48000; ++i) { float l = amp * std::sin (kTwoPi * 1000.0f * (float) i / 48000.0f), r = l; q.process (l, r, g, 1.0f); }
            std::printf ("  Dyn-EQ Boost +9 dB bei Eingang %.1f dBFS -> wirksamer Boost %.2f dB\n", gainToDb (amp), q.currentGain (4));
        }
        for (float amp : { 0.003f, 0.3f })
        {
            DynEq8 q; q.prepare (48000); float g[8] = { 0, 0, 0, 0, -9, 0, 0, 0 };
            for (int i = 0; i < 48000; ++i) { float l = amp * std::sin (kTwoPi * 1000.0f * (float) i / 48000.0f), r = l; q.process (l, r, g, 1.0f); }
            std::printf ("  Dyn-EQ Cut -9 dB bei Eingang %.1f dBFS -> wirksamer Cut %.2f dB\n", gainToDb (amp), q.currentGain (4));
        }
    }

    std::printf ("== 8. Sample-Wiedergabe (synthetisches Sample C3 = Note 48, 440-Hz-Ton-unabhaengig)\n");
    {
        SampleBank bank; SampleData sd; sd.rate = 44100; sd.root = 48;
        const float f0 = midiToHz (48.0f);
        for (int i = 0; i < 44100 * 3; ++i) sd.l.push_back (0.5f * std::sin (kTwoPi * f0 * (float) i / 44100.0f) * std::exp (-(float) i / 44100.0f));
        bank.inst[8].push_back (sd);
        Engine e; e.prepare (48000); e.setSampleBank (&bank);
        float P[pCount]; setDefaults (P); P[pInstrument] = 8; P[pReverbMix] = 0; e.setParams (P);
        Stereo s = run (e, { { 0, 0, 48, 127 }, { 48000, 1, 48, 0 }, { 60000, 0, 60, 127 } }, 120000, 256);
        dump ("sample_c3_then_c4", s);
        CHECK (allFinite (s), "NaN");
    }

    std::printf ("\n%s (%d Fehler)\n", failures == 0 ? "ALLE TESTS BESTANDEN" : "FEHLER", failures);
    return failures == 0 ? 0 : 1;
}
