#include "sdr/RtlSdrDevice.hpp"
#include <iostream>
#include <cstring>
#include <chrono>

#if __has_include(<rtl-sdr.h>)
#include <rtl-sdr.h>
#define HAS_LIBRTLSDR 1
#else
#define HAS_LIBRTLSDR 0
struct rtlsdr_dev {};
#endif

namespace RadiosondePI::SDR {

RtlSdrDevice::RtlSdrDevice(size_t ringBufferCapacity)
    : m_ringBuffer(ringBufferCapacity) {
    m_conversionBuffer.resize(131072);
}

RtlSdrDevice::~RtlSdrDevice() {
    stopAsync();
    close();
}

bool RtlSdrDevice::open(const SdrConfig& config) {
    m_config = config;

#if HAS_LIBRTLSDR
    int deviceCount = rtlsdr_get_device_count();
    if (deviceCount == 0) {
        std::cerr << "[SDR] Warning: No RTL-SDR devices found. Running in simulation mode." << std::endl;
        m_isSimulation = true;
        return true;
    }

    int r = rtlsdr_open(&m_dev, m_config.deviceIndex);
    if (r < 0) {
        std::cerr << "[SDR] Failed to open RTL-SDR device #" << m_config.deviceIndex << " (error: " << r << ")" << std::endl;
        m_dev = nullptr;
        m_isSimulation = true;
        return false;
    }

    setSampleRate(m_config.sampleRate);
    setFrequency(m_config.frequencyHz);
    setGain(m_config.gain);
    if (m_config.ppmCorrection != 0) {
        rtlsdr_set_freq_correction(m_dev, m_config.ppmCorrection);
    }
    if (m_config.biasTee) {
        setBiasTee(true);
    }

    rtlsdr_reset_buffer(m_dev);
    std::cout << "[SDR] Successfully initialized RTL-SDR device." << std::endl;
    return true;
#else
    std::cout << "[SDR] librtlsdr headers not found during compilation. Initializing simulated SDR." << std::endl;
    m_isSimulation = true;
    return true;
#endif
}

void RtlSdrDevice::close() {
    if (m_dev) {
#if HAS_LIBRTLSDR
        rtlsdr_close(m_dev);
#endif
        m_dev = nullptr;
    }
    m_isSimulation = false;
}

bool RtlSdrDevice::setFrequency(uint32_t frequencyHz) {
    m_config.frequencyHz = frequencyHz;
    if (!m_dev) return true;

#if HAS_LIBRTLSDR
    int r = rtlsdr_set_center_freq(m_dev, frequencyHz);
    return r == 0;
#else
    return true;
#endif
}

bool RtlSdrDevice::setSampleRate(uint32_t sampleRate) {
    m_config.sampleRate = sampleRate;
    if (!m_dev) return true;

#if HAS_LIBRTLSDR
    int r = rtlsdr_set_sample_rate(m_dev, sampleRate);
    return r == 0;
#else
    return true;
#endif
}

bool RtlSdrDevice::setGain(int gainTenthsDb) {
    m_config.gain = gainTenthsDb;
    if (!m_dev) return true;

#if HAS_LIBRTLSDR
    if (gainTenthsDb == 0) {
        rtlsdr_set_tuner_gain_mode(m_dev, 0); // AGC mode
    } else {
        rtlsdr_set_tuner_gain_mode(m_dev, 1); // Manual gain mode
        rtlsdr_set_tuner_gain(m_dev, gainTenthsDb);
    }
    return true;
#else
    return true;
#endif
}

bool RtlSdrDevice::setBiasTee(bool enable) {
    m_config.biasTee = enable;
    if (!m_dev) return true;

#if HAS_LIBRTLSDR
    #if defined(rtlsdr_set_bias_tee)
    rtlsdr_set_bias_tee(m_dev, enable ? 1 : 0);
    #endif
    return true;
#else
    return true;
#endif
}

void RtlSdrDevice::rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx) {
    auto* device = static_cast<RtlSdrDevice*>(ctx);
    if (device && device->m_running) {
        device->processRawBytes(buf, len);
    }
}

void RtlSdrDevice::processRawBytes(const uint8_t* buf, uint32_t len) {
    size_t numSamples = len / 2;
    if (m_conversionBuffer.size() < numSamples) {
        m_conversionBuffer.resize(numSamples);
    }

    // Convert unsigned 8-bit I/Q [0..255] to normalized float [-1.0..1.0]
    for (size_t i = 0; i < numSamples; ++i) {
        float iSample = (static_cast<float>(buf[2 * i]) - 127.5f) / 127.5f;
        float qSample = (static_cast<float>(buf[2 * i + 1]) - 127.5f) / 127.5f;
        m_conversionBuffer[i] = Complex32(iSample, qSample);
    }

    if (m_callback) {
        m_callback(m_conversionBuffer.data(), numSamples);
    }
}

bool RtlSdrDevice::startAsync(SampleBlockCallback callback) {
    if (m_running) return false;
    m_callback = std::move(callback);
    m_running = true;

    m_workerThread = std::thread(&RtlSdrDevice::workerThreadFunc, this);
    return true;
}

void RtlSdrDevice::stopAsync() {
    if (!m_running) return;
    m_running = false;

    if (m_dev) {
#if HAS_LIBRTLSDR
        rtlsdr_cancel_async(m_dev);
#endif
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void RtlSdrDevice::workerThreadFunc() {
    if (m_dev) {
#if HAS_LIBRTLSDR
        rtlsdr_read_async(m_dev, rtlsdrCallback, this, 0, 16384 * 16);
#endif
    } else {
        // Simulated sample stream loop
        std::vector<uint8_t> dummyBuffer(32768, 128);
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            processRawBytes(dummyBuffer.data(), dummyBuffer.size());
        }
    }
}

} // namespace RadiosondePI::SDR
