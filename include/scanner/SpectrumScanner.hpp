#pragma once

#include <vector>
#include <complex>
#include <cstdint>
#include <functional>
#include <atomic>

namespace RadiosondePI::Scanner {

using Complex32 = std::complex<float>;

struct PeakResult {
    uint32_t frequencyHz{0};
    float powerDb{0.0f};
    float snrDb{0.0f};
};

struct ScannerConfig {
    uint32_t minFrequencyHz{400050000};
    uint32_t maxFrequencyHz{406000000};
    uint32_t stepHz{1800000};
    uint32_t fftSize{2048};
    float detectionThresholdDb{8.0f};
    uint32_t dwellTimeMs{200};
};

class SpectrumScanner {
public:
    using PeakFoundCallback = std::function<void(const PeakResult&)>;

    explicit SpectrumScanner(const ScannerConfig& config = ScannerConfig{});
    ~SpectrumScanner() = default;

    void setConfig(const ScannerConfig& config);
    [[nodiscard]] const ScannerConfig& getConfig() const { return m_config; }

    void setPeakCallback(PeakFoundCallback callback);

    // Analyzes a single I/Q block at the current center frequency and detects peaks
    std::vector<PeakResult> analyzeBlock(const Complex32* samples, size_t count, uint32_t centerFreqHz, float sampleRate);

private:
    ScannerConfig m_config;
    PeakFoundCallback m_peakCallback;

    std::vector<float> m_window;
    std::vector<float> m_powerSpectrum;
    void initWindow(size_t size);
};

} // namespace RadiosondePI::Scanner
