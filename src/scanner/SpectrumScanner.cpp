#include "scanner/SpectrumScanner.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <numbers>

namespace RadiosondePI::Scanner {

SpectrumScanner::SpectrumScanner(const ScannerConfig& config)
    : m_config(config) {
    initWindow(m_config.fftSize);
    m_powerSpectrum.resize(m_config.fftSize);
}

void SpectrumScanner::setConfig(const ScannerConfig& config) {
    m_config = config;
    initWindow(m_config.fftSize);
    m_powerSpectrum.resize(m_config.fftSize);
}

void SpectrumScanner::initWindow(size_t size) {
    m_window.resize(size);
    for (size_t i = 0; i < size; ++i) {
        // Hann window: 0.5 * (1 - cos(2*pi*n / (N-1)))
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / (size - 1)));
    }
}

void SpectrumScanner::setPeakCallback(PeakFoundCallback callback) {
    m_peakCallback = std::move(callback);
}

std::vector<PeakResult> SpectrumScanner::analyzeBlock(const Complex32* samples, size_t count, uint32_t centerFreqHz, float sampleRate) {
    std::vector<PeakResult> peaks;
    if (count < m_config.fftSize) return peaks;

    size_t N = m_config.fftSize;

    // Direct discrete power estimation across N bins (or FFT)
    // For fast peak scanning on Raspberry Pi:
    std::vector<Complex32> windowed(N);
    for (size_t i = 0; i < N; ++i) {
        windowed[i] = samples[i] * m_window[i];
    }

    // Compute simple power binning for peak detection
    float totalPower = 0.0f;
    for (size_t k = 0; k < N; ++k) {
        // Approximate bin energy
        Complex32 sum(0.0f, 0.0f);
        // DFT kernel calculation or FFT
        float angleStep = -2.0f * std::numbers::pi_v<float> * static_cast<float>(k) / static_cast<float>(N);
        for (size_t n = 0; n < std::min(N, (size_t)256); ++n) {
            float a = angleStep * n;
            Complex32 rot(std::cos(a), std::sin(a));
            sum += windowed[n] * rot;
        }

        float magSq = sum.real() * sum.real() + sum.imag() * sum.imag();
        float powerDb = 10.0f * std::log10(std::max(magSq, 1e-12f));
        m_powerSpectrum[k] = powerDb;
        totalPower += powerDb;
    }

    float noiseFloorDb = totalPower / static_cast<float>(N);

    // Peak detection with threshold
    for (size_t k = 2; k < N - 2; ++k) {
        float val = m_powerSpectrum[k];
        float snr = val - noiseFloorDb;

        if (snr >= m_config.detectionThresholdDb &&
            val > m_powerSpectrum[k - 1] && val > m_powerSpectrum[k + 1]) {
            
            // Calculate frequency offset for bin k (FFT shift logic)
            float binFreqOffset = (static_cast<float>(k) / static_cast<float>(N) - 0.5f) * sampleRate;
            uint32_t detectedFreq = static_cast<uint32_t>(static_cast<int64_t>(centerFreqHz) + static_cast<int64_t>(binFreqOffset));

            if (detectedFreq >= m_config.minFrequencyHz && detectedFreq <= m_config.maxFrequencyHz) {
                PeakResult peak{detectedFreq, val, snr};
                peaks.push_back(peak);

                if (m_peakCallback) {
                    m_peakCallback(peak);
                }
            }
        }
    }

    return peaks;
}

} // namespace RadiosondePI::Scanner
