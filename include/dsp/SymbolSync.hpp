#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>

namespace RadiosondePI::DSP {

/**
 * @brief Gardner / Zero-crossing timing error detector for FSK/GFSK symbol synchronization.
 */
class SymbolSync {
public:
    using BitCallback = std::function<void(uint8_t bit)>;

    SymbolSync() = default;

    void init(float sampleRate, float symbolRate, float loopGain = 0.05f) {
        m_samplesPerSymbol = sampleRate / symbolRate;
        m_loopGain = loopGain;
        m_phase = 0.0f;
        m_prevSample = 0.0f;
        m_midSample = 0.0f;
        m_prevSign = 0;
    }

    void setBitCallback(BitCallback callback) {
        m_bitCallback = std::move(callback);
    }

    void processSample(float sample) {
        m_phase += 1.0f;

        // Mid-point sample for Gardner timing detector
        if (m_phase >= m_samplesPerSymbol * 0.5f && m_midSample == 0.0f) {
            m_midSample = sample;
        }

        if (m_phase >= m_samplesPerSymbol) {
            m_phase -= m_samplesPerSymbol;

            // Bit slice at strobe instant
            uint8_t bit = (sample > 0.0f) ? 1 : 0;
            if (m_bitCallback) {
                m_bitCallback(bit);
            }

            // Gardner Timing Error Detector: e = (prevSample - sample) * midSample
            float error = (m_prevSample - sample) * m_midSample;
            m_phase += error * m_loopGain;

            m_prevSample = sample;
            m_midSample = 0.0f;
        }
    }

    void processBlock(const float* samples, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            processSample(samples[i]);
        }
    }

private:
    float m_samplesPerSymbol{10.0f};
    float m_loopGain{0.05f};
    float m_phase{0.0f};
    float m_prevSample{0.0f};
    float m_midSample{0.0f};
    int m_prevSign{0};

    BitCallback m_bitCallback;
};

} // namespace RadiosondePI::DSP
