#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

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

    juce::Slider gainKnob;
    juce::Label  gainLabel;

    juce::Label lufsIntegratedValue, lufsIntegratedCaption;
    juce::Label lufsShortTermValue,  lufsShortTermCaption;
    juce::Label ceilingCaption;

    float grL = 0.0f, grR = 0.0f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleLimiterAudioProcessorEditor)
};
