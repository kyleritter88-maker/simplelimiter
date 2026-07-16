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

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "attack", 1 },
        "Attack",
        juce::NormalisableRange<float> (0.05f, 20.0f, 0.01f, 0.35f), // skewed for finer control at fast end
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "release", 1 },
        "Release",
        juce::NormalisableRange<float> (20.0f, 1000.0f, 0.1f, 0.4f),
        400.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ceiling", 1 },
        "Ceiling",
        juce::NormalisableRange<float> (-6.0f, 0.0f, 0.01f),
        -1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dBTP")));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "osFactor", 1 },
        "Oversampling",
        juce::StringArray { "2x", "4x", "8x" },
        1)); // default "4x", matching the original fixed behavior

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "dither", 1 },
        "Dither",
        false));

    return { params.begin(), params.end() };
}

void SimpleLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    preparedBlockSize = samplesPerBlock;

    int osChoiceIndex = (int) apvts.getRawParameterValue ("osFactor")->load(); // 0=2x,1=4x,2=8x
    int factorLog2 = osChoiceIndex + 1;

    limiterL.prepare (sampleRate, samplesPerBlock, factorLog2);
    limiterR.prepare (sampleRate, samplesPerBlock, factorLog2);
    lufsMeter.prepare (sampleRate, 2);

    int latSamples = (int) std::round (limiterL.getLatencySamples());
    setLatencySamples (latSamples);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    bypassDelayL.prepare (spec);
    bypassDelayR.prepare (spec);
    bypassDelayL.setDelay ((float) latSamples);
    bypassDelayR.setDelay ((float) latSamples);
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

    if (resetIntegratedFlag.exchange (false))
        lufsMeter.resetIntegrated();
    if (resetShortTermFlag.exchange (false))
        lufsMeter.resetShortTerm();

    bool bypassed = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    if (bypassed)
    {
        // Sample-accurate delay-matched passthrough, so toggling bypass
        // doesn't shift the signal relative to the plugin's reported latency.
        if (buffer.getNumChannels() > 0)
        {
            auto* data = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
            {
                bypassDelayL.pushSample (0, data[i]);
                data[i] = bypassDelayL.popSample (0);
            }
        }
        if (buffer.getNumChannels() > 1)
        {
            auto* data = buffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
            {
                bypassDelayR.pushSample (0, data[i]);
                data[i] = bypassDelayR.popSample (0);
            }
        }

        gainReductionDbL.store (0.0f);
        gainReductionDbR.store (0.0f);
        if (buffer.getNumChannels() > 0)
            peakLevelDbL.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, 0, numSamples), -100.0f));
        if (buffer.getNumChannels() > 1)
            peakLevelDbR.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (1, 0, numSamples), -100.0f));
    }
    else
    {
        float gainDb = apvts.getRawParameterValue ("gain")->load();
        float gainLinear = juce::Decibels::decibelsToGain (gainDb);

        float ceilingDb = apvts.getRawParameterValue ("ceiling")->load();
        float ceilingLinear = juce::Decibels::decibelsToGain (ceilingDb);

        float attackMs = apvts.getRawParameterValue ("attack")->load();
        float releaseMs = apvts.getRawParameterValue ("release")->load();

        int osChoiceIndex = (int) apvts.getRawParameterValue ("osFactor")->load();
        int factorLog2 = osChoiceIndex + 1;

        bool ditherOn = apvts.getRawParameterValue ("dither")->load() > 0.5f;

        double sr = getSampleRate();
        bool reinitL = limiterL.reinitIfNeeded (sr, preparedBlockSize, factorLog2);
        bool reinitR = limiterR.reinitIfNeeded (sr, preparedBlockSize, factorLog2);
        if (reinitL || reinitR)
        {
            // Oversampling factor changed — latency changed with it. Update
            // the host and re-align the bypass path so switching bypass
            // in/out afterward still stays in sync.
            int newLatSamples = (int) std::round (limiterL.getLatencySamples());
            setLatencySamples (newLatSamples);
            bypassDelayL.setDelay ((float) newLatSamples);
            bypassDelayR.setDelay ((float) newLatSamples);
        }

        limiterL.setTimes (attackMs, releaseMs);
        limiterR.setTimes (attackMs, releaseMs);
        limiterL.setDitherEnabled (ditherOn);
        limiterR.setDitherEnabled (ditherOn);

        // Apply input drive
        buffer.applyGain (gainLinear);

        if (buffer.getNumChannels() > 0)
        {
            float grL = limiterL.processBlock (buffer.getWritePointer (0), numSamples, ceilingLinear);
            gainReductionDbL.store (grL);
            peakLevelDbL.store (limiterL.getPeakLevelDb());
        }
        if (buffer.getNumChannels() > 1)
        {
            float grR = limiterR.processBlock (buffer.getWritePointer (1), numSamples, ceilingLinear);
            gainReductionDbR.store (grR);
            peakLevelDbR.store (limiterR.getPeakLevelDb());
        }
    }

    // Feed LUFS meter with the actual output signal (post-limiter, or the
    // delay-matched dry signal when bypassed) — this is what's really being
    // sent downstream, and what you'd compare against another limiter's
    // output loudness.
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
