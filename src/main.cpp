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
#include "sdr/DiversityReceiver.hpp"
#include "dsp/FirFilter.hpp"
#include "dsp/FmDiscriminator.hpp"
#include "dsp/SymbolSync.hpp"
#include "dsp/Afc.hpp"
#include "dsp/DiversityCombiner.hpp"
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
#include "config/Config.hpp"

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

    std::string configPath = "config/config.example.json";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    RadiosondePI::Config::AppConfig appConfig;
    if (appConfig.loadFromFile(configPath)) {
        std::cout << "[CONFIG] Loaded configuration from: " << configPath << std::endl;
    } else if (appConfig.loadFromFile("/etc/radiosondepi/config.json")) {
        std::cout << "[CONFIG] Loaded configuration from /etc/radiosondepi/config.json" << std::endl;
    } else {
        std::cout << "[CONFIG] Using default configuration parameters." << std::endl;
    }

    // CLI overrides
    uint32_t frequencyHz = (appConfig.sdr.frequencyHz > 0) ? appConfig.sdr.frequencyHz : 403000000;
    bool autoScan = appConfig.scannerEnabled;
    bool diversityMode = appConfig.diversityEnabled;
    uint16_t webPort = (appConfig.web.port > 0) ? appConfig.web.port : 8080;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--freq" || arg == "-f") && i + 1 < argc) {
            double f = std::stod(argv[++i]);
            frequencyHz = (f < 1000.0) ? static_cast<uint32_t>(f * 1e6) : static_cast<uint32_t>(f);
            autoScan = false; // Explicit frequency overrides auto-scan
        } else if (arg == "--scan" || arg == "-s") {
            autoScan = true;
        } else if (arg == "--diversity" || arg == "-d") {
            diversityMode = true;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            webPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--gain" || arg == "-g") && i + 1 < argc) {
            std::string gStr = argv[++i];
            appConfig.sdr.gain = (gStr == "auto") ? 0 : static_cast<int>(std::stof(gStr) * 10.0f);
        }
    }

    std::cout << "[CONFIG] Tuned Frequency: " << std::fixed << std::setprecision(3) 
              << (frequencyHz / 1e6) << " MHz" 
              << " | AutoScan: " << (autoScan ? "ENABLED" : "DISABLED")
              << " | Gain: " << (appConfig.sdr.gain == 0 ? "AUTO" : std::to_string(appConfig.sdr.gain / 10.0f) + " dB")
              << std::endl;

    // 1. Initialize Uplinks & Storage
    RadiosondePI::Storage::FlightLogger flightLogger;
    RadiosondePI::Telemetry::LandingPredictor landingPredictor;

    RadiosondePI::Uplink::AeroHubUplink aeroHubUplink(appConfig.aeroHub);
    RadiosondePI::Uplink::SondeHubUplink sondeHubUplink(appConfig.station);
    RadiosondePI::Uplink::AprsGateway aprsGateway(appConfig.station.callsign + "-11");

    // 2. Initialize Embedded Web Dashboard
    appConfig.web.port = webPort;
    RadiosondePI::Web::WebDashboard webDashboard(appConfig.web);
    webDashboard.setCurrentFrequency(frequencyHz);
    webDashboard.start();

    // 3. Unified Telemetry Dispatcher Callback
    std::atomic<bool> sondeLocked{false};
    std::atomic<uint64_t> lastFrameTimeMs{0};

    auto onTelemetryReceived = [&](const RadiosondePI::Telemetry::TelemetryFrame& frame) {
        sondeLocked = true;
        auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        lastFrameTimeMs = nowMs;

        // Console Output
        std::cout << "\n>>> [" << RadiosondePI::Telemetry::sondeTypeToString(frame.type) 
                  << " LOCKED @ " << std::fixed << std::setprecision(3) << (frequencyHz / 1e6) << " MHz] Frame #" << frame.frameNumber 
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
        if (appConfig.aeroHub.enabled) {
            std::string aeroRecord = aeroHubUplink.formatAeroHubRecordJson(frame);
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

    RadiosondePI::Scanner::SpectrumScanner spectrumScanner(appConfig.scanner);

    // 6. Initialize RTL-SDR Device / Diversity Receiver
    RadiosondePI::SDR::DiversityConfig divConfig{};
    divConfig.enabled = diversityMode;
    divConfig.mode = appConfig.diversityMode;

    RadiosondePI::SDR::SdrConfig dev0 = appConfig.sdr;
    dev0.deviceIndex = 0;
    dev0.frequencyHz = frequencyHz;
    dev0.sampleRate = static_cast<uint32_t>(inputSampleRate);
    divConfig.dongleConfigs.push_back(dev0);

    if (diversityMode) {
        RadiosondePI::SDR::SdrConfig dev1 = appConfig.sdr;
        dev1.deviceIndex = appConfig.diversitySecondaryIndex;
        dev1.frequencyHz = frequencyHz;
        dev1.sampleRate = static_cast<uint32_t>(inputSampleRate);
        divConfig.dongleConfigs.push_back(dev1);
        std::cout << "[SDR] Dual-dongle diversity receiver enabled (Maximal Ratio Combining)." << std::endl;
    }

    RadiosondePI::SDR::DiversityReceiver sdrReceiver(divConfig);
    if (!sdrReceiver.open()) {
        std::cerr << "[ERROR] Could not open SDR receiver." << std::endl;
        return 1;
    }

    // Hook web dashboard tuning callback to re-tune hardware on the fly
    std::mutex freqMutex;
    webDashboard.setTuneCallback([&](uint32_t newFreqHz) {
        std::lock_guard<std::mutex> lock(freqMutex);
        frequencyHz = newFreqHz;
        sdrReceiver.setFrequency(newFreqHz);
        channelFilter.reset();
        afc.reset();
        rs41Decoder.reset();
        dfmDecoder.reset();
        m10Decoder.reset();
        std::cout << "[SDR] Retuned to: " << std::fixed << std::setprecision(3) << (newFreqHz / 1e6) << " MHz" << std::endl;
    });

    // Peak detection auto-tune callback
    spectrumScanner.setPeakCallback([&](const RadiosondePI::Scanner::PeakResult& peak) {
        if (autoScan && !sondeLocked) {
            std::lock_guard<std::mutex> lock(freqMutex);
            if (peak.frequencyHz != frequencyHz && std::abs(static_cast<int>(peak.frequencyHz - frequencyHz)) > 10000) {
                std::cout << "[SCANNER] Strong signal peak detected at " << std::fixed << std::setprecision(3) 
                          << (peak.frequencyHz / 1e6) << " MHz (SNR: " << std::setprecision(1) << peak.snrDb << " dB). Locking..." << std::endl;
                frequencyHz = peak.frequencyHz;
                sdrReceiver.setFrequency(peak.frequencyHz);
                webDashboard.setCurrentFrequency(peak.frequencyHz);
                channelFilter.reset();
                afc.reset();
                rs41Decoder.reset();
                dfmDecoder.reset();
                m10Decoder.reset();
            }
        }
    });

    // Zero-allocation pre-allocated DSP processing buffers
    constexpr size_t maxDecimatedBufferSize = 65536;
    std::vector<RadiosondePI::DSP::Complex32> decimatedBuffer(maxDecimatedBufferSize);
    std::vector<RadiosondePI::DSP::Complex32> afcRotatedBuffer(maxDecimatedBufferSize);
    std::vector<float> demodulatedAudioBuffer(maxDecimatedBufferSize);

    // Start SDR async ingestion with zero heap allocations per block
    sdrReceiver.start([&](const RadiosondePI::DSP::Complex32* samples, size_t count) {
        // Wideband Spectrum Scanner Hook
        if (autoScan && !sondeLocked) {
            spectrumScanner.analyzeBlock(samples, count, frequencyHz, inputSampleRate);
        }

        // 1. Zero-allocation Channel Decimation Filter
        size_t decimatedCount = channelFilter.processBlockDecimateZeroAlloc(
            samples, count, decimationFactor,
            decimatedBuffer.data(), decimatedBuffer.size()
        );

        if (decimatedCount == 0) return;

        // 2. Zero-allocation AFC Frequency Correction
        afc.rotateBlockZeroAlloc(decimatedBuffer.data(), decimatedCount, afcRotatedBuffer.data());

        // 3. Zero-allocation FM Demodulation
        fmDemod.processBlockZeroAlloc(afcRotatedBuffer.data(), decimatedCount, demodulatedAudioBuffer.data());

        // 4. Feed Audio Stream into Multi-Baud Symbol Synchronizers
        syncRS41.processBlock(demodulatedAudioBuffer.data(), decimatedCount);
        syncDFM.processBlock(demodulatedAudioBuffer.data(), decimatedCount);
        syncM10.processBlock(demodulatedAudioBuffer.data(), decimatedCount);
    });

    std::cout << "[DSP] Receiver pipeline running. Press Ctrl+C to terminate." << std::endl;

    // Background frequency sweeping thread for auto-scan mode when unlocked
    std::thread scanSweepThread([&]() {
        // Step frequencies across meteorological band: 400.5, 402.3, 404.1, 405.5 MHz
        const uint32_t sweepSteps[] = { 401000000, 402800000, 404800000, 405500000 };
        size_t stepIdx = 0;

        while (g_keepRunning) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (!autoScan) continue;

            auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            // Unlock if no frames received for > 8 seconds
            if (sondeLocked && (nowMs - lastFrameTimeMs > 8000)) {
                std::cout << "[SCANNER] Signal lost or burst detected. Resuming wideband auto-scan..." << std::endl;
                sondeLocked = false;
            }

            if (!sondeLocked) {
                // Hop to next sweep step
                uint32_t nextFreq = sweepSteps[stepIdx % (sizeof(sweepSteps)/sizeof(sweepSteps[0]))];
                stepIdx++;

                std::lock_guard<std::mutex> lock(freqMutex);
                frequencyHz = nextFreq;
                sdrReceiver.setFrequency(nextFreq);
                webDashboard.setCurrentFrequency(nextFreq);
                channelFilter.reset();
                afc.reset();
            }
        }
    });

    while (g_keepRunning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (scanSweepThread.joinable()) {
        scanSweepThread.join();
    }

    std::cout << "\n[SHUTDOWN] Stopping SDR acquisition..." << std::endl;
    sdrReceiver.stop();
    sdrReceiver.close();

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
