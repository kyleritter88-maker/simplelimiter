#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// A label that fires a callback on click — used for the LUFS readouts so
// clicking them resets the running measurement.
struct ClickableLabel : public juce::Label
{
    std::function<void()> onClick;
    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
};

class SimpleLimiterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit SimpleLimiterAudioProcessorEditor (SimpleLimiterAudioProcessor&);
    ~SimpleLimiterAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    SimpleLimiterAudioProcessor& processor;

    juce::Slider gainKnob, attackKnob, releaseKnob, ceilingKnob;
    juce::Label  gainLabel, attackLabel, releaseLabel, ceilingLabel;

    juce::ToggleButton bypassButton;
    juce::ToggleButton ditherButton;
    juce::ComboBox oversamplingBox;
    juce::Label oversamplingLabel;

    ClickableLabel lufsIntegratedValue, lufsShortTermValue;
    juce::Label lufsIntegratedCaption, lufsShortTermCaption;

    float grL = 0.0f, grR = 0.0f;
    float peakL = -100.0f, peakR = -100.0f;
    juce::Rectangle<int> meterAreaBounds;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ditherAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleLimiterAudioProcessorEditor)
};
