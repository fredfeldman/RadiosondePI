#pragma once

#include <cstdint>
#include <string>
#include <complex>
#include <functional>

namespace RadiosondePI::SDR {

using Complex32 = std::complex<float>;

enum class SdrDriverType {
    Auto = 0,
    RtlSdr,
    SdrplayRSP,
    BladeRF,
    Simulation
};

inline const char* sdrDriverTypeToString(SdrDriverType type) {
    switch (type) {
        case SdrDriverType::RtlSdr: return "RTL-SDR";
        case SdrDriverType::SdrplayRSP: return "SDRplay RSPdx-R2 / RSP";
        case SdrDriverType::BladeRF: return "Nuand bladeRF (2.0 micro / Classic)";
        case SdrDriverType::Simulation: return "Simulation";
        default: return "Auto";
    }
}

struct SdrConfig {
    std::string driver{"auto"}; // "auto", "rtlsdr", "sdrplay", "rspdx", "rsp", "bladerf"
    int deviceIndex{0};
    uint32_t frequencyHz{403000000};
    uint32_t sampleRate{2400000};
    int gain{0}; // 0 = auto, or gain in tenths of dB
    int ppmCorrection{0};
    bool biasTee{false};

    // SDRplay RSPdx-R2 / RSP specific settings
    std::string antennaPort{"AntennaA"}; // "AntennaA", "AntennaB", "AntennaC"
    int lnaState{0}; // LNA attenuation state (0 = maximum gain)
    bool broadcastNotch{false}; // Broadcast FM/DAB notch filter
    bool dabNotch{false};
    int ifType{0}; // 0 = Zero-IF, 1 = Low-IF (e.g. 1.62 MHz or 2.048 MHz)

    // Nuand bladeRF specific settings
    int rxChannel{0}; // 0 = RX1, 1 = RX2 (for bladeRF 2.0 micro xA9/xA4)
    uint32_t bandwidthHz{1500000}; // Analog LPF bandwidth
    std::string gainMode{"manual"}; // "manual", "fast", "slow", "hybrid"
};

class ISdrDevice {
public:
    using SampleBlockCallback = std::function<void(const Complex32* samples, size_t count)>;

    virtual ~ISdrDevice() = default;

    virtual bool open(const SdrConfig& config) = 0;
    virtual void close() = 0;

    virtual bool setFrequency(uint32_t frequencyHz) = 0;
    virtual bool setSampleRate(uint32_t sampleRate) = 0;
    virtual bool setGain(int gainTenthsDb) = 0;
    virtual bool setBiasTee(bool enable) = 0;

    virtual bool startAsync(SampleBlockCallback callback) = 0;
    virtual void stopAsync() = 0;

    [[nodiscard]] virtual bool isOpen() const = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    [[nodiscard]] virtual const SdrConfig& getConfig() const = 0;
    [[nodiscard]] virtual std::string getDeviceName() const = 0;
    [[nodiscard]] virtual SdrDriverType getDriverType() const = 0;
};

} // namespace RadiosondePI::SDR
