#pragma once

#include "sdr/RtlSdrDevice.hpp"
#include "dsp/DiversityCombiner.hpp"
#include "scanner/SpectrumScanner.hpp"
#include "uplink/AeroHubUplink.hpp"
#include "uplink/SondeHubUplink.hpp"
#include "web/WebDashboard.hpp"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace RadiosondePI::Config {

struct AppConfig {
    SDR::SdrConfig sdr{};
    bool diversityEnabled{false};
    DSP::DiversityMode diversityMode{DSP::DiversityMode::MaximalRatioCombining};
    int diversitySecondaryIndex{1};

    Scanner::ScannerConfig scanner{};
    bool scannerEnabled{true};

    Uplink::StationConfig station{};
    Web::WebServerConfig web{};
    Uplink::AeroHubConfig aeroHub{};
    bool sondeHubEnabled{false};
    bool aprsEnabled{false};
    std::string aprsServer{"rotate.aprs2.net"};
    uint16_t aprsPort{14580};
    int aprsPasscode{-1};

    static std::string extractString(const std::string& json, const std::string& key, const std::string& defVal = "") {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return defVal;

        size_t colon = json.find(':', pos + pattern.size());
        if (colon == std::string::npos) return defVal;

        size_t firstQuote = json.find('\"', colon);
        if (firstQuote == std::string::npos) return defVal;

        size_t secondQuote = json.find('\"', firstQuote + 1);
        if (secondQuote == std::string::npos) return defVal;

        return json.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    }

    static double extractDouble(const std::string& json, const std::string& key, double defVal = 0.0) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return defVal;

        size_t colon = json.find(':', pos + pattern.size());
        if (colon == std::string::npos) return defVal;

        size_t start = json.find_first_of("0123456789-.", colon);
        if (start == std::string::npos) return defVal;

        size_t end = json.find_first_not_of("0123456789-.eE", start);
        std::string valStr = json.substr(start, end - start);
        try {
            return std::stod(valStr);
        } catch (...) {
            return defVal;
        }
    }

    static int extractInt(const std::string& json, const std::string& key, int defVal = 0) {
        return static_cast<int>(extractDouble(json, key, defVal));
    }

    static bool extractBool(const std::string& json, const std::string& key, bool defVal = false) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return defVal;

        size_t colon = json.find(':', pos + pattern.size());
        if (colon == std::string::npos) return defVal;

        size_t valPos = json.find_first_of("tfTF", colon);
        if (valPos == std::string::npos) return defVal;

        return (json[valPos] == 't' || json[valPos] == 'T');
    }

    bool loadFromFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        // 1. SDR Section
        sdr.deviceIndex = extractInt(json, "device_index", 0);
        sdr.sampleRate = static_cast<uint32_t>(extractDouble(json, "sample_rate", 2400000));
        sdr.ppmCorrection = extractInt(json, "ppm_error", 0);
        sdr.biasTee = extractBool(json, "bias_tee", false);

        double freq = extractDouble(json, "frequency_hz", 0.0);
        if (freq > 1e6) {
            sdr.frequencyHz = static_cast<uint32_t>(freq);
        }

        std::string gainStr = extractString(json, "gain", "auto");
        if (gainStr == "auto" || gainStr.empty()) {
            sdr.gain = 0;
        } else {
            try {
                // gain in tenths of dB (e.g. 42.1 dB -> 421)
                float g = std::stof(gainStr);
                sdr.gain = static_cast<int>(g * 10.0f);
            } catch (...) {
                sdr.gain = 0;
            }
        }

        // 2. Diversity Section
        diversityEnabled = extractBool(json, "diversity", false);
        diversitySecondaryIndex = extractInt(json, "secondary_device_index", 1);

        // 3. Scanner Section
        scannerEnabled = extractBool(json, "enabled", true);
        double minFreq = extractDouble(json, "min_frequency_hz", 400050000);
        double maxFreq = extractDouble(json, "max_frequency_hz", 406000000);
        scanner.minFrequencyHz = static_cast<uint32_t>(minFreq);
        scanner.maxFrequencyHz = static_cast<uint32_t>(maxFreq);
        scanner.stepHz = static_cast<uint32_t>(extractDouble(json, "step_hz", 1800000));
        scanner.detectionThresholdDb = static_cast<float>(extractDouble(json, "detection_threshold_db", 8.0));
        scanner.dwellTimeMs = static_cast<uint32_t>(extractDouble(json, "dwell_time_ms", 250));

        // 4. Station Section
        station.callsign = extractString(json, "callsign", "N0CALL");
        station.stationLat = extractDouble(json, "latitude", 0.0);
        station.stationLon = extractDouble(json, "longitude", 0.0);
        station.stationAltMeters = extractDouble(json, "altitude_m", 0.0);
        station.antenna = extractString(json, "antenna", "Turnstile 403MHz");

        // 5. Web Section
        web.port = static_cast<uint16_t>(extractInt(json, "port", 8080));
        web.bindAddress = extractString(json, "bind_address", "0.0.0.0");
        web.webRoot = "web";

        // 6. Uplink Section
        aeroHub.enabled = extractBool(json, "aerohub", true);
        aeroHub.endpointUrl = extractString(json, "endpoint_url", "http://localhost:8088/api/v1/telemetry");
        aeroHub.apiKey = extractString(json, "api_key", "");
        aeroHub.stationId = extractString(json, "station_id", station.callsign);

        sondeHubEnabled = extractBool(json, "sondehub", false);
        aprsEnabled = extractBool(json, "aprs_is", false);

        return true;
    }
};

} // namespace RadiosondePI::Config
