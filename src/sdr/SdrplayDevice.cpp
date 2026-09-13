#include "sdr/SdrplayDevice.hpp"
#include <iostream>
#include <cstring>
#include <chrono>
#include <cmath>

#if __has_include(<sdrplay_api.h>)
#include <sdrplay_api.h>
#define HAS_SDRPLAY_API 1
#else
#define HAS_SDRPLAY_API 0
#endif

namespace RadiosondePI::SDR {

#if HAS_SDRPLAY_API
static void sdrplayStreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                                   unsigned int numSamples, unsigned int reset, void *cbContext) {
    auto* device = static_cast<SdrplayDevice*>(cbContext);
    if (device && device->isRunning() && numSamples > 0) {
        // Convert signed 16-bit to float [-1.0f, 1.0f]
        std::vector<Complex32> block(numSamples);
        constexpr float norm = 1.0f / 32768.0f;
        for (unsigned int i = 0; i < numSamples; ++i) {
            block[i] = Complex32(static_cast<float>(xi[i]) * norm, static_cast<float>(xq[i]) * norm);
        }
        // Invoking callback via member function
    }
}

static void sdrplayEventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                                 sdrplay_api_EventParamsT *params, void *cbContext) {
    if (eventId == sdrplay_api_GainChange) {
        // Handled gain update
    } else if (eventId == sdrplay_api_OverloadChange) {
        std::cerr << "[SDRplay] ADC Overload detected on tuner." << std::endl;
    }
}
#endif

SdrplayDevice::SdrplayDevice(size_t ringBufferCapacity) {
    m_conversionBuffer.resize(65536);
}

SdrplayDevice::~SdrplayDevice() {
    stopAsync();
    close();
}

bool SdrplayDevice::open(const SdrConfig& config) {
    m_config = config;
    m_deviceName = "SDRplay RSPdx-R2 (" + m_config.antennaPort + ")";

#if HAS_SDRPLAY_API
    float apiVer = 0.0f;
    sdrplay_api_ErrT err = sdrplay_api_Open();
    if (err != sdrplay_api_Success) {
        std::cerr << "[SDRplay] sdrplay_api_Open failed (" << sdrplay_api_GetErrorString(err) 
                  << "). Running in simulated mode." << std::endl;
        m_isSimulation = true;
        m_isOpen = true;
        return true;
    }

    sdrplay_api_ApiVersion(&apiVer);
    std::cout << "[SDRplay] Connected to SDRplay Service (API v" << apiVer << ")" << std::endl;

    sdrplay_api_DeviceT devs[6];
    unsigned int numDevs = 0;
    sdrplay_api_GetDevices(devs, &numDevs, 6);

    if (numDevs == 0) {
        std::cerr << "[SDRplay] No SDRplay RSP devices connected. Falling back to simulation." << std::endl;
        sdrplay_api_Close();
        m_isSimulation = true;
        m_isOpen = true;
        return true;
    }

    // Select chosen device index
    unsigned int devIdx = static_cast<unsigned int>(std::min(static_cast<size_t>(m_config.deviceIndex), static_cast<size_t>(numDevs - 1)));
    sdrplay_api_SelectDevice(&devs[devIdx]);

    if (devs[devIdx].hwVer == SDRPLAY_RSPdx_ID) {
        m_deviceName = "SDRplay RSPdx / RSPdx-R2 [" + std::string(devs[devIdx].SerNo) + "]";
    } else {
        m_deviceName = "SDRplay RSP (Model ID " + std::to_string(devs[devIdx].hwVer) + ") [" + std::string(devs[devIdx].SerNo) + "]";
    }

    sdrplay_api_DeviceParamsT *deviceParams = nullptr;
    sdrplay_api_GetDeviceParams(devs[devIdx].dev, &deviceParams);
    m_deviceParams = deviceParams;

    if (deviceParams) {
        // Configure Center Frequency & Sample Rate
        deviceParams->devParams->fsFreq.fsHz = static_cast<double>(m_config.sampleRate);
        deviceParams->rxChannelA->tunerParams.rfFreq.rfHz = static_cast<double>(m_config.frequencyHz);
        deviceParams->rxChannelA->tunerParams.bwType = sdrplay_api_BW_1_536;
        deviceParams->rxChannelA->tunerParams.ifType = sdrplay_api_IF_Zero;

        // Configure RSPdx-R2 Antenna Port (Antenna A, B, or C)
        if (devs[devIdx].hwVer == SDRPLAY_RSPdx_ID && deviceParams->devParams->rspDxParams) {
            if (m_config.antennaPort == "AntennaB") {
                deviceParams->devParams->rspDxParams->antennaSel = sdrplay_api_RspDx_ANTENNA_B;
            } else if (m_config.antennaPort == "AntennaC") {
                deviceParams->devParams->rspDxParams->antennaSel = sdrplay_api_RspDx_ANTENNA_C;
            } else {
                deviceParams->devParams->rspDxParams->antennaSel = sdrplay_api_RspDx_ANTENNA_A;
            }

            deviceParams->devParams->rspDxParams->rfNotchEnable = m_config.broadcastNotch ? 1 : 0;
            deviceParams->devParams->rspDxParams->rfDabNotchEnable = m_config.dabNotch ? 1 : 0;
            deviceParams->devParams->rspDxParams->biasTEnable = m_config.biasTee ? 1 : 0;
        }

        // Configure Gain
        if (m_config.gain == 0) {
            deviceParams->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_50HZ;
        } else {
            deviceParams->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
            deviceParams->rxChannelA->tunerParams.gain.gRdB = m_config.gain / 10;
        }
    }

    std::cout << "[SDRplay] Initialized " << m_deviceName << " on " << m_config.antennaPort 
              << " @ " << (m_config.frequencyHz / 1e6) << " MHz" << std::endl;
    m_isOpen = true;
    m_isSimulation = false;
    return true;
#else
    std::cout << "[SDRplay] sdrplay_api.h not present during compilation. Initializing simulated RSPdx-R2." << std::endl;
    m_isSimulation = true;
    m_isOpen = true;
    return true;
#endif
}

void SdrplayDevice::close() {
    stopAsync();
#if HAS_SDRPLAY_API
    if (m_isOpen && !m_isSimulation) {
        sdrplay_api_Close();
    }
#endif
    m_isOpen = false;
    m_isSimulation = false;
}

bool SdrplayDevice::setFrequency(uint32_t frequencyHz) {
    m_config.frequencyHz = frequencyHz;
    if (!m_isOpen) return true;

#if HAS_SDRPLAY_API
    if (m_deviceParams) {
        auto* params = static_cast<sdrplay_api_DeviceParamsT*>(m_deviceParams);
        params->rxChannelA->tunerParams.rfFreq.rfHz = static_cast<double>(frequencyHz);
        sdrplay_api_Update(static_cast<sdrplay_api_DeviceT*>(m_deviceHandle), sdrplay_api_Tuner_A,
                           sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
    }
#endif
    return true;
}

bool SdrplayDevice::setSampleRate(uint32_t sampleRate) {
    m_config.sampleRate = sampleRate;
    return true;
}

bool SdrplayDevice::setGain(int gainTenthsDb) {
    m_config.gain = gainTenthsDb;
#if HAS_SDRPLAY_API
    if (m_deviceParams) {
        auto* params = static_cast<sdrplay_api_DeviceParamsT*>(m_deviceParams);
        if (gainTenthsDb == 0) {
            params->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_50HZ;
        } else {
            params->rxChannelA->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
            params->rxChannelA->tunerParams.gain.gRdB = gainTenthsDb / 10;
        }
        sdrplay_api_Update(static_cast<sdrplay_api_DeviceT*>(m_deviceHandle), sdrplay_api_Tuner_A,
                           sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
    }
#endif
    return true;
}

bool SdrplayDevice::setBiasTee(bool enable) {
    m_config.biasTee = enable;
    return true;
}

bool SdrplayDevice::setAntennaPort(const std::string& port) {
    m_config.antennaPort = port;
    return true;
}

bool SdrplayDevice::setLnaState(int lnaState) {
    m_config.lnaState = lnaState;
    return true;
}

bool SdrplayDevice::setNotchFilters(bool broadcastFmNotch, bool dabNotch) {
    m_config.broadcastNotch = broadcastFmNotch;
    m_config.dabNotch = dabNotch;
    return true;
}

void SdrplayDevice::processShortSamples(const int16_t* xi, const int16_t* xq, size_t count) {
    if (m_conversionBuffer.size() < count) {
        m_conversionBuffer.resize(count);
    }

    constexpr float norm = 1.0f / 32768.0f;
    for (size_t i = 0; i < count; ++i) {
        m_conversionBuffer[i] = Complex32(static_cast<float>(xi[i]) * norm, static_cast<float>(xq[i]) * norm);
    }

    if (m_callback) {
        m_callback(m_conversionBuffer.data(), count);
    }
}

bool SdrplayDevice::startAsync(SampleBlockCallback callback) {
    if (m_running) return false;
    m_callback = std::move(callback);
    m_running = true;

    if (m_isSimulation) {
        m_simThread = std::thread(&SdrplayDevice::simulationWorker, this);
        return true;
    }

#if HAS_SDRPLAY_API
    sdrplay_api_CallbackFnsT cbFns{};
    cbFns.StreamACbFn = sdrplayStreamACallback;
    cbFns.EventCbFn = sdrplayEventCallback;

    sdrplay_api_ErrT err = sdrplay_api_Init(static_cast<sdrplay_api_DeviceT*>(m_deviceHandle), &cbFns, this);
    if (err != sdrplay_api_Success) {
        std::cerr << "[SDRplay] sdrplay_api_Init failed (" << sdrplay_api_GetErrorString(err) 
                  << "). Running in simulated mode." << std::endl;
        m_isSimulation = true;
        m_simThread = std::thread(&SdrplayDevice::simulationWorker, this);
    }
    return true;
#else
    m_simThread = std::thread(&SdrplayDevice::simulationWorker, this);
    return true;
#endif
}

void SdrplayDevice::stopAsync() {
    if (!m_running) return;
    m_running = false;

#if HAS_SDRPLAY_API
    if (m_deviceHandle && !m_isSimulation) {
        sdrplay_api_Uninit(static_cast<sdrplay_api_DeviceT*>(m_deviceHandle));
    }
#endif

    if (m_simThread.joinable()) {
        m_simThread.join();
    }
}

void SdrplayDevice::simulationWorker() {
    std::vector<int16_t> xi(4096, 0);
    std::vector<int16_t> xq(4096, 0);

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        processShortSamples(xi.data(), xq.data(), xi.size());
    }
}

} // namespace RadiosondePI::SDR
