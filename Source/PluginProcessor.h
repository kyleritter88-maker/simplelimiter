#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
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
    std::atomic<float> peakLevelDbL { -100.0f };
    std::atomic<float> peakLevelDbR { -100.0f };
    std::atomic<float> lufsIntegrated { -70.0f };
    std::atomic<float> lufsShortTerm  { -70.0f };

    // Called from the editor (e.g. on clicking the LUFS numbers) to start a
    // fresh reading. Safe to call from the message thread; actually applied
    // on the audio thread at the top of the next processBlock().
    void requestResetIntegratedLufs() { resetIntegratedFlag.store (true); }
    void requestResetShortTermLufs()  { resetShortTermFlag.store (true); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ChannelLimiter limiterL, limiterR;
    LufsMeter lufsMeter;
    int preparedBlockSize = 0;

    std::atomic<bool> resetIntegratedFlag { false };
    std::atomic<bool> resetShortTermFlag  { false };

    // Sample-accurate delay lines used only when bypass is engaged, so the
    // passthrough signal stays time-aligned with the plugin's reported
    // latency instead of jumping ahead when bypass is toggled.
    juce::dsp::DelayLine<float> bypassDelayL { 8192 };
    juce::dsp::DelayLine<float> bypassDelayR { 8192 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleLimiterAudioProcessor)
};
