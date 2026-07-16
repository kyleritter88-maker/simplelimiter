#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Limiter.h"
#include "LufsMeter.h"

class SimpleLimiterAudioProcessor : public juce::AudioProcessor
{
public:
    SimpleLimiterAudioProcessor();
    ~SimpleLimiterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Simple Limiter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Live meter readouts, read by the editor on a timer.
    std::atomic<float> gainReductionDbL { 0.0f };
    std::atomic<float> gainReductionDbR { 0.0f };
    std::atomic<float> lufsIntegrated { -70.0f };
    std::atomic<float> lufsShortTerm  { -70.0f };

    static constexpr float ceilingDbTP = -1.0f;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ChannelLimiter limiterL, limiterR;
    LufsMeter lufsMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleLimiterAudioProcessor)
};
