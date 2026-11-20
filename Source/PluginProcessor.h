#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>
#include "Engine/NovaEngine.h"

class NovaToneAudioProcessor : public juce::AudioProcessor
{
public:
    NovaToneAudioProcessor();
    ~NovaToneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    // Programme = Werkspresets (in Cubase im Preset-/Programmmenue sichtbar)
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets als Datei
    bool savePresetToFile (const juce::File& file);
    bool loadPresetFromFile (const juce::File& file);
    juce::File getPresetFolder() const;

    // Samples (optional, eigene Aufnahmen)
    juce::File getSampleFolder() const;
    void reloadSamples();
    int getLoadedSampleCount() const { return loadedSamples.load(); }
    bool isLoadingSamples() const { return loading.load(); }

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;     // Bildschirm-Tastatur (Standalone + Plugin)
    std::atomic<float> meterL { 0.0f }, meterR { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyPreset (int index);
    void publishBank (std::shared_ptr<nova::SampleBank> bank, int count);

    class SampleLoader;
    std::unique_ptr<SampleLoader> loader;
    friend class SampleLoader;

    nova::Engine engine;
    std::array<std::atomic<float>*, nova::pCount> raw {};
    int currentProgram = 0;
    bool samplesRequested = false;

    juce::CriticalSection bankLock;
    std::vector<std::shared_ptr<nova::SampleBank>> banks; // haelt die letzten Baenke am Leben
    std::atomic<const nova::SampleBank*> pendingBank { nullptr };
    std::atomic<int> loadedSamples { 0 };
    std::atomic<bool> loading { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaToneAudioProcessor)
};
