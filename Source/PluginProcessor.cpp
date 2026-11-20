#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/NovaNotes.h"

//==============================================================================
// Sample-Lader: laeuft in einem Hintergrund-Thread, blockiert weder Audio noch Cubase-Start.
class NovaToneAudioProcessor::SampleLoader : public juce::Thread
{
public:
    explicit SampleLoader (NovaToneAudioProcessor& o) : juce::Thread ("NovaTone Sample Loader"), owner (o) {}

    void run() override
    {
        owner.loading = true;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        auto bank = std::make_shared<nova::SampleBank>();
        int count = 0;
        const juce::File root = owner.getSampleFolder();

        for (int i = 0; i < nova::kNumInstruments; ++i)
        {
            const juce::File dir = root.getChildFile (nova::sampleFolderNames()[i]);
            if (! dir.isDirectory()) continue;

            juce::Array<juce::File> files;
            for (auto* ext : { "*.wav", "*.aif", "*.aiff", "*.flac" })
                dir.findChildFiles (files, juce::File::findFiles, false, ext);

            for (const auto& f : files)
            {
                if (threadShouldExit()) { owner.loading = false; return; }

                const int note = nova::parseNoteName (f.getFileNameWithoutExtension().toStdString());
                if (note < 0) continue;

                std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
                if (reader == nullptr || reader->lengthInSamples <= 0) continue;

                const int len = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
                const int chans = (int) juce::jmin<unsigned int> (reader->numChannels, 2u);
                juce::AudioBuffer<float> buf (chans, len);
                if (! reader->read (&buf, 0, len, 0, true, chans > 1)) continue;

                nova::SampleData sd;
                sd.rate = reader->sampleRate;
                sd.root = note;
                sd.l.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
                if (chans > 1) sd.r.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
                bank->inst[(size_t) i].push_back (std::move (sd));
                ++count;
            }
        }
        owner.publishBank (std::move (bank), count);
        owner.loading = false;
    }

private:
    NovaToneAudioProcessor& owner;
};

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout NovaToneAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const nova::ParamInfo* t = nova::paramTable();

    for (int i = 0; i < nova::pCount; ++i)
    {
        const nova::ParamInfo& p = t[i];
        const ParameterID pid { p.id, 1 };
        switch (p.kind)
        {
            case nova::kChoice:
                layout.add (std::make_unique<AudioParameterChoice> (pid, p.name, StringArray::fromTokens (p.choices, "|", ""), (int) p.defaultValue));
                break;
            case nova::kBool:
                layout.add (std::make_unique<AudioParameterBool> (pid, p.name, p.defaultValue > 0.5f));
                break;
            default:
                layout.add (std::make_unique<AudioParameterFloat> (pid, p.name,
                                NormalisableRange<float> (p.minValue, p.maxValue, p.step, p.skew),
                                p.defaultValue, AudioParameterFloatAttributes().withLabel (p.unit)));
                break;
        }
    }
    return layout;
}

NovaToneAudioProcessor::NovaToneAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NOVATONE", createLayout())
{
    for (int i = 0; i < nova::pCount; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (nova::paramTable()[i].id);
}

NovaToneAudioProcessor::~NovaToneAudioProcessor()
{
    if (loader != nullptr) loader->stopThread (10000);
}

//==============================================================================
void NovaToneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
    if (! samplesRequested) reloadSamples();
}

void NovaToneAudioProcessor::releaseResources() {}

bool NovaToneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NovaToneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || n <= 0) { midi.clear(); return; }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    float p[nova::pCount];
    for (int i = 0; i < nova::pCount; ++i) p[i] = raw[(size_t) i]->load();
    engine.setParams (p);
    if (auto* b = pendingBank.exchange (nullptr)) engine.setSampleBank (b);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;

    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        const int t = juce::jlimit (0, n, metadata.samplePosition);
        if (t > pos) { engine.render (L + pos, R + pos, t - pos); pos = t; }

        if (m.isNoteOn())                          engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())                    engine.noteOff (m.getNoteNumber());
        else if (m.isPitchWheel())                 engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isController())                 engine.controller (m.getControllerNumber(), m.getControllerValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < n) engine.render (L + pos, R + pos, n - pos);
    midi.clear(); // Instrument: kein MIDI-Durchreichen

    const float pl = buffer.getMagnitude (0, 0, n), pr = buffer.getMagnitude (1, 0, n);
    meterL.store (juce::jmax (pl, meterL.load() * 0.92f));
    meterR.store (juce::jmax (pr, meterR.load() * 0.92f));
}

//==============================================================================
int NovaToneAudioProcessor::getNumPrograms() { return nova::presetCount(); }
const juce::String NovaToneAudioProcessor::getProgramName (int index) { return nova::presetName (index); }

void NovaToneAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, nova::presetCount() - 1, index);
    currentProgram = index;
    applyPreset (index);
}

void NovaToneAudioProcessor::applyPreset (int index)
{
    float v[nova::pCount];
    nova::presetValues (index, v);
    for (int i = 0; i < nova::pCount; ++i)
        if (auto* prm = apvts.getParameter (nova::paramTable()[i].id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v[i]));
}

//==============================================================================
void NovaToneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("novaVersion", "0.2", nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void NovaToneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentProgram = juce::jlimit (0, nova::presetCount() - 1, (int) apvts.state.getProperty ("program", 0));
        }
}

juce::File NovaToneAudioProcessor::getPresetFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("NovaTone").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool NovaToneAudioProcessor::savePresetToFile (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) return xml->writeTo (file);
    return false;
}

bool NovaToneAudioProcessor::loadPresetFromFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            return true;
        }
    return false;
}

//==============================================================================
juce::File NovaToneAudioProcessor::getSampleFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("NovaTone").getChildFile ("Samples");
    dir.createDirectory();
    return dir;
}

void NovaToneAudioProcessor::reloadSamples()
{
    samplesRequested = true;
    if (loader != nullptr) loader->stopThread (10000);
    loader = std::make_unique<SampleLoader> (*this);
    loader->startThread();
}

void NovaToneAudioProcessor::publishBank (std::shared_ptr<nova::SampleBank> bank, int count)
{
    {
        const juce::ScopedLock sl (bankLock);
        banks.push_back (bank);
        while (banks.size() > 3) banks.erase (banks.begin());   // aeltere Baenke bleiben bewusst noch kurz gueltig
    }
    loadedSamples = count;
    pendingBank.store (bank.get());
}

juce::AudioProcessorEditor* NovaToneAudioProcessor::createEditor() { return new NovaToneAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NovaToneAudioProcessor(); }
