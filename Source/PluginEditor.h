#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

//==============================================================================
class NovaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NovaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    static juce::Colour accent() { return juce::Colour (0xff35d3c1); }
};

//==============================================================================
// Ein Bedienelement, das automatisch zum Parametertyp passt (Knob / Auswahl / Schalter)
class ParamControl : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState& apvts, int paramIndex);
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
    int kind = 0;
};

//==============================================================================
// 12 Instrument-Kacheln (Bass | Layer | Piano)
class InstrumentTiles : public juce::Component
{
public:
    explicit InstrumentTiles (juce::AudioProcessorValueTreeState& apvts);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void refresh();
    std::vector<std::unique_ptr<juce::TextButton>> tiles;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int selected = 0;
};

//==============================================================================
// Seite aus Ueberschriften, Bedienelementen und optionalen Breiten-Komponenten
class ControlPage : public juce::Component
{
public:
    void addHeading (const juce::String& text);
    void addParam (juce::AudioProcessorValueTreeState& apvts, const char* paramId);
    void addWide (juce::Component& c, int height);
    int layoutFor (int width, bool apply);
    void resized() override { layoutFor (getWidth(), true); }
    void paint (juce::Graphics&) override;

private:
    struct Item { juce::Component* comp = nullptr; juce::String heading; int wideHeight = 0; };
    std::vector<Item> items;
    std::vector<std::unique_ptr<ParamControl>> controls;
    std::vector<juce::Rectangle<int>> headingRects;
};

//==============================================================================
class NovaToneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NovaToneAudioProcessorEditor (NovaToneAudioProcessor&);
    ~NovaToneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPage (int index);
    void layoutPages();

    NovaToneAudioProcessor& proc;
    NovaLookAndFeel laf;

    juce::Label title;
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "Save" }, loadButton { "Load" }, samplesButton { "Samples" };

    InstrumentTiles tiles;
    ControlPage pages[5];
    juce::Viewport viewports[5];
    juce::TextButton tabButtons[5];
    juce::Label infoLabel, footer;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> meterArea;
    int lastProgram = -1, currentPage = 0, tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NovaToneAudioProcessorEditor)
};
