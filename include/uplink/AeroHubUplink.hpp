#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace RadiosondePI::Uplink {

struct AeroHubConfig {
    bool enabled{false};
    std::string endpointUrl{"http://localhost:8088/api/v1/telemetry"};
    std::string apiKey{""};
    std::string stationId{"RadiosondePI-01"};
    bool streamGpsTrack{true};
    bool streamPtuSensors{true};
};

/**
 * @brief AeroHub telemetry aggregator client and JSON payload formatter.
 * Transmits standardized telemetry events, GPS tracks, and sensor readings to AeroHub aggregator instances.
 */
class AeroHubUplink {
public:
    explicit AeroHubUplink(const AeroHubConfig& config = AeroHubConfig{})
        : m_config(config) {}

    void setConfig(const AeroHubConfig& config) {
        m_config = config;
    }

    [[nodiscard]] const AeroHubConfig& getConfig() const {
        return m_config;
    }

    /**
     * @brief Formats a single telemetry frame matching AeroHub's RadioSondeImportRecord contract.
     * Compatible with AeroHub NDJSON import pipeline and direct REST ingestion.
     */
    [[nodiscard]] std::string formatAeroHubRecordJson(const Telemetry::TelemetryFrame& frame) const {
        if (!frame.gpsValid && frame.serialNumber.empty()) {
            return "{}";
        }

        auto timeT = std::chrono::system_clock::to_time_t(frame.timestamp);
        std::stringstream timeStr;
        timeStr << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%SZ");

        std::stringstream json;
        json << std::fixed << std::setprecision(6);
        json << "{\n";
        json << "  \"serial\": \"" << frame.serialNumber << "\",\n";
        json << "  \"sondeType\": \"" << Telemetry::sondeTypeToString(frame.type) << "\",\n";
        json << "  \"lat\": " << frame.latitude << ",\n";
        json << "  \"lon\": " << frame.longitude << ",\n";
        json << "  \"altitudeMeters\": " << std::setprecision(1) << frame.altitudeMeters << ",\n";
        json << "  \"ascentRateMetersPerSecond\": " << std::setprecision(2) << frame.climbRateMps << ",\n";
        json << "  \"temperatureCelsius\": " << (frame.temperatureC.has_value() ? std::to_string(*frame.temperatureC) : "null") << ",\n";
        json << "  \"humidityPercent\": " << (frame.relativeHumidityPercent.has_value() ? std::to_string(*frame.relativeHumidityPercent) : "null") << ",\n";
        json << "  \"pressureHpa\": " << (frame.pressureHpa.has_value() ? std::to_string(*frame.pressureHpa) : "null") << ",\n";
        json << "  \"frameSequence\": " << frame.frameNumber << ",\n";
        json << "  \"crcValid\": " << (frame.gpsValid ? "true" : "false") << ",\n";
        json << "  \"burstKill\": " << (frame.burstKillDetected ? "true" : "false") << ",\n";
        json << "  \"batteryVoltage\": " << (frame.batteryVoltageV.has_value() ? std::to_string(*frame.batteryVoltageV) : "null") << ",\n";
        json << "  \"frequencyMHz\": " << std::setprecision(4) << (frame.frequencyHz / 1e6) << ",\n";
        json << "  \"rssi\": " << std::setprecision(1) << frame.snrDb << ",\n";
        json << "  \"sourceApp\": \"RadiosondePI\",\n";
        json << "  \"sourceFormat\": \"application/x-ndjson; domain=radiosonde\",\n";
        json << "  \"sourceVersion\": \"0.1.0\",\n";
        json << "  \"originalTimestampUtc\": \"" << timeStr.str() << "\"\n";
        json << "}";

        return json.str();
    }

private:
    AeroHubConfig m_config;
};

} // namespace RadiosondePI::Uplink
