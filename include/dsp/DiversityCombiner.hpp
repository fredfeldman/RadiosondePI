#pragma once

#include "utils/MathConstants.hpp"
#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <numeric>

namespace RadiosondePI::DSP {

using Complex32 = std::complex<float>;

enum class DiversityMode {
    SelectionCombining = 0, // Picks the channel with the highest instantaneous SNR
    EqualGainCombining,     // Phase-aligns and sums channels with equal weight
    MaximalRatioCombining   // Phase-aligns and weights channels proportional to SNR
};

inline const char* diversityModeToString(DiversityMode mode) {
    switch (mode) {
        case DiversityMode::SelectionCombining: return "SelectionCombining";
        case DiversityMode::EqualGainCombining: return "EqualGainCombining";
        case DiversityMode::MaximalRatioCombining: return "MaximalRatioCombining";
        default: return "Unknown";
    }
}

class DiversityCombiner {
public:
    explicit DiversityCombiner(DiversityMode mode = DiversityMode::MaximalRatioCombining, size_t numChannels = 2)
        : m_mode(mode)
        , m_channelWeights(numChannels, 1.0f)
        , m_channelPhases(numChannels, 0.0f)
        , m_channelSnrDb(numChannels, 0.0f) {}

    void setMode(DiversityMode mode) {
        m_mode = mode;
    }

    [[nodiscard]] DiversityMode getMode() const {
        return m_mode;
    }

    [[nodiscard]] const std::vector<float>& getChannelSnr() const {
        return m_channelSnrDb;
    }

    /**
     * @brief Estimate instantaneous power/SNR and phase offset relative to channel 0.
     */
    void updateChannelEstimates(const Complex32* const* channels, size_t numChannels, size_t blockSize) {
        if (numChannels == 0 || blockSize == 0) return;

        if (m_channelWeights.size() != numChannels) {
            m_channelWeights.resize(numChannels, 1.0f);
            m_channelPhases.resize(numChannels, 0.0f);
            m_channelSnrDb.resize(numChannels, 0.0f);
        }

        const Complex32* refChannel = channels[0];

        for (size_t ch = 0; ch < numChannels; ++ch) {
            const Complex32* data = channels[ch];
            if (!data) continue;

            float powerSum = 0.0f;
            Complex32 xcorr(0.0f, 0.0f);

            for (size_t i = 0; i < blockSize; ++i) {
                float magSq = data[i].real() * data[i].real() + data[i].imag() * data[i].imag();
                powerSum += magSq;

                if (refChannel && ch > 0) {
                    // Cross-correlation with reference channel: ref * conj(ch)
                    xcorr += refChannel[i] * std::conj(data[i]);
                }
            }

            float avgPower = powerSum / static_cast<float>(blockSize);
            float snrDb = 10.0f * std::log10(std::max(avgPower, 1e-9f));
            m_channelSnrDb[ch] = snrDb;

            if (ch == 0) {
                m_channelPhases[0] = 0.0f;
                m_channelWeights[0] = 1.0f;
            } else {
                // Phase angle to align channel `ch` with reference channel 0
                float phaseDiff = std::atan2(xcorr.imag(), xcorr.real());
                m_channelPhases[ch] = phaseDiff;

                if (m_mode == DiversityMode::MaximalRatioCombining) {
                    m_channelWeights[ch] = std::sqrt(std::max(avgPower, 1e-6f));
                } else {
                    m_channelWeights[ch] = 1.0f;
                }
            }
        }
    }

    /**
     * @brief Zero-allocation multi-channel diversity combination.
     */
    size_t combineBlocks(const Complex32* const* channels, const size_t* sampleCounts,
                         size_t numChannels, Complex32* output, size_t maxOutputCapacity) {
        if (numChannels == 0 || !channels || !output || maxOutputCapacity == 0) return 0;

        size_t commonCount = maxOutputCapacity;
        for (size_t ch = 0; ch < numChannels; ++ch) {
            if (channels[ch]) {
                commonCount = std::min(commonCount, sampleCounts[ch]);
            }
        }

        if (commonCount == 0) return 0;

        updateChannelEstimates(channels, numChannels, commonCount);

        if (m_mode == DiversityMode::SelectionCombining) {
            // Find channel with maximum SNR
            size_t bestCh = 0;
            float bestSnr = -999.0f;
            for (size_t ch = 0; ch < numChannels; ++ch) {
                if (m_channelSnrDb[ch] > bestSnr) {
                    bestSnr = m_channelSnrDb[ch];
                    bestCh = ch;
                }
            }

            const Complex32* bestStream = channels[bestCh];
            std::copy_n(bestStream, commonCount, output);
            return commonCount;
        }

        // Equal Gain or Maximal Ratio Combining with phase alignment
        float weightSum = 0.0f;
        for (size_t ch = 0; ch < numChannels; ++ch) {
            if (channels[ch]) {
                weightSum += m_channelWeights[ch];
            }
        }
        if (weightSum <= 0.0f) weightSum = 1.0f;

        std::vector<Complex32> rotators(numChannels);
        for (size_t ch = 0; ch < numChannels; ++ch) {
            float phase = m_channelPhases[ch];
            rotators[ch] = Complex32(std::cos(phase), std::sin(phase)) * (m_channelWeights[ch] / weightSum);
        }

        for (size_t i = 0; i < commonCount; ++i) {
            Complex32 combined(0.0f, 0.0f);
            for (size_t ch = 0; ch < numChannels; ++ch) {
                if (channels[ch]) {
                    combined += channels[ch][i] * rotators[ch];
                }
            }
            output[i] = combined;
        }

        return commonCount;
    }

private:
    DiversityMode m_mode;
    std::vector<float> m_channelWeights;
    std::vector<float> m_channelPhases;
    std::vector<float> m_channelSnrDb;
};

} // namespace RadiosondePI::DSP
