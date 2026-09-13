#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>
#include <numbers>

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
                std::sin(2.0f * std::numbers::pi_v<float> * normalizedCutoff * n) / (std::numbers::pi_v<float> * n);
            
            // Hamming window
            float window = 0.54f - 0.46f * std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / (numTaps - 1));
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

    void processBlockDecimate(const Complex32* input, size_t inputLength, size_t decimationFactor, std::vector<Complex32>& output) {
        output.clear();
        output.reserve(inputLength / decimationFactor);

        for (size_t i = 0; i < inputLength; ++i) {
            Complex32 filtered = processSample(input[i]);
            if (i % decimationFactor == 0) {
                output.push_back(filtered);
            }
        }
    }

private:
    std::vector<float> m_taps;
    std::vector<Complex32> m_history;
    size_t m_historyIndex{0};
};

} // namespace RadiosondePI::DSP
