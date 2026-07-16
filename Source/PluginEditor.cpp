#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour        { 0xff1a1c20 };
    const juce::Colour panelColour     { 0xff24262c };
    const juce::Colour accentColour    { 0xff4fd1c5 };
    const juce::Colour textColour      { 0xffe8e8ea };
    const juce::Colour dimTextColour   { 0xff8a8d94 };
    const juce::Colour meterColour     { 0xffff5c5c };
    const juce::Colour levelTickColour { 0xff7cfc9e };

    // Shared layout constants so paint() and resized() can never disagree.
    constexpr int margin         = 24;
    constexpr int titleHeight    = 40;
    constexpr int knobAreaH      = 140;
    constexpr int knobSize       = 120;
    constexpr int gainLabelH     = 18;
    constexpr int smallKnobAreaH = 92;
    constexpr int smallKnobSize  = 58;
    constexpr int smallLabelH    = 16;
    constexpr int optionsRowH    = 30;
    constexpr int meterAreaH     = 120;
    constexpr int meterLabelGap  = 34; // room for "L"/"R" + peak dB text below meters
    constexpr int lufsRowH       = 56;
    constexpr int sectionGap     = 10;
}

SimpleLimiterAudioProcessorEditor::SimpleLimiterAudioProcessorEditor (SimpleLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (400, 700);

    auto setupBigKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 100, 24);
        s.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
        s.setColour (juce::Slider::thumbColourId, textColour);
        s.setColour (juce::Slider::textBoxTextColourId, textColour);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
    };
    auto setupSmallKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
        s.setColour (juce::Slider::thumbColourId, textColour);
        s.setColour (juce::Slider::textBoxTextColourId, textColour);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);
    };

    setupBigKnob (gainKnob);
    setupSmallKnob (attackKnob);
    setupSmallKnob (releaseKnob);
    setupSmallKnob (ceilingKnob);

    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "gain", gainKnob);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "attack", attackKnob);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "release", releaseKnob);
    ceilingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "ceiling", ceilingKnob);

    auto setupCaptionLabel = [this] (juce::Label& l, const juce::String& text, float fontSize = 12.0f)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, dimTextColour);
        l.setFont (juce::Font (fontSize, juce::Font::bold));
        addAndMakeVisible (l);
    };

    setupCaptionLabel (gainLabel, "GAIN", 14.0f);
    setupCaptionLabel (attackLabel, "ATTACK");
    setupCaptionLabel (releaseLabel, "RELEASE");
    setupCaptionLabel (ceilingLabel, "CEILING");

    bypassButton.setButtonText ("BYPASS");
    bypassButton.setColour (juce::ToggleButton::textColourId, dimTextColour);
    bypassButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "bypass", bypassButton);

    ditherButton.setButtonText ("DITHER");
    ditherButton.setColour (juce::ToggleButton::textColourId, dimTextColour);
    ditherButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    addAndMakeVisible (ditherButton);
    ditherAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "dither", ditherButton);

    oversamplingBox.addItem ("2x", 1);
    oversamplingBox.addItem ("4x", 2);
    oversamplingBox.addItem ("8x", 3);
    oversamplingBox.setColour (juce::ComboBox::textColourId, textColour);
    oversamplingBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
    oversamplingBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (oversamplingBox);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, "osFactor", oversamplingBox);

    auto setupValueLabel = [this] (ClickableLabel& l, std::function<void()> onClickFn)
    {
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, textColour);
        l.setFont (juce::Font (22.0f, juce::Font::bold));
        l.onClick = std::move (onClickFn);
        l.setTooltip ("Click to reset this reading");
        l.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (l);
    };

    setupValueLabel (lufsIntegratedValue, [this] { processor.requestResetIntegratedLufs(); });
    setupCaptionLabel (lufsIntegratedCaption, "LUFS INTEGRATED");

    setupValueLabel (lufsShortTermValue, [this] { processor.requestResetShortTermLufs(); });
    setupCaptionLabel (lufsShortTermCaption, "LUFS SHORT-TERM");

    startTimerHz (20);
}

void SimpleLimiterAudioProcessorEditor::timerCallback()
{
    grL = processor.gainReductionDbL.load();
    grR = processor.gainReductionDbR.load();
    peakL = processor.peakLevelDbL.load();
    peakR = processor.peakLevelDbR.load();

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

    // Combination meter: red GR fill from the top, green tick showing current
    // output peak level (-24dBFS at bottom to 0dBFS at top).
    auto drawMeter = [&] (juce::Rectangle<int> r, float grDb, float levelDb, const juce::String& label)
    {
        g.setColour (panelColour);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        float maxGrRange = 12.0f;
        float grFrac = juce::jlimit (0.0f, 1.0f, grDb / maxGrRange);
        auto grFilled = r.withTop (r.getY() + (int) ((1.0f - grFrac) * (float) r.getHeight()));
        g.setColour (meterColour);
        g.fillRoundedRectangle (grFilled.toFloat(), 4.0f);

        float levelFrac = juce::jlimit (0.0f, 1.0f, (levelDb + 24.0f) / 24.0f);
        int tickY = r.getBottom() - (int) (levelFrac * (float) r.getHeight());
        g.setColour (levelTickColour);
        g.fillRect (r.getX() - 3, tickY - 1, r.getWidth() + 6, 2);

        g.setColour (dimTextColour);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (label, r.getX(), r.getBottom() + 2, r.getWidth(), 14, juce::Justification::centred);

        g.setFont (juce::Font (10.0f));
        juce::String levelText = levelDb <= -99.0f ? juce::String ("-inf")
                                                    : juce::String (levelDb, 1);
        g.drawText (levelText + " dB", r.getX() - 14, r.getBottom() + 16, r.getWidth() + 28, 14,
                    juce::Justification::centred);
    };

    int meterWidth = 30;
    int gap = 50;
    int totalWidth = meterWidth * 2 + gap;
    int startX = meterAreaBounds.getCentreX() - totalWidth / 2;

    juce::Rectangle<int> lMeter (startX, meterAreaBounds.getY(), meterWidth, meterAreaBounds.getHeight());
    juce::Rectangle<int> rMeter (startX + meterWidth + gap, meterAreaBounds.getY(), meterWidth, meterAreaBounds.getHeight());

    drawMeter (lMeter, grL, peakL, "L");
    drawMeter (rMeter, grR, peakR, "R");

    g.setColour (dimTextColour);
    g.setFont (juce::Font (11.0f));
    g.drawText ("GR (red)  /  PEAK (green)", lMeter.getX() - 20, meterAreaBounds.getY() - 18,
                totalWidth + 40, 16, juce::Justification::centred);
}

void SimpleLimiterAudioProcessorEditor::resized()
{
    bypassButton.setBounds (getWidth() - 100, 12, 80, 22);

    auto bounds = getLocalBounds().reduced (margin);

    bounds.removeFromTop (titleHeight);

    auto knobArea = bounds.removeFromTop (knobAreaH);
    gainKnob.setBounds (knobArea.withSizeKeepingCentre (knobSize, knobSize));

    gainLabel.setBounds (bounds.removeFromTop (gainLabelH));
    bounds.removeFromTop (sectionGap);

    // Attack / Release / Ceiling — three small knobs across
    auto smallKnobsRow = bounds.removeFromTop (smallKnobAreaH);
    int colWidth = smallKnobsRow.getWidth() / 3;

    auto attackArea  = smallKnobsRow.removeFromLeft (colWidth);
    auto releaseArea = smallKnobsRow.removeFromLeft (colWidth);
    auto ceilingArea = smallKnobsRow;

    auto layoutSmallKnob = [] (juce::Rectangle<int> area, juce::Slider& knob, juce::Label& label)
    {
        auto knobArea2 = area.removeFromTop (smallKnobAreaH - smallLabelH);
        knob.setBounds (knobArea2.withSizeKeepingCentre (smallKnobSize, smallKnobSize));
        label.setBounds (area);
    };

    layoutSmallKnob (attackArea, attackKnob, attackLabel);
    layoutSmallKnob (releaseArea, releaseKnob, releaseLabel);
    layoutSmallKnob (ceilingArea, ceilingKnob, ceilingLabel);

    bounds.removeFromTop (sectionGap);

    // Oversampling selector + Dither toggle, side by side
    auto optionsRow = bounds.removeFromTop (optionsRowH);
    auto osArea = optionsRow.removeFromLeft (optionsRow.getWidth() / 2).reduced (8, 2);
    auto ditherArea = optionsRow.reduced (8, 2);
    oversamplingBox.setBounds (osArea);
    ditherButton.setBounds (ditherArea);

    bounds.removeFromTop (sectionGap);

    meterAreaBounds = bounds.removeFromTop (meterAreaH);
    bounds.removeFromTop (meterLabelGap);
    bounds.removeFromTop (sectionGap);

    auto lufsRow = bounds.removeFromTop (lufsRowH);
    auto leftHalf  = lufsRow.removeFromLeft (lufsRow.getWidth() / 2);
    auto rightHalf = lufsRow;

    lufsIntegratedValue.setBounds (leftHalf.removeFromTop (32));
    lufsIntegratedCaption.setBounds (leftHalf);

    lufsShortTermValue.setBounds (rightHalf.removeFromTop (32));
    lufsShortTermCaption.setBounds (rightHalf);
}
