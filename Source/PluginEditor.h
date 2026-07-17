#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
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
    void mouseDown (const juce::MouseEvent&) override;

    SimpleLimiterAudioProcessor& processor;

    juce::Slider gainKnob, attackKnob, releaseKnob, ceilingKnob;
    juce::Label  gainLabel, attackLabel, releaseLabel, ceilingLabel;

    juce::ToggleButton bypassButton;
    juce::ToggleButton ditherButton;
    juce::ComboBox oversamplingBox;
    juce::Label oversamplingLabel;

    ClickableLabel lufsIntegratedValue, lufsShortTermValue;
    juce::Label lufsIntegratedCaption, lufsShortTermCaption;

    // Displayed (post hold/decay) meter values, and click-to-freeze state.
    float grL = 0.0f, grR = 0.0f;
    float peakL = -100.0f, peakR = -100.0f;
    int holdCounterL = 0, holdCounterR = 0;
    bool metersFrozen = false;
    juce::Rectangle<int> meterAreaBounds;
    juce::Rectangle<int> historyAreaBounds;

    // Local snapshot of the processor's history ring buffer, refreshed each
    // timer tick and stored oldest-to-newest for easy left-to-right drawing.
    std::array<float, SimpleLimiterAudioProcessor::historySize> peakHistoryLocal;
    std::array<float, SimpleLimiterAudioProcessor::historySize> grHistoryLocal;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ditherAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleLimiterAudioProcessorEditor)
};
