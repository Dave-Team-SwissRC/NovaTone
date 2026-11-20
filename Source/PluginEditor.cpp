#include "PluginEditor.h"

namespace
{
const juce::Colour kBg      { 0xff10151c };
const juce::Colour kPanel   { 0xff182029 };
const juce::Colour kText    { 0xffe6edf3 };
const juce::Colour kDim     { 0xff8b98a8 };

int indexOfParam (const char* id)
{
    const nova::ParamInfo* t = nova::paramTable();
    for (int i = 0; i < nova::pCount; ++i)
        if (juce::String (t[i].id) == id) return i;
    jassertfalse;
    return 0;
}

juce::String shortName (const nova::ParamInfo& p)
{
    juce::String n (p.name);
    if (n.startsWith ("Dynamic Band"))               // "Dynamic Band 3 (250 Hz)" -> "250 Hz"
        return n.fromFirstOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (")", false, false);
    return n.replace ("Comp ", "").replace ("EQ ", "").replace ("Reverb ", "").replace ("Delay ", "").replace ("Chorus ", "");
}
} // namespace

//==============================================================================
NovaLookAndFeel::NovaLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);
    setColour (juce::ComboBox::backgroundColourId, kPanel);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a3340));
    setColour (juce::ComboBox::arrowColourId, accent());
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent().withAlpha (0.35f));
    setColour (juce::Label::textColourId, kText);
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, kPanel);
    setColour (juce::TextButton::buttonOnColourId, accent().withAlpha (0.8f));
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff04201d));
    setColour (juce::ToggleButton::textColourId, kText);
    setColour (juce::ToggleButton::tickColourId, accent());
}

void NovaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, a1, true);
    g.setColour (juce::Colour (0xff2a3340));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - 2.0f, r - 2.0f, 0.0f, a0, ang, true);
    g.setColour (accent());
    g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff1d2630));
    g.fillEllipse (c.x - (r - 9.0f), c.y - (r - 9.0f), (r - 9.0f) * 2.0f, (r - 9.0f) * 2.0f);
    juce::Path p;
    p.addRoundedRectangle (-1.5f, -(r - 10.0f), 3.0f, (r - 10.0f) * 0.55f, 1.0f);
    g.setColour (kText);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

//==============================================================================
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& apvts, int idx)
{
    const nova::ParamInfo& p = nova::paramTable()[idx];
    kind = p.kind;

    label.setText (shortName (p), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.0f));
    label.setColour (juce::Label::textColourId, kDim);
    addAndMakeVisible (label);

    if (kind == nova::kChoice)
    {
        combo.addItemList (juce::StringArray::fromTokens (p.choices, "|", ""), 1);
        addAndMakeVisible (combo);
        cAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, p.id, combo);
    }
    else if (kind == nova::kBool)
    {
        toggle.setButtonText ("On");
        addAndMakeVisible (toggle);
        bAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (apvts, p.id, toggle);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setDoubleClickReturnValue (true, p.defaultValue);
        addAndMakeVisible (slider);
        sAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, p.id, slider);
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (18));
    if (kind == nova::kChoice)      combo.setBounds (r.withSizeKeepingCentre (juce::jmin (r.getWidth() - 6, 130), 26));
    else if (kind == nova::kBool)   toggle.setBounds (r.withSizeKeepingCentre (70, 26));
    else                            slider.setBounds (r);
}

//==============================================================================
InstrumentTiles::InstrumentTiles (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < nova::kNumInstruments; ++i)
    {
        auto b = std::make_unique<juce::TextButton> (nova::instrumentNames()[i]);
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (*b);
        tiles.push_back (std::move (b));
    }
    auto* prm = apvts.getParameter ("instrument");
    attachment = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { selected = (int) (v + 0.5f); refresh(); });
    attachment->sendInitialUpdate();
}

void InstrumentTiles::refresh()
{
    for (int i = 0; i < (int) tiles.size(); ++i) tiles[(size_t) i]->setToggleState (i == selected, juce::dontSendNotification);
    repaint();
}

void InstrumentTiles::resized()
{
    auto r = getLocalBounds().withTrimmedTop (22);
    const int colW = r.getWidth() / 3;
    for (int g = 0; g < 3; ++g)
    {
        auto col = r.removeFromLeft (colW).reduced (4, 0);
        const int rowH = col.getHeight() / 2;
        for (int k = 0; k < 4; ++k)
        {
            const int row = k / 2, cx = k % 2;
            auto cell = juce::Rectangle<int> (col.getX() + cx * (col.getWidth() / 2), col.getY() + row * rowH, col.getWidth() / 2, rowH).reduced (3);
            tiles[(size_t) (g * 4 + k)]->setBounds (cell);
        }
    }
}

void InstrumentTiles::paint (juce::Graphics& g)
{
    static const char* names[3] = { "BASS", "SYNTH LAYERS", "PIANO" };
    const int colW = getWidth() / 3;
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    g.setColour (NovaLookAndFeel::accent());
    for (int i = 0; i < 3; ++i) g.drawText (names[i], i * colW + 8, 0, colW - 16, 20, juce::Justification::centredLeft);
}

//==============================================================================
void ControlPage::addHeading (const juce::String& text) { Item it; it.heading = text; items.push_back (it); }

void ControlPage::addParam (juce::AudioProcessorValueTreeState& apvts, const char* paramId)
{
    auto c = std::make_unique<ParamControl> (apvts, indexOfParam (paramId));
    addAndMakeVisible (*c);
    Item it; it.comp = c.get(); items.push_back (it);
    controls.push_back (std::move (c));
}

void ControlPage::addWide (juce::Component& c, int height)
{
    addAndMakeVisible (c);
    Item it; it.comp = &c; it.wideHeight = height; items.push_back (it);
}

int ControlPage::layoutFor (int width, bool apply)
{
    constexpr int pad = 12, cellW = 112, cellH = 108;
    int x = pad, y = pad;
    if (apply) headingRects.clear();
    for (auto& it : items)
    {
        if (it.heading.isNotEmpty())
        {
            if (x > pad) { x = pad; y += cellH; }
            if (apply) headingRects.push_back ({ pad, y, width - 2 * pad, 20 });
            y += 24;
        }
        else if (it.wideHeight > 0)
        {
            if (x > pad) { x = pad; y += cellH; }
            if (apply) it.comp->setBounds (pad, y, width - 2 * pad, it.wideHeight);
            y += it.wideHeight + 8;
        }
        else
        {
            if (x + cellW > width - pad && x > pad) { x = pad; y += cellH; }
            if (apply) it.comp->setBounds (x, y, cellW, cellH);
            x += cellW;
        }
    }
    if (x > pad) y += cellH;
    return y + pad;
}

void ControlPage::paint (juce::Graphics& g)
{
    int h = 0;
    g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
    for (auto& it : items)
        if (it.heading.isNotEmpty() && h < (int) headingRects.size())
        {
            const auto r = headingRects[(size_t) h++];
            g.setColour (NovaLookAndFeel::accent());
            g.drawText (it.heading.toUpperCase(), r, juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff2a3340));
            g.drawHorizontalLine (r.getBottom() - 1, (float) r.getX() + 130.0f, (float) r.getRight());
        }
}

//==============================================================================
NovaToneAudioProcessorEditor::NovaToneAudioProcessorEditor (NovaToneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), tiles (p.apvts),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&laf);
    auto& a = proc.apvts;

    title.setText ("NOVATONE", juce::dontSendNotification);
    title.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    title.setColour (juce::Label::textColourId, NovaLookAndFeel::accent());
    addAndMakeVisible (title);

    for (int i = 0; i < nova::presetCount(); ++i) presetBox.addItem (nova::presetName (i), i + 1);
    presetBox.onChange = [this] { const int i = presetBox.getSelectedItemIndex(); if (i >= 0) proc.setCurrentProgram (i); };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { const int n = nova::presetCount(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); };
    nextButton.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % nova::presetCount()); };
    saveButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Preset speichern", proc.getPresetFolder(), "*.novapreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.savePresetToFile (f.withFileExtension ("novapreset"));
                              });
    };
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Preset laden", proc.getPresetFolder(), "*.novapreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File()) proc.loadPresetFromFile (f);
                              });
    };
    samplesButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Reload samples");
        m.addItem (2, "Open sample folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&samplesButton),
                         [this] (int r)
                         {
                             if (r == 1) proc.reloadSamples();
                             if (r == 2) proc.getSampleFolder().revealToUser();
                         });
    };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &loadButton, &samplesButton }) addAndMakeVisible (*b);

    // ---- Seite 0: Instrument
    pages[0].addWide (tiles, 120);
    pages[0].addHeading ("Level & Envelope");
    for (auto* id : { "instrumentGain", "pan", "attack", "decay", "sustain", "release", "velSens", "tune" }) pages[0].addParam (a, id);
    pages[0].addHeading ("Filter");
    for (auto* id : { "filterType", "cutoff", "resonance", "keyTrack", "filterEnv", "filterDecay" }) pages[0].addParam (a, id);

    // ---- Seite 1: Sound
    pages[1].addHeading ("Oscillators (Synth instruments)");
    for (auto* id : { "waveSine", "waveTriangle", "waveSaw", "waveSquare", "unisonVoices", "unisonDetune" }) pages[1].addParam (a, id);
    pages[1].addHeading ("Playing");
    for (auto* id : { "playMode", "glide", "bendRange" }) pages[1].addParam (a, id);
    pages[1].addHeading ("Modulation");
    for (auto* id : { "lfoTarget", "lfoRate", "lfoDepth" }) pages[1].addParam (a, id);

    // ---- Seite 2: Effekte
    pages[2].addHeading ("Drive");
    for (auto* id : { "distortionType", "drive", "distortionMix" }) pages[2].addParam (a, id);
    pages[2].addHeading ("Octave layer");
    for (auto* id : { "octave", "octaveMix" }) pages[2].addParam (a, id);
    pages[2].addHeading ("Chorus");
    for (auto* id : { "chorusRate", "chorusDepth", "chorusMix" }) pages[2].addParam (a, id);
    pages[2].addHeading ("Delay");
    for (auto* id : { "delayTime", "delayFeedback", "delayMix" }) pages[2].addParam (a, id);
    pages[2].addHeading ("Reverb");
    for (auto* id : { "reverbType", "reverbMix", "reverbSize", "reverbDamp" }) pages[2].addParam (a, id);

    // ---- Seite 3: EQ & Dynamik
    pages[3].addHeading ("EQ");
    for (auto* id : { "eqMode", "eqLow", "eqLowMid", "eqHighMid", "eqHigh" }) pages[3].addParam (a, id);
    pages[3].addHeading ("Dynamic EQ (cut = acts when loud, boost = backs off when loud)");
    for (auto* id : { "dynBand1", "dynBand2", "dynBand3", "dynBand4", "dynBand5", "dynBand6", "dynBand7", "dynBand8", "dynamicAmount" }) pages[3].addParam (a, id);
    pages[3].addHeading ("Compressor");
    for (auto* id : { "compMode", "compThreshold", "compRatio", "compAttack", "compRelease", "compMix" }) pages[3].addParam (a, id);

    // ---- Seite 4: Ausgang
    pages[4].addHeading ("Output");
    for (auto* id : { "masterGain", "stereoWidth", "panDepth", "panRate", "limiter" }) pages[4].addParam (a, id);
    infoLabel.setJustificationType (juce::Justification::topLeft);
    infoLabel.setColour (juce::Label::textColourId, kDim);
    pages[4].addHeading ("Info");
    pages[4].addWide (infoLabel, 120);

    static const char* tabNames[5] = { "INSTRUMENT", "SOUND", "EFFECTS", "EQ / DYNAMICS", "OUTPUT" };
    for (int i = 0; i < 5; ++i)
    {
        viewports[i].setViewedComponent (&pages[i], false);
        viewports[i].setScrollBarsShown (true, false);
        addChildComponent (viewports[i]);
        tabButtons[i].setButtonText (tabNames[i]);
        tabButtons[i].setClickingTogglesState (true);
        tabButtons[i].setRadioGroupId (1001);
        tabButtons[i].onClick = [this, i] { showPage (i); };
        addAndMakeVisible (tabButtons[i]);
    }

    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setVelocity (0.8f, true);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, NovaLookAndFeel::accent().withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, NovaLookAndFeel::accent().withAlpha (0.3f));
    addAndMakeVisible (keyboard);

    footer.setColour (juce::Label::textColourId, kDim);
    footer.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (footer);

    setResizable (true, true);
    setResizeLimits (860, 620, 1800, 1200);
    setSize (1180, 820);

    tabButtons[0].setToggleState (true, juce::dontSendNotification);
    showPage (0);
    startTimerHz (30);
}

NovaToneAudioProcessorEditor::~NovaToneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NovaToneAudioProcessorEditor::showPage (int index)
{
    currentPage = index;
    for (int i = 0; i < 5; ++i) viewports[i].setVisible (i == index);
    layoutPages();
}

void NovaToneAudioProcessorEditor::layoutPages()
{
    for (int i = 0; i < 5; ++i)
    {
        const int w = viewports[i].getWidth() - viewports[i].getScrollBarThickness();
        const int h = pages[i].layoutFor (w, false);
        pages[i].setBounds (0, 0, w, juce::jmax (h, viewports[i].getHeight()));
    }
}

void NovaToneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kPanel);
    g.fillRect (0, 0, getWidth(), 60);

    // Pegelanzeige
    g.setColour (juce::Colour (0xff0b0f14));
    g.fillRoundedRectangle (meterArea.toFloat(), 3.0f);
    const float ml = juce::jlimit (0.0f, 1.0f, nova::gainToDb (proc.meterL.load()) / 60.0f + 1.0f);
    const float mr = juce::jlimit (0.0f, 1.0f, nova::gainToDb (proc.meterR.load()) / 60.0f + 1.0f);
    auto bar = meterArea.reduced (2);
    const int half = bar.getHeight() / 2;
    auto top = bar.removeFromTop (half - 1);
    bar.removeFromTop (2);
    g.setColour (ml > 0.98f ? juce::Colours::red : NovaLookAndFeel::accent());
    g.fillRect (top.withWidth ((int) ((float) top.getWidth() * ml)));
    g.setColour (mr > 0.98f ? juce::Colours::red : NovaLookAndFeel::accent());
    g.fillRect (bar.withWidth ((int) ((float) bar.getWidth() * mr)));
}

void NovaToneAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (60).reduced (12, 10);
    title.setBounds (head.removeFromLeft (150));
    meterArea = head.removeFromRight (110).reduced (0, 8);
    head.removeFromRight (10);
    samplesButton.setBounds (head.removeFromRight (80)); head.removeFromRight (6);
    loadButton.setBounds (head.removeFromRight (60)); head.removeFromRight (6);
    saveButton.setBounds (head.removeFromRight (60)); head.removeFromRight (12);
    nextButton.setBounds (head.removeFromRight (34)); head.removeFromRight (4);
    presetBox.setBounds (head.removeFromRight (juce::jmin (320, head.getWidth() - 40))); head.removeFromRight (4);
    prevButton.setBounds (head.removeFromRight (34));

    footer.setBounds (r.removeFromBottom (22).reduced (12, 2));
    keyboard.setBounds (r.removeFromBottom (86).reduced (8, 4));

    auto tabs = r.removeFromTop (36).reduced (12, 4);
    const int tw = juce::jmin (150, tabs.getWidth() / 5);
    for (int i = 0; i < 5; ++i) tabButtons[i].setBounds (tabs.removeFromLeft (tw).reduced (2, 0));

    for (int i = 0; i < 5; ++i) viewports[i].setBounds (r);
    layoutPages();
}

void NovaToneAudioProcessorEditor::timerCallback()
{
    repaint (meterArea);
    const int prog = proc.getCurrentProgram();
    if (prog != lastProgram) { lastProgram = prog; presetBox.setSelectedItemIndex (prog, juce::dontSendNotification); }

    if ((++tick % 15) == 0)
    {
        const juce::String s = proc.isLoadingSamples() ? "loading samples..." : (juce::String (proc.getLoadedSampleCount()) + " sample file(s) loaded");
        footer.setText ("NovaTone 0.2  |  " + s + "  |  latency " + juce::String (proc.getLatencySamples()) + " samples  |  "
                        + (juce::JUCEApplicationBase::isStandaloneApp() ? "Standalone: gear/Options button = audio + MIDI settings" : "Plugin mode"),
                        juce::dontSendNotification);
        infoLabel.setText ("Sample folder: " + proc.getSampleFolder().getFullPathName()
                           + "\nSubfolders per instrument, file names like C3.wav, F#2.wav, Bb1.wav (C3 = MIDI 60, Cubase convention).\n"
                           + "Without samples the built-in sound models are used (string model for bass, modal model for piano).",
                           juce::dontSendNotification);
    }
}
