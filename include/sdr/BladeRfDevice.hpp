#pragma once

#include "sdr/ISdrDevice.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <vector>
#include <complex>

struct bladerf;

namespace RadiosondePI::SDR {

/**
 * @brief Nuand bladeRF (bladeRF 2.0 micro xA4/xA9 and bladeRF Classic) driver.
 * Supports full-duplex / multi-RX channels (RX1 / RX2), hardware AGC,
 * analog bandwidth filtering, bias-tee, and high-rate SC16_Q11 streaming via libbladeRF.
 */
class BladeRfDevice : public ISdrDevice {
public:
    explicit BladeRfDevice(size_t ringBufferCapacity = 1048576);
    ~BladeRfDevice() override;

    bool open(const SdrConfig& config) override;
    void close() override;

    bool setFrequency(uint32_t frequencyHz) override;
    bool setSampleRate(uint32_t sampleRate) override;
    bool setGain(int gainTenthsDb) override;
    bool setBiasTee(bool enable) override;

    // bladeRF specific controls
    bool setBandwidth(uint32_t bandwidthHz);
    bool setGainMode(const std::string& mode); // "manual", "fast", "slow", "hybrid"
    bool setRxChannel(int channel); // 0 = RX1, 1 = RX2

    bool startAsync(SampleBlockCallback callback) override;
    void stopAsync() override;

    [[nodiscard]] bool isOpen() const override { return m_dev != nullptr || m_isSimulation; }
    [[nodiscard]] bool isRunning() const override { return m_running; }
    [[nodiscard]] const SdrConfig& getConfig() const override { return m_config; }
    [[nodiscard]] std::string getDeviceName() const override { return m_deviceName; }
    [[nodiscard]] SdrDriverType getDriverType() const override {
        return m_isSimulation ? SdrDriverType::Simulation : SdrDriverType::BladeRF;
    }

private:
    void streamWorker();
    void simulationWorker();
    void processSc16Q11Samples(const int16_t* interleavedIq, size_t numComplexSamples);

    bladerf* m_dev{nullptr};
    SdrConfig m_config;
    std::string m_deviceName{"Nuand bladeRF"};
    std::atomic<bool> m_running{false};
    bool m_isSimulation{false};

    SampleBlockCallback m_callback;
    std::thread m_workerThread;

    std::vector<Complex32> m_conversionBuffer;
};

} // namespace RadiosondePI::SDR
