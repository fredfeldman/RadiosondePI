#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

#include "sdr/RtlSdrDevice.hpp"
#include "dsp/FirFilter.hpp"
#include "dsp/FmDiscriminator.hpp"
#include "dsp/SymbolSync.hpp"
#include "decoders/RS41Decoder.hpp"
#include "telemetry/TelemetryData.hpp"

static std::atomic<bool> g_keepRunning{true};

void signalHandler(int) {
    g_keepRunning = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "=================================================" << std::endl;
    std::cout << " RadiosondePI - RTL-SDR Radiosonde Core (v0.1.0) " << std::endl;
    std::cout << " Sprint 1: DSP Demodulation & Vaisala RS41       " << std::endl;
    std::cout << "=================================================" << std::endl;

    uint32_t frequencyHz = 403000000;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--freq" || arg == "-f") && i + 1 < argc) {
            frequencyHz = static_cast<uint32_t>(std::stoul(argv[++i]));
        }
    }

    std::cout << "[CONFIG] Target Frequency: " << std::fixed << std::setprecision(3) 
              << (frequencyHz / 1e6) << " MHz" << std::endl;

    // Initialize DSP blocks
    constexpr float inputSampleRate = 2400000.0f;
    constexpr float channelSampleRate = 48000.0f;
    constexpr size_t decimationFactor = static_cast<size_t>(inputSampleRate / channelSampleRate); // 50
    constexpr float rs41BaudRate = 4800.0f;

    RadiosondePI::DSP::FirFilter channelFilter;
    channelFilter.initLowPass(63, inputSampleRate, 15000.0f); // 15 kHz low-pass cutoff

    RadiosondePI::DSP::FmDiscriminator fmDemod;

    RadiosondePI::DSP::SymbolSync symbolSync;
    symbolSync.init(channelSampleRate, rs41BaudRate, 0.05f);

    RadiosondePI::Decoders::RS41Decoder rs41Decoder;

    // RS41 Decoded Telemetry Callback
    rs41Decoder.setFrameCallback([](const RadiosondePI::Telemetry::TelemetryFrame& frame) {
        std::cout << "\n>>> [RS41 DETECTED] Frame #" << frame.frameNumber 
                  << " | Serial: " << frame.serialNumber << " <<<\n";
        if (frame.gpsValid) {
            std::cout << "    Position: " << std::fixed << std::setprecision(5)
                      << frame.latitude << ", " << frame.longitude 
                      << " | Alt: " << std::setprecision(1) << frame.altitudeMeters << " m"
                      << " | Climb: " << frame.climbRateMps << " m/s"
                      << " | Speed: " << frame.speedMps * 3.6f << " km/h"
                      << " | Heading: " << frame.headingDeg << " deg\n";
        }
        if (frame.temperatureC.has_value()) {
            std::cout << "    Temp: " << std::setprecision(2) << *frame.temperatureC << " C";
        }
        if (frame.relativeHumidityPercent.has_value()) {
            std::cout << " | RH: " << std::setprecision(1) << *frame.relativeHumidityPercent << " %";
        }
        if (frame.batteryVoltageV.has_value()) {
            std::cout << " | Battery: " << std::setprecision(2) << *frame.batteryVoltageV << " V";
        }
        std::cout << std::endl;
    });

    symbolSync.setBitCallback([&rs41Decoder](uint8_t bit) {
        rs41Decoder.processBit(bit);
    });

    // Initialize RTL-SDR Device
    RadiosondePI::SDR::RtlSdrDevice sdr;
    RadiosondePI::SDR::SdrConfig sdrConfig{};
    sdrConfig.deviceIndex = 0;
    sdrConfig.frequencyHz = frequencyHz;
    sdrConfig.sampleRate = static_cast<uint32_t>(inputSampleRate);
    sdrConfig.gain = 0; // Auto gain

    if (!sdr.open(sdrConfig)) {
        std::cerr << "[ERROR] Could not open SDR device." << std::endl;
        return 1;
    }

    std::vector<RadiosondePI::DSP::Complex32> decimatedSamples;
    std::vector<float> demodulatedAudio;

    // Start SDR async ingestion
    sdr.startAsync([&](const RadiosondePI::DSP::Complex32* samples, size_t count) {
        channelFilter.processBlockDecimate(samples, count, decimationFactor, decimatedSamples);
        fmDemod.processBlock(decimatedSamples.data(), decimatedSamples.size(), demodulatedAudio);
        symbolSync.processBlock(demodulatedAudio.data(), demodulatedAudio.size());
    });

    std::cout << "[DSP] Pipeline running. Press Ctrl+C to terminate." << std::endl;

    while (g_keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[SHUTDOWN] Stopping SDR acquisition..." << std::endl;
    sdr.stopAsync();
    sdr.close();

    std::cout << "[SHUTDOWN] Clean exit completed." << std::endl;
    return 0;
}
