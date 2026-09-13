#pragma once

#include "sdr/ISdrDevice.hpp"
#include "dsp/RingBuffer.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <complex>

namespace RadiosondePI::SDR {

/**
 * @brief SDRplay RSPdx-R2, RSPdx, RSPduo, RSP1A, RSP2 driver using SDRplay API v3.
 * Supports multi-antenna switching (Antenna A, B, C), LNA gain attenuation,
 * notch filters, and high-dynamic-range 14-bit ADC sampling.
 */
class SdrplayDevice : public ISdrDevice {
public:
    explicit SdrplayDevice(size_t ringBufferCapacity = 1048576);
    ~SdrplayDevice() override;

    bool open(const SdrConfig& config) override;
    void close() override;

    bool setFrequency(uint32_t frequencyHz) override;
    bool setSampleRate(uint32_t sampleRate) override;
    bool setGain(int gainTenthsDb) override;
    bool setBiasTee(bool enable) override;

    // RSPdx-R2 specific controls
    bool setAntennaPort(const std::string& port); // "AntennaA", "AntennaB", "AntennaC"
    bool setLnaState(int lnaState); // 0 (0 dB attenuation) to max states
    bool setNotchFilters(bool broadcastFmNotch, bool dabNotch);

    bool startAsync(SampleBlockCallback callback) override;
    void stopAsync() override;

    [[nodiscard]] bool isOpen() const override { return m_isOpen || m_isSimulation; }
    [[nodiscard]] bool isRunning() const override { return m_running; }
    [[nodiscard]] const SdrConfig& getConfig() const override { return m_config; }
    [[nodiscard]] std::string getDeviceName() const override { return m_deviceName; }
    [[nodiscard]] SdrDriverType getDriverType() const override {
        return m_isSimulation ? SdrDriverType::Simulation : SdrDriverType::SdrplayRSP;
    }

private:
    void processShortSamples(const int16_t* xi, const int16_t* xq, size_t count);
    void simulationWorker();

    SdrConfig m_config;
    std::string m_deviceName{"SDRplay RSPdx-R2"};
    std::atomic<bool> m_isOpen{false};
    std::atomic<bool> m_running{false};
    bool m_isSimulation{false};

    SampleBlockCallback m_callback;
    std::thread m_simThread;

    std::vector<Complex32> m_conversionBuffer;
    void* m_deviceHandle{nullptr};
    void* m_deviceParams{nullptr};
};

} // namespace RadiosondePI::SDR
