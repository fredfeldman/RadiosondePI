#pragma once

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

using Complex32 = std::complex<float>;

struct SdrConfig {
    int deviceIndex{0};
    uint32_t frequencyHz{403000000};
    uint32_t sampleRate{2400000};
    int gain{0}; // 0 = auto
    int ppmCorrection{0};
    bool biasTee{false};
};

class RtlSdrDevice {
public:
    using SampleBlockCallback = std::function<void(const Complex32* samples, size_t count)>;

    explicit RtlSdrDevice(size_t ringBufferCapacity = 1048576);
    ~RtlSdrDevice();

    bool open(const SdrConfig& config);
    void close();

    bool setFrequency(uint32_t frequencyHz);
    bool setSampleRate(uint32_t sampleRate);
    bool setGain(int gainTenthsDb); // e.g. 297 for 29.7 dB, 0 for auto
    bool setBiasTee(bool enable);

    bool startAsync(SampleBlockCallback callback);
    void stopAsync();

    [[nodiscard]] bool isOpen() const { return m_dev != nullptr || m_isSimulation; }
    [[nodiscard]] bool isRunning() const { return m_running; }
    [[nodiscard]] const SdrConfig& getConfig() const { return m_config; }

private:
    static void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx);
    void processRawBytes(const uint8_t* buf, uint32_t len);
    void workerThreadFunc();

    rtlsdr_dev* m_dev{nullptr};
    SdrConfig m_config;
    std::atomic<bool> m_running{false};
    std::thread m_workerThread;
    SampleBlockCallback m_callback;

    DSP::RingBuffer<Complex32> m_ringBuffer;
    std::vector<Complex32> m_conversionBuffer;
    bool m_isSimulation{false};
};

} // namespace RadiosondePI::SDR
