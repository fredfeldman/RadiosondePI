#pragma once

#include "utils/MathConstants.hpp"
#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>

namespace RadiosondePI::DSP {

using Complex32 = std::complex<float>;

class FirFilter {
public:
    FirFilter() = default;

    void initLowPass(size_t numTaps, float sampleRate, float cutoffFreq) {
        m_taps.resize(numTaps);
        m_history.assign(numTaps, Complex32(0.0f, 0.0f));
        m_historyIndex = 0;

        float sum = 0.0f;
        int middle = static_cast<int>(numTaps) / 2;
        float normalizedCutoff = cutoffFreq / sampleRate;

        for (int i = 0; i < static_cast<int>(numTaps); ++i) {
            float n = static_cast<float>(i - middle);
            float sinc = (n == 0.0f) ? (2.0f * normalizedCutoff) : 
                std::sin(2.0f * RadiosondePI::Math::PiF * normalizedCutoff * n) / (RadiosondePI::Math::PiF * n);
            
            // Hamming window
            float window = 0.54f - 0.46f * std::cos(2.0f * RadiosondePI::Math::PiF * static_cast<float>(i) / (numTaps - 1));
            m_taps[i] = sinc * window;
            sum += m_taps[i];
        }

        // Normalize DC gain to 1.0
        if (sum != 0.0f) {
            for (auto& tap : m_taps) {
                tap /= sum;
            }
        }
    }

    Complex32 processSample(Complex32 input) {
        m_history[m_historyIndex] = input;
        
        Complex32 output(0.0f, 0.0f);
        size_t size = m_taps.size();
        size_t idx = m_historyIndex;

        for (size_t i = 0; i < size; ++i) {
            output += m_history[idx] * m_taps[i];
            if (idx == 0) {
                idx = size - 1;
            } else {
                --idx;
            }
        }

        m_historyIndex = (m_historyIndex + 1) % size;
        return output;
    }

    /**
     * @brief Optimized decimation FIR filter.
     * Computes the FIR convolution ONLY on every N-th (decimationFactor) sample,
     * avoiding redundant dot-product calculations on skipped samples.
     */
    void processBlockDecimate(const Complex32* input, size_t inputLength, size_t decimationFactor, std::vector<Complex32>& output) {
        if (inputLength == 0 || decimationFactor == 0) return;

        output.clear();
        output.reserve((inputLength + decimationFactor - 1) / decimationFactor);

        size_t size = m_taps.size();

        for (size_t i = 0; i < inputLength; ++i) {
            m_history[m_historyIndex] = input[i];

            if (m_decimationPhase == 0) {
                // Compute FIR dot product only at decimation output intervals
                Complex32 acc(0.0f, 0.0f);
                size_t idx = m_historyIndex;
                for (size_t k = 0; k < size; ++k) {
                    acc += m_history[idx] * m_taps[k];
                    if (idx == 0) {
                        idx = size - 1;
                    } else {
                        --idx;
                    }
                }
                output.push_back(acc);
            }

            m_historyIndex = (m_historyIndex + 1) % size;
            m_decimationPhase = (m_decimationPhase + 1) % decimationFactor;
        }
    }

    void reset() {
        std::fill(m_history.begin(), m_history.end(), Complex32(0.0f, 0.0f));
        m_historyIndex = 0;
        m_decimationPhase = 0;
    }

private:
    std::vector<float> m_taps;
    std::vector<Complex32> m_history;
    size_t m_historyIndex{0};
    size_t m_decimationPhase{0};
};

} // namespace RadiosondePI::DSP
