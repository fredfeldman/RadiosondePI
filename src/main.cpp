#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <fstream>
#include <sstream>

#include "sdr/RtlSdrDevice.hpp"
#include "dsp/FirFilter.hpp"
#include "dsp/FmDiscriminator.hpp"
#include "dsp/SymbolSync.hpp"
#include "dsp/Afc.hpp"
#include "scanner/SpectrumScanner.hpp"
#include "decoders/RS41Decoder.hpp"
#include "decoders/DFMDecoder.hpp"
#include "decoders/M10Decoder.hpp"
#include "telemetry/TelemetryData.hpp"
#include "telemetry/LandingPredictor.hpp"
#include "storage/FlightLogger.hpp"
#include "uplink/AeroHubUplink.hpp"
#include "uplink/SondeHubUplink.hpp"
#include "uplink/AprsGateway.hpp"
#include "web/WebDashboard.hpp"

static std::atomic<bool> g_keepRunning{true};

void signalHandler(int) {
    g_keepRunning = false;
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "=================================================" << std::endl;
    std::cout << " RadiosondePI - RTL-SDR Radiosonde Core (v0.1.0) " << std::endl;
    std::cout << " Multi-Protocol Receiver & Live AeroHub Pipeline  " << std::endl;
    std::cout << "=================================================" << std::endl;

    uint32_t frequencyHz = 403000000;
    std::string configPath = "config/config.example.json";
    bool autoScan = false;
    uint16_t webPort = 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--freq" || arg == "-f") && i + 1 < argc) {
            frequencyHz = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--scan" || arg == "-s") {
            autoScan = true;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            webPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        }
    }

    std::cout << "[CONFIG] Target Frequency: " << std::fixed << std::setprecision(3) 
              << (frequencyHz / 1e6) << " MHz" << std::endl;

    // 1. Initialize Uplinks & Storage
    RadiosondePI::Storage::FlightLogger flightLogger;
    RadiosondePI::Telemetry::LandingPredictor landingPredictor;

    RadiosondePI::Uplink::AeroHubConfig aeroHubConfig{};
    aeroHubConfig.enabled = true;
    aeroHubConfig.stationId = "RadiosondePI-01";
    RadiosondePI::Uplink::AeroHubUplink aeroHubUplink(aeroHubConfig);

    RadiosondePI::Uplink::StationConfig stationConfig{};
    stationConfig.callsign = "N0CALL";
    RadiosondePI::Uplink::SondeHubUplink sondeHubUplink(stationConfig);
    RadiosondePI::Uplink::AprsGateway aprsGateway("N0CALL-11");

    // 2. Initialize Embedded Web Dashboard
    RadiosondePI::Web::WebServerConfig webConfig{};
    webConfig.port = webPort;
    webConfig.bindAddress = "0.0.0.0";
    webConfig.webRoot = "web";

    RadiosondePI::Web::WebDashboard webDashboard(webConfig);
    webDashboard.start();

    // 3. Unified Telemetry Dispatcher Callback
    auto onTelemetryReceived = [&](const RadiosondePI::Telemetry::TelemetryFrame& frame) {
        // Console Output
        std::cout << "\n>>> [" << RadiosondePI::Telemetry::sondeTypeToString(frame.type) 
                  << " DETECTED] Frame #" << frame.frameNumber 
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

        // Update Web Dashboard & In-Memory Flight Path
        webDashboard.updateTelemetry(frame);

        // Update Landing Trajectory
        landingPredictor.addPoint(frame);

        // Store into Local Flight Logger
        flightLogger.logFrame(frame);

        // AeroHub Aggregator JSON event
        if (aeroHubConfig.enabled) {
            std::string aeroRecord = aeroHubUplink.formatAeroHubRecordJson(frame);
            // In live mode, transmitted via HTTP POST or socket to AeroHub instance
        }
    };

    // 4. Initialize Multi-Protocol Decoders
    RadiosondePI::Decoders::RS41Decoder rs41Decoder;
    rs41Decoder.setFrameCallback(onTelemetryReceived);

    RadiosondePI::Decoders::DFMDecoder dfmDecoder;
    dfmDecoder.setFrameCallback(onTelemetryReceived);

    RadiosondePI::Decoders::M10Decoder m10Decoder;
    m10Decoder.setFrameCallback(onTelemetryReceived);

    // 5. Initialize DSP Pipeline
    constexpr float inputSampleRate = 2400000.0f;
    constexpr float channelSampleRate = 48000.0f;
    constexpr size_t decimationFactor = static_cast<size_t>(inputSampleRate / channelSampleRate); // 50

    RadiosondePI::DSP::FirFilter channelFilter;
    channelFilter.initLowPass(63, inputSampleRate, 15000.0f); // 15 kHz low-pass cutoff

    RadiosondePI::DSP::FmDiscriminator fmDemod;
    RadiosondePI::DSP::Afc afc;
    afc.init(channelSampleRate, 0.01f);

    RadiosondePI::DSP::SymbolSync syncRS41;
    syncRS41.init(channelSampleRate, 4800.0f, 0.05f);
    syncRS41.setBitCallback([&](uint8_t bit) { rs41Decoder.processBit(bit); });

    RadiosondePI::DSP::SymbolSync syncDFM;
    syncDFM.init(channelSampleRate, 2400.0f, 0.05f);
    syncDFM.setBitCallback([&](uint8_t bit) { dfmDecoder.processBit(bit); });

    RadiosondePI::DSP::SymbolSync syncM10;
    syncM10.init(channelSampleRate, 9600.0f, 0.05f);
    syncM10.setBitCallback([&](uint8_t bit) { m10Decoder.processBit(bit); });

    RadiosondePI::Scanner::ScannerConfig scannerConfig{};
    RadiosondePI::Scanner::SpectrumScanner spectrumScanner(scannerConfig);

    // 6. Initialize RTL-SDR Device
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
    std::vector<RadiosondePI::DSP::Complex32> afcRotatedSamples;
    std::vector<float> demodulatedAudio;

    // Start SDR async ingestion
    sdr.startAsync([&](const RadiosondePI::DSP::Complex32* samples, size_t count) {
        // Optional Wideband Spectrum Scanner Hook
        if (autoScan) {
            spectrumScanner.analyzeBlock(samples, count, frequencyHz, inputSampleRate);
        }

        // Channel Decimation Filter
        channelFilter.processBlockDecimate(samples, count, decimationFactor, decimatedSamples);

        // AFC Frequency Correction
        afc.rotateBlock(decimatedSamples.data(), decimatedSamples.size(), afcRotatedSamples);

        // FM Demodulation
        fmDemod.processBlock(afcRotatedSamples.data(), afcRotatedSamples.size(), demodulatedAudio);

        // Feed Audio Stream into Multi-Baud Symbol Synchronizers
        syncRS41.processBlock(demodulatedAudio.data(), demodulatedAudio.size());
        syncDFM.processBlock(demodulatedAudio.data(), demodulatedAudio.size());
        syncM10.processBlock(demodulatedAudio.data(), demodulatedAudio.size());
    });

    std::cout << "[DSP] Multi-protocol pipeline running. Press Ctrl+C to terminate." << std::endl;

    while (g_keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    std::cout << "\n[SHUTDOWN] Stopping SDR acquisition..." << std::endl;
    sdr.stopAsync();
    sdr.close();

    std::cout << "[SHUTDOWN] Stopping Web Dashboard..." << std::endl;
    webDashboard.stop();

    if (flightLogger.getRecordCount() > 0) {
        std::cout << "[STORAGE] Exporting flight session (" << flightLogger.getRecordCount() << " records)..." << std::endl;
        std::ofstream csvFile("flight_session.csv");
        if (csvFile.is_open()) {
            csvFile << flightLogger.exportCsv();
            std::cout << "[STORAGE] Saved flight_session.csv" << std::endl;
        }
    }

    std::cout << "[SHUTDOWN] Clean exit completed." << std::endl;
    return 0;
}
