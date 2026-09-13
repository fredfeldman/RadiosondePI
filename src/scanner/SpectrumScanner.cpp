#include "scanner/SpectrumScanner.hpp"
#include "utils/MathConstants.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <iostream>

#if __has_include(<fftw3.h>)
#include <fftw3.h>
#define HAS_FFTW3 1
#else
#define HAS_FFTW3 0
#endif

namespace RadiosondePI::Scanner {

SpectrumScanner::SpectrumScanner(const ScannerConfig& config)
    : m_config(config) {
    initWindow(m_config.fftSize);
    m_powerSpectrum.resize(m_config.fftSize, 0.0f);
    m_fftInput.resize(m_config.fftSize, Complex32(0.0f, 0.0f));
    m_fftOutput.resize(m_config.fftSize, Complex32(0.0f, 0.0f));
    initFft();
}

SpectrumScanner::~SpectrumScanner() {
    cleanupFft();
}

SpectrumScanner::SpectrumScanner(SpectrumScanner&& other) noexcept
    : m_config(other.m_config)
    , m_peakCallback(std::move(other.m_peakCallback))
    , m_window(std::move(other.m_window))
    , m_powerSpectrum(std::move(other.m_powerSpectrum))
    , m_fftInput(std::move(other.m_fftInput))
    , m_fftOutput(std::move(other.m_fftOutput))
    , m_fftwPlan(other.m_fftwPlan) {
    other.m_fftwPlan = nullptr;
}

SpectrumScanner& SpectrumScanner::operator=(SpectrumScanner&& other) noexcept {
    if (this != &other) {
        cleanupFft();
        m_config = other.m_config;
        m_peakCallback = std::move(other.m_peakCallback);
        m_window = std::move(other.m_window);
        m_powerSpectrum = std::move(other.m_powerSpectrum);
        m_fftInput = std::move(other.m_fftInput);
        m_fftOutput = std::move(other.m_fftOutput);
        m_fftwPlan = other.m_fftwPlan;
        other.m_fftwPlan = nullptr;
    }
    return *this;
}

void SpectrumScanner::cleanupFft() {
#if HAS_FFTW3
    if (m_fftwPlan) {
        fftwf_destroy_plan(static_cast<fftwf_plan>(m_fftwPlan));
        m_fftwPlan = nullptr;
    }
#endif
}

void SpectrumScanner::initFft() {
    cleanupFft();
#if HAS_FFTW3
    if (m_config.fftSize > 0) {
        m_fftwPlan = fftwf_plan_dft_1d(
            static_cast<int>(m_config.fftSize),
            reinterpret_cast<fftwf_complex*>(m_fftInput.data()),
            reinterpret_cast<fftwf_complex*>(m_fftOutput.data()),
            FFTW_FORWARD,
            FFTW_ESTIMATE
        );
    }
#endif
}

void SpectrumScanner::setConfig(const ScannerConfig& config) {
    m_config = config;
    initWindow(m_config.fftSize);
    m_powerSpectrum.resize(m_config.fftSize, 0.0f);
    m_fftInput.resize(m_config.fftSize, Complex32(0.0f, 0.0f));
    m_fftOutput.resize(m_config.fftSize, Complex32(0.0f, 0.0f));
    initFft();
}

void SpectrumScanner::initWindow(size_t size) {
    m_window.resize(size);
    for (size_t i = 0; i < size; ++i) {
        // Hann window: 0.5 * (1 - cos(2*pi*n / (N-1)))
        m_window[i] = 0.5f * (1.0f - std::cos(2.0f * RadiosondePI::Math::PiF * static_cast<float>(i) / (size - 1)));
    }
}

void SpectrumScanner::setPeakCallback(PeakFoundCallback callback) {
    m_peakCallback = std::move(callback);
}

static void computeCooleyTukeyFft(std::vector<Complex32>& a, bool invert) {
    size_t n = a.size();
    if (n <= 1) return;

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * RadiosondePI::Math::PiF / len * (invert ? -1 : 1);
        Complex32 wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            Complex32 w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                Complex32 u = a[i + j];
                Complex32 v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

std::vector<PeakResult> SpectrumScanner::analyzeBlock(const Complex32* samples, size_t count, uint32_t centerFreqHz, float sampleRate) {
    std::vector<PeakResult> peaks;
    if (count < m_config.fftSize || m_config.fftSize == 0) return peaks;

    size_t N = m_config.fftSize;

    // 1. Apply Hann Window
    for (size_t i = 0; i < N; ++i) {
        m_fftInput[i] = samples[i] * m_window[i];
    }

    // 2. Compute Fast Fourier Transform
#if HAS_FFTW3
    if (m_fftwPlan) {
        fftwf_execute(static_cast<fftwf_plan>(m_fftwPlan));
    } else {
        m_fftOutput = m_fftInput;
        computeCooleyTukeyFft(m_fftOutput, false);
    }
#else
    m_fftOutput = m_fftInput;
    computeCooleyTukeyFft(m_fftOutput, false);
#endif

    // 3. Compute FFT-Shifted Power Spectrum in dB
    float totalPower = 0.0f;
    for (size_t k = 0; k < N; ++k) {
        size_t shiftedIdx = (k + N / 2) % N;
        const auto& val = m_fftOutput[shiftedIdx];
        float magSq = (val.real() * val.real() + val.imag() * val.imag()) / static_cast<float>(N * N);
        float powerDb = 10.0f * std::log10(std::max(magSq, 1e-12f));
        m_powerSpectrum[k] = powerDb;
        totalPower += powerDb;
    }

    float noiseFloorDb = totalPower / static_cast<float>(N);

    // 4. Peak Detection with Detection Threshold
    for (size_t k = 2; k < N - 2; ++k) {
        float val = m_powerSpectrum[k];
        float snr = val - noiseFloorDb;

        if (snr >= m_config.detectionThresholdDb &&
            val > m_powerSpectrum[k - 1] && val > m_powerSpectrum[k + 1] &&
            val > m_powerSpectrum[k - 2] && val > m_powerSpectrum[k + 2]) {
            
            // Frequency calculation with bin interpolation
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
