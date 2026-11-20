# NovaTone 0.2

Hybrid-Instrument fuer Bass, Synth-Layer und Piano als **VST3 (Cubase)** und als **eigenstaendiges Programm (Standalone)** -
beides aus demselben Code, so wie HALion Sonic als Plugin und als Standalone laeuft.

> **Wichtig, bitte zuerst lesen:** Die Klang-Engine (`Source/Engine/`) wurde gebaut, kompiliert und ausfuehrlich getestet
> (siehe `VALIDATION.txt`). Die JUCE-Huelle (Plugin-Wrapper, Oberflaeche, CMake) konnte in der Entwicklungsumgebung **nicht
> kompiliert** werden (kein JUCE, kein Netzwerk). Sie ist sorgfaeltig gegen die JUCE-API geschrieben, aber ein erster
> Build kann trotzdem kleine Compilerfehler zeigen. Wenn ja: Fehlermeldung kopieren, dann ist das schnell behoben.

## Was bei v0.1 nicht stimmte (und jetzt behoben ist)

| Thema | v0.1 | v0.2 |
|---|---|---|
| Standalone | Build-Preset baute nur `NovaTone_VST3` - das Standalone-Programm wurde nie erzeugt | Preset, Skripte und CI bauen **VST3 + Standalone** |
| Windows-Build | `COPY_PLUGIN_AFTER_BUILD TRUE` bricht ohne Admin-Rechte ab | Kopieren optional, separates `INSTALL-WINDOWS.ps1` |
| Master/Limiter | Master-Gain wurde zweimal angewendet; "Limiter" war ein `tanh` | Echter Lookahead-Limiter (-0.3 dBFS, mathematisch garantiert, 1.5 ms) |
| Filter | Ein globaler 1-Pol-Tiefpass fuer alle Stimmen, Resonanz nur simuliert | Pro Stimme ein TPT-State-Variable-Filter (24 dB LP/HP, BP) mit echter Resonanz, Filter-Huellkurve, Key-Tracking |
| Dynamic EQ | Statischer EQ (Gain nur leicht skaliert), nicht pegelabhaengig | Echter dynamischer EQ: Cut greift bei Lautstaerke, Boost zieht sich bei Lautstaerke zurueck |
| EQ | "Shelf"-Baender waren Peak-Filter | Echte Low-/High-Shelves |
| Klangerzeugung | Einfache Oszillatoren | Bass: Saiten-Modell (Karplus-Strong) ; Piano: modale Synthese mit Inharmonizitaet, 2-stufigem Abklingen, Hammer-Rauschen, Velocity-Helligkeit ; Synth: PolyBLEP, Unison |
| Samples | Synchron in `prepareToPlay` geladen (kann Cubase beim Laden blockieren); Dateiname C3 = Note 48 | Im Hintergrund-Thread geladen; Cubase-Konvention **C3 = 60** (Dateien aus v0.1 ggf. 1 Oktave umbenennen) |
| Spielbarkeit | Keine Pitchbend-/Controller-Behandlung (nur Noten) | Pitchbend (Bereich einstellbar), Modrad (CC1), Sustain-Pedal (CC64), CC7/11/74, All-Notes-Off, Mono/Legato + Glide |
| Presets | 12 Presets, die nur ca. 16 Parameter setzten (Rest blieb vom vorigen Preset haengen) | 16 Werkspresets, die **alle** Parameter setzen (inkl. Spielmodus, Filter, Unison, EQ), als Host-Programme waehlbar |
| Oktav-Layer | Nur ein Sinus, nur fuer Synth-Stimmen (nicht fuer Samples) | Zusaetzliche Stimme mit derselben Klangerzeugung (Modell, Synth oder Sample) |
| Verzerrung | ohne Oversampling | 2x-Oversampling (Latenz wird an die Host-PDC gemeldet) |

## Installation

### Weg A - ohne lokale Entwicklungsumgebung (empfohlen): GitHub Actions
1. Ordner als neues GitHub-Repository hochladen (privat reicht).
2. Tab **Actions**, der Workflow `Build NovaTone` startet automatisch (oder "Run workflow").
3. Nach ca. 10-15 Minuten unter **Artifacts** `NovaTone-Windows` bzw. `NovaTone-macOS` herunterladen.

### Weg B - lokal bauen
**Windows:** Visual Studio 2022 (Workload "Desktopentwicklung mit C++"), CMake >= 3.22, Git.
```powershell
.\BUILD-WINDOWS.ps1      # baut VST3 + Standalone
.\INSTALL-WINDOWS.ps1    # kopiert das VST3 nach C:\Program Files\Common Files\VST3 (fragt nach Admin)
```
Standalone: `build\windows-release\NovaTone_artefacts\Release\Standalone\NovaTone.exe`

**macOS:** Xcode, `brew install cmake`
```bash
./BUILD-MACOS.command    # baut Universal-Binary und installiert VST3 + Standalone fuer den aktuellen Benutzer
```
Der erste CMake-Lauf laedt JUCE 9.0.5 (Internet noetig, dauert einige Minuten).

## Benutzung

**In Cubase:** Plugin-Verwaltung aktualisieren, dann Spur hinzufuegen > **Instrument** > *NovaTone*
(Kategorie Instrument | Synth). Die Werkspresets stehen im Programm-/Preset-Menue des Plugins; Parameter sind
automatisierbar. Das Plugin meldet seine Latenz (86 Samples = 1,8 ms bei 48 kHz, gemessen) an Cubase, die Verzoegerungskompensation
laeuft automatisch.

**Standalone:** `NovaTone` starten wie jedes Programm - ohne Cubase.
- Oben rechts im Fenster (Zahnrad / *Options*) **Audio/MIDI Settings**: Audiogeraet und **MIDI-Eingang** waehlen.
- Ohne MIDI-Keyboard: die **Bildschirm-Tastatur** unten anklicken (Mausposition auf der Taste = Velocity). Sie reagiert
  auch auf die Computertastatur, sobald sie den Fokus hat.
- ASIO ist nicht eingebaut (Steinberg-Lizenz); verwendet wird WASAPI/DirectSound bzw. CoreAudio. Ein exklusiver ASIO-Treiber
  kann ohnehin nicht gleichzeitig von Cubase und dem Standalone belegt werden.

**Instrumente:** Bass: Pick, Finger, Synth Fast, Synth Soft. Layer: Warm Analog, Digital (FM-Glas), Atmospheric, Wide.
Piano: Concert Grand, Vintage, Muted, Classic. Waveform-Mischung, Unison und Filter-Huellkurve wirken auf die Synth-Instrumente;
Saiten- und Piano-Modell haben eigene Parameter, laufen aber ebenfalls durch Filter, Huellkurve und alle Effekte.

**Signalweg:** Stimmen > Level > Drive (2x OS) > EQ (4 Baender) > Dynamic EQ (8) > Kompressor > Chorus > Delay > Reverb > Breite/Pan > Master > Limiter.

**Eigene Samples (optional):** siehe `Samples/README.txt`. Ordner: `Dokumente/NovaTone/Samples/<Instrument>/C3.wav`.
Ohne Samples klingt das Plugin trotzdem vollstaendig (Modelle).

## Entwicklungsstruktur
```
Source/Engine/     JUCE-freie Engine (Params, Voice, FX, Engine, Presets, Notennamen)   <- getestet
Source/Plugin*     JUCE-Huelle: Parameter, MIDI, State, Programme, Sample-Lader, Oberflaeche
Tests/             Engine-Test (run_tests.sh), Klanganalyse (analyze*.py)
.github/workflows  CI: Windows + macOS Build, Engine-Test
```
Engine-Test lokal: `./Tests/run_tests.sh` (nur g++/clang++ noetig).

## Ehrliche Grenzen
- Das Piano ist ein **physikalisch inspiriertes Modell**, keine gesampelte Flügelaufnahme. Es klingt fuer ein Modell
  ordentlich und reagiert stark auf Velocity, erreicht aber nicht die Detailtiefe grosser Sample-Libraries. Mit eigenen
  Samples (Ordner oben) laesst sich das ersetzen.
- Samples: eine Datei pro Note wird umgepitcht (keine Velocity-Layer, keine Loops, keine Round-Robins).
- Kein AU, kein MPE, keine Mehrfach-Ausgaenge, nur Stereo-Ausgang (v0.1 erlaubte auch Mono). Reverb-Typwechsel kann kurz knacken (nur Wet-Signal).
- Bei extremer Last (alle 64 Stimmen) wird die aelteste Stimme hart uebernommen. Gemessene Last im Stresstest (200 Noten + Pedal, 20 s,
  alle Effekte an): Piano ca. 7x, Synth-Layer 14-20x, Pick-Bass 26x schneller als Echtzeit (ein Kern, 48 kHz, Testrechner).
- Nicht getestet: echte Hosts (Cubase), Oberflaeche, Plugin-Validierung (empfohlen: `pluginval` auf das gebaute VST3).
