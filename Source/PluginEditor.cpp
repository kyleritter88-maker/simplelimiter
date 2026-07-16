#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour       { 0xff1a1c20 };
    const juce::Colour panelColour    { 0xff24262c };
    const juce::Colour accentColour   { 0xff4fd1c5 };
    const juce::Colour textColour     { 0xffe8e8ea };
    const juce::Colour dimTextColour  { 0xff8a8d94 };
    const juce::Colour meterColour    { 0xffff5c5c };
}

SimpleLimiterAudioProcessorEditor::SimpleLimiterAudioProcessorEditor (SimpleLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (360, 460);

    gainKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 24);
    gainKnob.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    gainKnob.setColour (juce::Slider::thumbColourId, textColour);
    gainKnob.setColour (juce::Slider::textBoxTextColourId, textColour);
    gainKnob.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (gainKnob);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "gain", gainKnob);

    gainLabel.setText ("GAIN", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, dimTextColour);
    gainLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    addAndMakeVisible (gainLabel);

    auto setupValueLabel = [this] (juce::Label& l)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, textColour);
        l.setFont (juce::Font (22.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };
    auto setupCaptionLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, dimTextColour);
        l.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupValueLabel (lufsIntegratedValue);
    setupCaptionLabel (lufsIntegratedCaption, "LUFS INTEGRATED");

    setupValueLabel (lufsShortTermValue);
    setupCaptionLabel (lufsShortTermCaption, "LUFS SHORT-TERM");

    ceilingCaption.setText ("CEILING -1.0 dBTP", juce::dontSendNotification);
    ceilingCaption.setJustificationType (juce::Justification::centred);
    ceilingCaption.setColour (juce::Label::textColourId, dimTextColour);
    ceilingCaption.setFont (juce::Font (12.0f, juce::Font::plain));
    addAndMakeVisible (ceilingCaption);

    startTimerHz (20);
}

void SimpleLimiterAudioProcessorEditor::timerCallback()
{
    grL = processor.gainReductionDbL.load();
    grR = processor.gainReductionDbR.load();

    float lufsI = processor.lufsIntegrated.load();
    float lufsS = processor.lufsShortTerm.load();

    lufsIntegratedValue.setText (lufsI <= -69.0f ? juce::String ("--")
                                                  : juce::String (lufsI, 1),
                                 juce::dontSendNotification);
    lufsShortTermValue.setText (lufsS <= -69.0f ? juce::String ("--")
                                                 : juce::String (lufsS, 1),
                                juce::dontSendNotification);

    repaint();
}

void SimpleLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);

    g.setColour (textColour);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("SIMPLE LIMITER", getLocalBounds().removeFromTop (40), juce::Justification::centred);

    // Gain reduction meters (L/R), drawn as two vertical bars under the knob area
    auto bounds = getLocalBounds().reduced (24);
    auto meterArea = bounds.removeFromBottom (140).reduced (0, 8);
    meterArea.removeFromBottom (60); // leave room for LUFS labels below, laid out in resized()

    auto drawMeter = [&] (juce::Rectangle<int> r, float grDb, const juce::String& label)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        float maxGrRange = 12.0f; // dB of GR shown full-scale
        float frac = juce::jlimit (0.0f, 1.0f, grDb / maxGrRange);
        auto filled = r.withTop (r.getY() + (int) ((1.0f - frac) * (float) r.getHeight()));
        g.setColour (meterColour);
        g.fillRoundedRectangle (filled.toFloat(), 4.0f);

        g.setColour (dimTextColour);
        g.setFont (juce::Font (11.0f));
        g.drawText (label, r.withY (r.getBottom() + 2).withHeight (16), juce::Justification::centred);
    };

    int meterWidth = 28;
    int gap = 40;
    int totalWidth = meterWidth * 2 + gap;
    int startX = bounds.getCentreX() - totalWidth / 2;

    juce::Rectangle<int> lMeter (startX, meterArea.getY(), meterWidth, meterArea.getHeight());
    juce::Rectangle<int> rMeter (startX + meterWidth + gap, meterArea.getY(), meterWidth, meterArea.getHeight());

    drawMeter (lMeter, grL, "L");
    drawMeter (rMeter, grR, "R");

    g.setColour (dimTextColour);
    g.setFont (juce::Font (11.0f));
    g.drawText ("GAIN REDUCTION (dB)", lMeter.getX() - 10, meterArea.getY() - 18,
                totalWidth + 20, 16, juce::Justification::centred);
}

void SimpleLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (24);

    bounds.removeFromTop (30); // title space

    auto knobArea = bounds.removeFromTop (190);
    gainKnob.setBounds (knobArea.withSizeKeepingCentre (160, 160));

    gainLabel.setBounds (bounds.removeFromTop (20));

    bounds.removeFromTop (170); // reserved for GR meters, drawn in paint()

    auto lufsRow = bounds.removeFromTop (70);
    auto leftHalf  = lufsRow.removeFromLeft (lufsRow.getWidth() / 2);
    auto rightHalf = lufsRow;

    lufsIntegratedValue.setBounds (leftHalf.removeFromTop (34));
    lufsIntegratedCaption.setBounds (leftHalf);

    lufsShortTermValue.setBounds (rightHalf.removeFromTop (34));
    lufsShortTermCaption.setBounds (rightHalf);

    ceilingCaption.setBounds (bounds.removeFromTop (20));
}
