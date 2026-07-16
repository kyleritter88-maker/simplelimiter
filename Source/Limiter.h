#pragma once
#include <juce_dsp/juce_dsp.h>
#include <deque>
#include <vector>

// Single-channel true-peak lookahead limiter.
// Architecture:
//   1. 4x oversample (catches inter-sample "true peaks")
//   2. Lookahead delay line + sliding-window-maximum peak detector
//   3. Gain computer with instant attack (governed by lookahead time)
//      and program-dependent release (blends a fast + slow time constant
//      based on how deep/sustained the current gain reduction is, so
//      short transients release quickly but sustained loud passages
//      release more slowly and stay smoother/less pumpy).
//   4. Downsample back to original rate.
class ChannelLimiter
{
public:
    // factorLog2: 1 = 2x oversampling, 2 = 4x, 3 = 8x.
    void prepare (double sampleRateIn, int maxBlockSize, int factorLog2 = 2, int lookaheadMs = 5)
    {
        sampleRate = sampleRateIn;
        preparedBlockSize = maxBlockSize;
        preparedFactorLog2 = factorLog2;

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, factorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);

        osSampleRate = sampleRate * (double) oversampler->getOversamplingFactor();

        lookaheadSamples = std::max (1, (int) std::round (osSampleRate * lookaheadMs / 1000.0));

        delayLine.assign ((size_t) lookaheadSamples + 1, 0.0f);
        writePos = 0;

        peakWindow.clear();

        currentGain = 1.0f;
        gainReductionDb = 0.0f;

        fastReleaseCoeff = (float) std::exp (-1.0 / (osSampleRate * 0.050));  // default 50ms
        slowReleaseCoeff = (float) std::exp (-1.0 / (osSampleRate * 0.400));  // default 400ms
        attackCoeff = (float) std::exp (-1.0 / (osSampleRate * 0.0005));      // default 0.5ms

        // Tracks how SUSTAINED the gain reduction is (separate from how deep
        // it is at this instant), so a single sharp transient doesn't get
        // mistaken for a long loud passage and needlessly slow the release.
        depthEnvelope = 0.0f;
        depthSmoothCoeff = (float) std::exp (-1.0 / (osSampleRate * 0.150)); // 150ms

        sampleIndex = 0;
    }

    // Cheap check called once per block. If the oversampling factor has
    // changed since the last prepare(), fully reinitializes (this causes a
    // brief discontinuity — unavoidable when the DSP engine itself changes
    // shape — and reports the new latency via the returned bool so the
    // caller can update the host and any delay-matched bypass path).
    bool reinitIfNeeded (double sampleRateIn, int maxBlockSize, int factorLog2)
    {
        if (factorLog2 == preparedFactorLog2 && sampleRateIn == sampleRate && maxBlockSize == preparedBlockSize)
            return false;

        prepare (sampleRateIn, maxBlockSize, factorLog2);
        return true;
    }

    void reset()
    {
        std::fill (delayLine.begin(), delayLine.end(), 0.0f);
        peakWindow.clear();
        currentGain = 1.0f;
        gainReductionDb = 0.0f;
        depthEnvelope = 0.0f;
        peakLevelDb = -100.0f;
        writePos = 0;
        sampleIndex = 0;
        if (oversampler) oversampler->reset();
    }

    // Call once per block before processBlock() to update attack/release times
    // from user-controlled parameters. Cheap (a couple of exp() calls).
    void setTimes (float attackMs, float releaseMs)
    {
        attackCoeff = (float) std::exp (-1.0 / (osSampleRate * juce::jmax (0.02, (double) attackMs) / 1000.0));

        float slowMs = juce::jmax (5.0f, releaseMs);
        float fastMs = juce::jlimit (5.0f, slowMs, slowMs / 8.0f);
        slowReleaseCoeff = (float) std::exp (-1.0 / (osSampleRate * slowMs / 1000.0));
        fastReleaseCoeff = (float) std::exp (-1.0 / (osSampleRate * fastMs / 1000.0));
    }

    void setDitherEnabled (bool enabled) { ditherEnabled = enabled; }

    // Processes one block in place. ceilingLinear is the true-peak ceiling (e.g. 0.891 for -1dBFS).
    // Returns the max instantaneous gain reduction (dB) seen during this block, for metering.
    float processBlock (float* samples, int numSamples, float ceilingLinear)
    {
        juce::dsp::AudioBlock<float> block (&samples, 1, (size_t) numSamples);
        juce::dsp::AudioBlock<float> osBlock = oversampler->processSamplesUp (block);

        float maxGrDb = 0.0f;
        auto* osData = osBlock.getChannelPointer (0);
        int osNumSamples = (int) osBlock.getNumSamples();

        for (int i = 0; i < osNumSamples; ++i)
        {
            float in = osData[i];

            // push into lookahead delay
            delayLine[(size_t) writePos] = in;

            // maintain sliding-window max of |sample| over the lookahead horizon
            float absVal = std::abs (in);
            while (! peakWindow.empty() && peakWindow.back().second <= absVal)
                peakWindow.pop_back();
            peakWindow.push_back ({ sampleIndex, absVal });
            while (! peakWindow.empty() && peakWindow.front().first <= sampleIndex - lookaheadSamples)
                peakWindow.pop_front();

            float windowPeak = peakWindow.empty() ? 0.0f : peakWindow.front().second;

            // required gain to keep windowPeak under ceiling
            float targetGain = 1.0f;
            if (windowPeak > 1.0e-9f)
                targetGain = std::min (1.0f, ceilingLinear / windowPeak);

            if (targetGain < currentGain)
            {
                // attack: smoothed toward target at the user-set attack time.
                // The hard safety clamp below still guarantees the true-peak
                // ceiling even if gain hasn't fully caught up yet.
                currentGain = targetGain + (currentGain - targetGain) * attackCoeff;
            }
            else
            {
                // release: program-dependent blend of fast/slow based on the
                // SUSTAIN envelope (how long GR has been deep), not how deep
                // it is this instant. This keeps brief transients snappy
                // while genuinely sustained loud passages stay smooth.
                float releaseCoeff = juce::jmap (depthEnvelope, fastReleaseCoeff, slowReleaseCoeff);
                currentGain = targetGain + (currentGain - targetGain) * releaseCoeff;
            }

            // Update the sustain envelope from the gain we just computed.
            float instDepthDb = -juce::Decibels::gainToDecibels (currentGain, -60.0f);
            float instDepth = juce::jlimit (0.0f, 1.0f, instDepthDb / 12.0f); // 0dB->0, 12dB+->1
            depthEnvelope = depthEnvelope * depthSmoothCoeff + instDepth * (1.0f - depthSmoothCoeff);

            int readPos = (writePos - lookaheadSamples + (int) delayLine.size()) % (int) delayLine.size();
            float delayed = delayLine[(size_t) readPos];
            float outSample = delayed * currentGain;

            // hard safety clamp in case of any overshoot
            outSample = juce::jlimit (-ceilingLinear, ceilingLinear, outSample);

            osData[i] = outSample;

            float grDb = -juce::Decibels::gainToDecibels (currentGain, -60.0f);
            maxGrDb = std::max (maxGrDb, grDb);

            writePos = (writePos + 1) % (int) delayLine.size();
            ++sampleIndex;
        }

        oversampler->processSamplesDown (block);
        gainReductionDb = maxGrDb;

        float blockPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            if (ditherEnabled)
            {
                // TPDF (triangular) dither sized to a 16-bit LSB — applied
                // here, to the final downsampled signal, since dither only
                // makes sense as the last step before requantization. Adding
                // it to the oversampled intermediate signal would have its
                // statistics altered by the decimation filter.
                float r1 = ditherRandom.nextFloat();
                float r2 = ditherRandom.nextFloat();
                samples[i] += (r1 - r2) * ditherAmplitude;
            }
            blockPeak = std::max (blockPeak, std::abs (samples[i]));
        }
        peakLevelDb = juce::Decibels::gainToDecibels (blockPeak, -100.0f);

        return maxGrDb;
    }

    float getLatencySamples() const
    {
        return oversampler ? (oversampler->getLatencyInSamples() + (float) lookaheadSamples / (float) oversampler->getOversamplingFactor()) : 0.0f;
    }

    float getGainReductionDb() const { return gainReductionDb; }
    float getPeakLevelDb() const { return peakLevelDb; }

private:
    double sampleRate = 48000.0;
    double osSampleRate = 96000.0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int preparedFactorLog2 = 2;
    int preparedBlockSize = 0;

    int lookaheadSamples = 1;
    std::vector<float> delayLine;
    int writePos = 0;
    long sampleIndex = 0;

    std::deque<std::pair<long, float>> peakWindow;

    float currentGain = 1.0f;
    float gainReductionDb = 0.0f;
    float attackCoeff = 0.0f;
    float fastReleaseCoeff = 0.0f;
    float slowReleaseCoeff = 0.0f;
    float depthEnvelope = 0.0f;
    float depthSmoothCoeff = 0.0f;
    float peakLevelDb = -100.0f;

    bool ditherEnabled = false;
    juce::Random ditherRandom;
    static constexpr float ditherAmplitude = 1.0f / 32768.0f; // 16-bit LSB
};
