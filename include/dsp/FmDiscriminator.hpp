#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <numbers>

namespace RadiosondePI::DSP {

using Complex32 = std::complex<float>;

class FmDiscriminator {
public:
    FmDiscriminator() : m_lastSample(1.0f, 0.0f) {}

    float processSample(Complex32 sample) {
        // Differential conjugate multiplication: s[n] * conj(s[n-1])
        Complex32 product = sample * std::conj(m_lastSample);
        m_lastSample = sample;

        // Angle in radians [-pi, pi] normalized to [-1.0, 1.0]
        float angle = std::atan2(product.imag(), product.real());
        return angle / std::numbers::pi_v<float>;
    }

    void processBlock(const Complex32* input, size_t count, std::vector<float>& output) {
        output.resize(count);
        for (size_t i = 0; i < count; ++i) {
            output[i] = processSample(input[i]);
        }
    }

    void reset() {
        m_lastSample = Complex32(1.0f, 0.0f);
    }

private:
    Complex32 m_lastSample;
};

} // namespace RadiosondePI::DSP
