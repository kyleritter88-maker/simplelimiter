#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour       { 0xff1a1c20 };
    const juce::Colour panelColour    { 0xff24262c };
    const juce::Colour accentColour   { 0xff4fd1c5 };
    const juce::Colour textColour     { 0xffe8e8ea };
    const juce::Colour dimTextColour  { 0xff8a8d94 };
    const juce::Colour meterColour    { 0xffff5c5c };

    // Shared layout constants so paint() and resized() can never disagree.
    constexpr int margin        = 24;
    constexpr int titleHeight   = 40;
    constexpr int knobAreaH     = 180;
    constexpr int knobSize      = 150;
    constexpr int gainLabelH    = 22;
    constexpr int meterAreaH    = 140;
    constexpr int meterLabelGap = 20;   // space below meters for L/R text
    constexpr int lufsRowH      = 60;
    constexpr int ceilingH      = 24;
    constexpr int sectionGap    = 10;
}

SimpleLimiterAudioProcessorEditor::SimpleLimiterAudioProcessorEditor (SimpleLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (360, 580);

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
    g.drawText ("SIMPLE LIMITER", getLocalBounds().removeFromTop (titleHeight), juce::Justification::centred);

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
        g.drawText (label, r.getX(), r.getBottom() + 2, r.getWidth(), 16, juce::Justification::centred);
    };

    int meterWidth = 28;
    int gap = 40;
    int totalWidth = meterWidth * 2 + gap;
    int startX = meterAreaBounds.getCentreX() - totalWidth / 2;

    juce::Rectangle<int> lMeter (startX, meterAreaBounds.getY(), meterWidth, meterAreaBounds.getHeight());
    juce::Rectangle<int> rMeter (startX + meterWidth + gap, meterAreaBounds.getY(), meterWidth, meterAreaBounds.getHeight());

    drawMeter (lMeter, grL, "L");
    drawMeter (rMeter, grR, "R");

    g.setColour (dimTextColour);
    g.setFont (juce::Font (11.0f));
    g.drawText ("GAIN REDUCTION (dB)", lMeter.getX() - 10, meterAreaBounds.getY() - 18,
                totalWidth + 20, 16, juce::Justification::centred);
}

void SimpleLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    bounds.removeFromTop (titleHeight);

    auto knobArea = bounds.removeFromTop (knobAreaH);
    gainKnob.setBounds (knobArea.withSizeKeepingCentre (knobSize, knobSize));

    gainLabel.setBounds (bounds.removeFromTop (gainLabelH));
    bounds.removeFromTop (sectionGap);

    meterAreaBounds = bounds.removeFromTop (meterAreaH);
    bounds.removeFromTop (meterLabelGap);
    bounds.removeFromTop (sectionGap);

    auto lufsRow = bounds.removeFromTop (lufsRowH);
    auto leftHalf  = lufsRow.removeFromLeft (lufsRow.getWidth() / 2);
    auto rightHalf = lufsRow;

    lufsIntegratedValue.setBounds (leftHalf.removeFromTop (34));
    lufsIntegratedCaption.setBounds (leftHalf);

    lufsShortTermValue.setBounds (rightHalf.removeFromTop (34));
    lufsShortTermCaption.setBounds (rightHalf);

    bounds.removeFromTop (sectionGap);
    ceilingCaption.setBounds (bounds.removeFromTop (ceilingH));
}
