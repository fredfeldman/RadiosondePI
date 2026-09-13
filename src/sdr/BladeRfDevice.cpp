#include "sdr/BladeRfDevice.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <algorithm>

#if __has_include(<libbladeRF.h>)
#include <libbladeRF.h>
#define HAS_LIBBLADERF 1
#else
#define HAS_LIBBLADERF 0
struct bladerf {};
#endif

namespace RadiosondePI::SDR {

BladeRfDevice::BladeRfDevice(size_t ringBufferCapacity) {
    m_conversionBuffer.resize(65536);
}

BladeRfDevice::~BladeRfDevice() {
    stopAsync();
    close();
}

bool BladeRfDevice::open(const SdrConfig& config) {
    m_config = config;
    m_deviceName = "Nuand bladeRF (RX" + std::to_string(m_config.rxChannel + 1) + ")";

#if HAS_LIBBLADERF
    int status = 0;
    std::string deviceSpec = "";
    if (m_config.deviceIndex > 0) {
        deviceSpec = "*:instance=" + std::to_string(m_config.deviceIndex);
    }

    status = bladerf_open(&m_dev, deviceSpec.empty() ? nullptr : deviceSpec.c_str());
    if (status < 0) {
        std::cerr << "[bladeRF] Failed to open bladeRF device (" << bladerf_strerror(status) 
                  << "). Falling back to simulation mode." << std::endl;
        m_dev = nullptr;
        m_isSimulation = true;
        return true;
    }

    const char* fpgaName = bladerf_get_board_name(m_dev);
    m_deviceName = std::string(fpgaName ? fpgaName : "bladeRF") + " [RX" + std::to_string(m_config.rxChannel + 1) + "]";

    // Channel selection: BLADERF_CHANNEL_RX(0) or BLADERF_CHANNEL_RX(1)
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);

    // 1. Frequency
    uint64_t actualFreq = 0;
    status = bladerf_set_frequency(m_dev, rxCh, m_config.frequencyHz);
    if (status < 0) {
        std::cerr << "[bladeRF] Warning: Failed to set frequency: " << bladerf_strerror(status) << std::endl;
    }

    // 2. Sample Rate
    uint32_t actualRate = 0;
    status = bladerf_set_sample_rate(m_dev, rxCh, m_config.sampleRate, &actualRate);
    if (status < 0) {
        std::cerr << "[bladeRF] Warning: Failed to set sample rate: " << bladerf_strerror(status) << std::endl;
    }

    // 3. Bandwidth
    uint32_t actualBw = 0;
    bladerf_set_bandwidth(m_dev, rxCh, m_config.bandwidthHz, &actualBw);

    // 4. Gain / AGC
    if (m_config.gainMode == "fast") {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_FASTATTACK_AGC);
    } else if (m_config.gainMode == "slow") {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_SLOWATTACK_AGC);
    } else if (m_config.gainMode == "hybrid") {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_HYBRID_AGC);
    } else {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_MANUAL);
        int gainDb = (m_config.gain == 0) ? 30 : (m_config.gain / 10);
        bladerf_set_gain(m_dev, rxCh, gainDb);
    }

    // 5. Bias-Tee
    if (m_config.biasTee) {
        bladerf_set_bias_tee(m_dev, rxCh, true);
    }

    // 6. Configure sync synchronous RX streaming buffer
    constexpr unsigned int numBuffers = 16;
    constexpr unsigned int bufferSize = 8192; // 8K complex samples per buffer
    constexpr unsigned int numTransfers = 4;
    constexpr unsigned int streamTimeoutMs = 1000;

    status = bladerf_sync_config(m_dev,
                                 BLADERF_RX_X1,
                                 BLADERF_FORMAT_SC16_Q11,
                                 numBuffers,
                                 bufferSize,
                                 numTransfers,
                                 streamTimeoutMs);
    if (status < 0) {
        std::cerr << "[bladeRF] bladerf_sync_config failed: " << bladerf_strerror(status) << std::endl;
        bladerf_close(m_dev);
        m_dev = nullptr;
        m_isSimulation = true;
        return false;
    }

    std::cout << "[bladeRF] Successfully initialized " << m_deviceName 
              << " @ " << (m_config.frequencyHz / 1e6) << " MHz"
              << " (SampleRate: " << (m_config.sampleRate / 1e6) << " MSPS)" << std::endl;

    m_isSimulation = false;
    return true;
#else
    std::cout << "[bladeRF] libbladeRF headers not found during compilation. Initializing simulated bladeRF." << std::endl;
    m_isSimulation = true;
    return true;
#endif
}

void BladeRfDevice::close() {
    stopAsync();
#if HAS_LIBBLADERF
    if (m_dev) {
        bladerf_close(m_dev);
        m_dev = nullptr;
    }
#endif
    m_isSimulation = false;
}

bool BladeRfDevice::setFrequency(uint32_t frequencyHz) {
    m_config.frequencyHz = frequencyHz;
    if (!m_dev) return true;

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    int status = bladerf_set_frequency(m_dev, rxCh, frequencyHz);
    return status == 0;
#else
    return true;
#endif
}

bool BladeRfDevice::setSampleRate(uint32_t sampleRate) {
    m_config.sampleRate = sampleRate;
    if (!m_dev) return true;

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    uint32_t actual = 0;
    int status = bladerf_set_sample_rate(m_dev, rxCh, sampleRate, &actual);
    return status == 0;
#else
    return true;
#endif
}

bool BladeRfDevice::setGain(int gainTenthsDb) {
    m_config.gain = gainTenthsDb;
    if (!m_dev) return true;

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    if (gainTenthsDb == 0) {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_FASTATTACK_AGC);
    } else {
        bladerf_set_gain_mode(m_dev, rxCh, BLADERF_GAIN_MANUAL);
        bladerf_set_gain(m_dev, rxCh, gainTenthsDb / 10);
    }
    return true;
#else
    return true;
#endif
}

bool BladeRfDevice::setBiasTee(bool enable) {
    m_config.biasTee = enable;
    if (!m_dev) return true;

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    bladerf_set_bias_tee(m_dev, rxCh, enable);
    return true;
#else
    return true;
#endif
}

bool BladeRfDevice::setBandwidth(uint32_t bandwidthHz) {
    m_config.bandwidthHz = bandwidthHz;
    if (!m_dev) return true;

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    uint32_t actual = 0;
    return bladerf_set_bandwidth(m_dev, rxCh, bandwidthHz, &actual) == 0;
#else
    return true;
#endif
}

bool BladeRfDevice::setGainMode(const std::string& mode) {
    m_config.gainMode = mode;
    return true;
}

bool BladeRfDevice::setRxChannel(int channel) {
    m_config.rxChannel = channel;
    return true;
}

void BladeRfDevice::processSc16Q11Samples(const int16_t* interleavedIq, size_t numComplexSamples) {
    if (m_conversionBuffer.size() < numComplexSamples) {
        m_conversionBuffer.resize(numComplexSamples);
    }

    // bladeRF SC16_Q11 format: 12-bit ADC mapped to signed 16-bit integer [-2048, 2047]
    constexpr float norm = 1.0f / 2048.0f;
    for (size_t i = 0; i < numComplexSamples; ++i) {
        float iSample = static_cast<float>(interleavedIq[2 * i]) * norm;
        float qSample = static_cast<float>(interleavedIq[2 * i + 1]) * norm;
        m_conversionBuffer[i] = Complex32(iSample, qSample);
    }

    if (m_callback) {
        m_callback(m_conversionBuffer.data(), numComplexSamples);
    }
}

bool BladeRfDevice::startAsync(SampleBlockCallback callback) {
    if (m_running) return false;
    m_callback = std::move(callback);
    m_running = true;

    if (m_isSimulation) {
        m_workerThread = std::thread(&BladeRfDevice::simulationWorker, this);
        return true;
    }

#if HAS_LIBBLADERF
    bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
    int status = bladerf_enable_module(m_dev, rxCh, true);
    if (status < 0) {
        std::cerr << "[bladeRF] Failed to enable RX module: " << bladerf_strerror(status) << std::endl;
        m_running = false;
        return false;
    }

    m_workerThread = std::thread(&BladeRfDevice::streamWorker, this);
    return true;
#else
    m_workerThread = std::thread(&BladeRfDevice::simulationWorker, this);
    return true;
#endif
}

void BladeRfDevice::stopAsync() {
    if (!m_running) return;
    m_running = false;

#if HAS_LIBBLADERF
    if (m_dev && !m_isSimulation) {
        bladerf_channel rxCh = BLADERF_CHANNEL_RX(m_config.rxChannel);
        bladerf_enable_module(m_dev, rxCh, false);
    }
#endif

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void BladeRfDevice::streamWorker() {
#if HAS_LIBBLADERF
    constexpr size_t samplesPerBlock = 8192;
    // Each complex sample is 2 int16_t (I and Q)
    std::vector<int16_t> sampleBuffer(samplesPerBlock * 2);

    while (m_running && m_dev) {
        int status = bladerf_sync_rx(m_dev, sampleBuffer.data(), samplesPerBlock, nullptr, 1000);
        if (status == 0) {
            processSc16Q11Samples(sampleBuffer.data(), samplesPerBlock);
        } else if (status == BLADERF_ERR_TIMEOUT) {
            continue;
        } else {
            if (m_running) {
                std::cerr << "[bladeRF] bladerf_sync_rx error: " << bladerf_strerror(status) << std::endl;
            }
            break;
        }
    }
#endif
}

void BladeRfDevice::simulationWorker() {
    constexpr size_t samplesPerBlock = 4096;
    std::vector<int16_t> dummyBuffer(samplesPerBlock * 2, 0);

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        processSc16Q11Samples(dummyBuffer.data(), samplesPerBlock);
    }
}

} // namespace RadiosondePI::SDR
