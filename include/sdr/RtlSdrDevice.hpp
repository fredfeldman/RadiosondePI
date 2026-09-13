#pragma once

#include "sdr/ISdrDevice.hpp"
#include "dsp/RingBuffer.hpp"
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <complex>

struct rtlsdr_dev;

namespace RadiosondePI::SDR {

class RtlSdrDevice : public ISdrDevice {
public:
    explicit RtlSdrDevice(size_t ringBufferCapacity = 1048576);
    ~RtlSdrDevice() override;

    bool open(const SdrConfig& config) override;
    void close() override;

    bool setFrequency(uint32_t frequencyHz) override;
    bool setSampleRate(uint32_t sampleRate) override;
    bool setGain(int gainTenthsDb) override; // e.g. 297 for 29.7 dB, 0 for auto
    bool setBiasTee(bool enable) override;

    bool startAsync(SampleBlockCallback callback) override;
    void stopAsync() override;

    [[nodiscard]] bool isOpen() const override { return m_dev != nullptr || m_isSimulation; }
    [[nodiscard]] bool isRunning() const override { return m_running; }
    [[nodiscard]] const SdrConfig& getConfig() const override { return m_config; }
    [[nodiscard]] std::string getDeviceName() const override { return m_deviceName; }
    [[nodiscard]] SdrDriverType getDriverType() const override {
        return m_isSimulation ? SdrDriverType::Simulation : SdrDriverType::RtlSdr;
    }

private:
    static void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx);
    void processRawBytes(const uint8_t* buf, uint32_t len);
    void workerThreadFunc();

    rtlsdr_dev* m_dev{nullptr};
    SdrConfig m_config;
    std::string m_deviceName{"Generic RTL2832U"};
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    SampleBlockCallback m_callback;

    DSP::RingBuffer<Complex32> m_ringBuffer;
    std::vector<Complex32> m_conversionBuffer;
    bool m_isSimulation{false};
};

} // namespace RadiosondePI::SDR
