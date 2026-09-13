#pragma once

#include <complex>
#include <vector>
#include <cmath>
#include <numbers>

namespace RadiosondePI::DSP {

using Complex32 = std::complex<float>;

/**
 * @brief Automatic Frequency Control (AFC) tracking and digital baseband frequency rotator.
 */
class Afc {
public:
    Afc() = default;

    void init(float sampleRate, float alpha = 0.01f) {
        m_sampleRate = sampleRate;
        m_alpha = alpha;
        m_frequencyOffsetHz = 0.0f;
        m_phase = 0.0f;
    }

    void updateOffsetEstimate(float meanDiscriminatorOutput) {
        // Discriminator output is normalized to [-1.0, 1.0] representing [-sampleRate/2, sampleRate/2]
        float instantOffsetHz = meanDiscriminatorOutput * (m_sampleRate * 0.5f);
        m_frequencyOffsetHz = (1.0f - m_alpha) * m_frequencyOffsetHz + m_alpha * instantOffsetHz;
    }

    [[nodiscard]] float getFrequencyOffsetHz() const {
        return m_frequencyOffsetHz;
    }

    void rotateBlock(const Complex32* input, size_t count, std::vector<Complex32>& output) {
        output.resize(count);
        float phaseInc = -2.0f * std::numbers::pi_v<float> * (m_frequencyOffsetHz / m_sampleRate);

        for (size_t i = 0; i < count; ++i) {
            Complex32 rotator(std::cos(m_phase), std::sin(m_phase));
            output[i] = input[i] * rotator;
            m_phase += phaseInc;
            if (m_phase > std::numbers::pi_v<float>) {
                m_phase -= 2.0f * std::numbers::pi_v<float>;
            } else if (m_phase < -std::numbers::pi_v<float>) {
                m_phase += 2.0f * std::numbers::pi_v<float>;
            }
        }
    }

    void reset() {
        m_frequencyOffsetHz = 0.0f;
        m_phase = 0.0f;
    }

private:
    float m_sampleRate{48000.0f};
    float m_alpha{0.01f};
    float m_frequencyOffsetHz{0.0f};
    float m_phase{0.0f};
};

} // namespace RadiosondePI::DSP
