#pragma once
#include <vector>
#include <deque>
#include <cmath>
#include <algorithm>

// ITU-R BS.1770-4 compliant loudness meter.
// Implements K-weighting (high-shelf + high-pass), 400ms gating blocks
// for Integrated loudness (with -70 LUFS absolute gate and relative gate
// at -10 LU below ungated loudness), and a rolling 3s window for
// Short-term loudness.

#ifndef LUFS_PI
#define LUFS_PI 3.14159265358979323846
#endif

class KWeightingFilter
{
public:
    void prepare (double sampleRate)
    {
        // Stage 1: high shelf (~+4dB above ~1.68kHz)
        {
            const double f0 = 1681.9744509555319;
            const double G  = 3.99984385397;
            const double Q  = 0.7071752369554193;
            computeHighShelf (sampleRate, f0, G, Q, stage1);
        }
        // Stage 2: high pass (~38Hz)
        {
            const double f0 = 38.13547087602444;
            const double Q  = 0.5003270373238773;
            computeHighPass (sampleRate, f0, Q, stage2);
        }
        reset();
    }

    void reset()
    {
        std::fill (std::begin (z1), std::end (z1), 0.0);
        std::fill (std::begin (z2), std::end (z2), 0.0);
    }

    float process (float x)
    {
        double y1 = processBiquad (stage1, z1, (double) x);
        double y2 = processBiquad (stage2, z2, y1);
        return (float) y2;
    }

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    Coeffs stage1, stage2;
    double z1[2] = { 0, 0 };
    double z2[2] = { 0, 0 };

    static double processBiquad (const Coeffs& c, double* z, double x)
    {
        double y = c.b0 * x + z[0];
        z[0] = c.b1 * x - c.a1 * y + z[1];
        z[1] = c.b2 * x - c.a2 * y;
        return y;
    }

    static void computeHighShelf (double sr, double f0, double gainDb, double Q, Coeffs& c)
    {
        double A  = std::pow (10.0, gainDb / 40.0);
        double w0 = 2.0 * M_PI * f0 / sr;
        double alpha = std::sin (w0) / (2.0 * Q);
        double cosW0 = std::cos (w0);
        double sqrtA = std::sqrt (A);

        double b0 =      A * ((A + 1) + (A - 1) * cosW0 + 2 * sqrtA * alpha);
        double b1 = -2 * A * ((A - 1) + (A + 1) * cosW0);
        double b2 =      A * ((A + 1) + (A - 1) * cosW0 - 2 * sqrtA * alpha);
        double a0 =          (A + 1) - (A - 1) * cosW0 + 2 * sqrtA * alpha;
        double a1 =      2 * ((A - 1) - (A + 1) * cosW0);
        double a2 =          (A + 1) - (A - 1) * cosW0 - 2 * sqrtA * alpha;

        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
    }

    static void computeHighPass (double sr, double f0, double Q, Coeffs& c)
    {
        double w0 = 2.0 * M_PI * f0 / sr;
        double alpha = std::sin (w0) / (2.0 * Q);
        double cosW0 = std::cos (w0);

        double b0 =  (1 + cosW0) / 2;
        double b1 = -(1 + cosW0);
        double b2 =  (1 + cosW0) / 2;
        double a0 =   1 + alpha;
        double a1 =  -2 * cosW0;
        double a2 =   1 - alpha;

        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
    }
};

class LufsMeter
{
public:
    void prepare (double sampleRateIn, int numChannelsIn)
    {
        sampleRate = sampleRateIn;
        numChannels = numChannelsIn;
        filters.resize ((size_t) numChannels);
        for (auto& f : filters) f.prepare (sampleRate);

        blockSizeSamples = (int) std::round (sampleRate * 0.4);   // 400ms
        hopSizeSamples   = (int) std::round (sampleRate * 0.1);   // 100ms (75% overlap)
        shortTermBlocks  = 30; // 3s / 100ms hop

        sumSquares.assign ((size_t) numChannels, 0.0);
        samplesInCurrentBlock = 0;

        gatingBlockLoudness.clear();
        shortTermHistory.clear();

        integratedLufs = -70.0;
        shortTermLufs  = -70.0;
    }

    // Call once per audio sample, per channel already K-weighted internally.
    void pushSample (const float* channelSamples)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float weighted = filters[(size_t) ch].process (channelSamples[ch]);
            sumSquares[(size_t) ch] += (double) weighted * (double) weighted;
        }

        ++samplesInCurrentBlock;

        if (samplesInCurrentBlock >= hopSizeSamples)
        {
            finishHopBlock();
        }
    }

    double getIntegratedLufs() const { return integratedLufs; }
    double getShortTermLufs()  const { return shortTermLufs; }

private:
    double sampleRate = 48000.0;
    int numChannels = 2;
    std::vector<KWeightingFilter> filters;

    int blockSizeSamples = 0;
    int hopSizeSamples = 0;
    int shortTermBlocks = 30;

    std::vector<double> sumSquares;
    int samplesInCurrentBlock = 0;

    // rolling buffer of per-hop mean-square sums (channel-summed, G=1 for L/R)
    std::deque<double> recentHopEnergies;
    std::vector<double> gatingBlockLoudness;
    std::vector<double> shortTermHistory;

    double integratedLufs = -70.0;
    double shortTermLufs = -70.0;

    void finishHopBlock()
    {
        double channelSum = 0.0;
        for (int ch = 0; ch < numChannels; ++ch)
            channelSum += sumSquares[(size_t) ch] / (double) samplesInCurrentBlock;

        recentHopEnergies.push_back (channelSum);
        // Keep enough hops to cover blockSize (400ms) at 100ms hop = 4 hops overlapped
        int hopsFor400ms = std::max (1, blockSizeSamples / hopSizeSamples);
        if ((int) recentHopEnergies.size() > 1000) recentHopEnergies.pop_front();

        if ((int) recentHopEnergies.size() >= hopsFor400ms)
        {
            double blockMean = 0.0;
            int count = 0;
            for (int i = (int) recentHopEnergies.size() - hopsFor400ms; i < (int) recentHopEnergies.size(); ++i)
            {
                blockMean += recentHopEnergies[(size_t) i];
                ++count;
            }
            blockMean /= (double) count;

            double blockLoudness = -0.691 + 10.0 * std::log10 (std::max (blockMean, 1e-12));
            gatingBlockLoudness.push_back (blockLoudness);
            if (gatingBlockLoudness.size() > 100000) gatingBlockLoudness.erase (gatingBlockLoudness.begin());

            updateIntegrated();
            updateShortTerm (blockMean);
        }

        for (auto& s : sumSquares) s = 0.0;
        samplesInCurrentBlock = 0;
    }

    void updateIntegrated()
    {
        // Absolute gate at -70 LUFS
        std::vector<double> above70;
        for (double l : gatingBlockLoudness)
            if (l > -70.0) above70.push_back (l);

        if (above70.empty()) { integratedLufs = -70.0; return; }

        double ungatedMeanEnergy = 0.0;
        for (double l : above70)
            ungatedMeanEnergy += std::pow (10.0, (l + 0.691) / 10.0);
        ungatedMeanEnergy /= (double) above70.size();
        double ungatedLoudness = -0.691 + 10.0 * std::log10 (std::max (ungatedMeanEnergy, 1e-12));

        // Relative gate at -10 LU below ungated loudness
        double relativeThreshold = ungatedLoudness - 10.0;
        std::vector<double> aboveRel;
        for (double l : above70)
            if (l > relativeThreshold) aboveRel.push_back (l);

        if (aboveRel.empty()) { integratedLufs = ungatedLoudness; return; }

        double finalMeanEnergy = 0.0;
        for (double l : aboveRel)
            finalMeanEnergy += std::pow (10.0, (l + 0.691) / 10.0);
        finalMeanEnergy /= (double) aboveRel.size();

        integratedLufs = -0.691 + 10.0 * std::log10 (std::max (finalMeanEnergy, 1e-12));
    }

    void updateShortTerm (double latestHopMean)
    {
        shortTermHistory.push_back (latestHopMean);
        if ((int) shortTermHistory.size() > shortTermBlocks)
            shortTermHistory.erase (shortTermHistory.begin());

        double mean = 0.0;
        for (double v : shortTermHistory) mean += v;
        mean /= (double) shortTermHistory.size();

        shortTermLufs = -0.691 + 10.0 * std::log10 (std::max (mean, 1e-12));
    }
};
