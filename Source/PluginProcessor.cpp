#include "PluginProcessor.h"
#include "PluginEditor.h"

SimpleLimiterAudioProcessor::SimpleLimiterAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleLimiterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // The single "big knob" — input drive into the limiter, in dB.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 },
        "Gain",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.01f),
        6.0f));

    return { params.begin(), params.end() };
}

void SimpleLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    limiterL.prepare (sampleRate, samplesPerBlock);
    limiterR.prepare (sampleRate, samplesPerBlock);
    lufsMeter.prepare (sampleRate, 2);

    setLatencySamples ((int) std::round (limiterL.getLatencySamples()));
}

bool SimpleLimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;
    return true;
}

void SimpleLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    float gainDb = apvts.getRawParameterValue ("gain")->load();
    float gainLinear = juce::Decibels::decibelsToGain (gainDb);
    float ceilingLinear = juce::Decibels::decibelsToGain (ceilingDbTP);

    // Apply input drive
    buffer.applyGain (gainLinear);

    if (buffer.getNumChannels() > 0)
    {
        float grL = limiterL.processBlock (buffer.getWritePointer (0), numSamples, ceilingLinear);
        gainReductionDbL.store (grL);
    }
    if (buffer.getNumChannels() > 1)
    {
        float grR = limiterR.processBlock (buffer.getWritePointer (1), numSamples, ceilingLinear);
        gainReductionDbR.store (grR);
    }

    // Feed LUFS meter with the actual output (post-limiter) signal — this is
    // what's really being sent downstream, and what you'd compare against
    // another limiter's output loudness.
    if (buffer.getNumChannels() >= 2)
    {
        auto* left  = buffer.getReadPointer (0);
        auto* right = buffer.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            float pair[2] = { left[i], right[i] };
            lufsMeter.pushSample (pair);
        }
    }

    lufsIntegrated.store ((float) lufsMeter.getIntegratedLufs());
    lufsShortTerm.store  ((float) lufsMeter.getShortTermLufs());
}

juce::AudioProcessorEditor* SimpleLimiterAudioProcessor::createEditor()
{
    return new SimpleLimiterAudioProcessorEditor (*this);
}

void SimpleLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void SimpleLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleLimiterAudioProcessor();
}
