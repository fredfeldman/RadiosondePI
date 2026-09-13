#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <functional>
#include <sstream>
#include <iomanip>

namespace RadiosondePI::Uplink {

struct StationConfig {
    std::string callsign{"N0CALL"};
    std::string antenna{"403MHz Turnstile"};
    double stationLat{0.0};
    double stationLon{0.0};
    double stationAltMeters{0.0};
    std::string email{""};
};

class SondeHubUplink {
public:
    explicit SondeHubUplink(const StationConfig& config = StationConfig{})
        : m_config(config) {}

    void setConfig(const StationConfig& config) {
        m_config = config;
    }

    [[nodiscard]] std::string formatTelemetryJson(const Telemetry::TelemetryFrame& frame) const {
        if (!frame.gpsValid || frame.serialNumber.empty()) {
            return "{}";
        }

        auto timeT = std::chrono::system_clock::to_time_t(frame.timestamp);
        std::stringstream timeStr;
        timeStr << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%SZ");

        std::stringstream json;
        json << std::fixed << std::setprecision(6);
        json << "{\n";
        json << "  \"software_name\": \"RadiosondePI\",\n";
        json << "  \"software_version\": \"0.1.0\",\n";
        json << "  \"uploader_callsign\": \"" << m_config.callsign << "\",\n";
        json << "  \"uploader_antenna\": \"" << m_config.antenna << "\",\n";
        if (m_config.stationLat != 0.0 || m_config.stationLon != 0.0) {
            json << "  \"uploader_position\": [" << m_config.stationLat << ", " << m_config.stationLon << ", " << m_config.stationAltMeters << "],\n";
        }
        json << "  \"payload_callsign\": \"" << frame.serialNumber << "\",\n";
        json << "  \"time_received\": \"" << timeStr.str() << "\",\n";
        json << "  \"datetime\": \"" << timeStr.str() << "\",\n";
        json << "  \"frame\": " << frame.frameNumber << ",\n";
        json << "  \"type\": \"" << Telemetry::sondeTypeToString(frame.type) << "\",\n";
        json << "  \"lat\": " << frame.latitude << ",\n";
        json << "  \"lon\": " << frame.longitude << ",\n";
        json << "  \"alt\": " << std::setprecision(1) << frame.altitudeMeters << ",\n";
        json << "  \"vel_h\": " << std::setprecision(2) << frame.speedMps << ",\n";
        json << "  \"vel_v\": " << std::setprecision(2) << frame.climbRateMps << ",\n";
        json << "  \"heading\": " << std::setprecision(1) << frame.headingDeg << ",\n";
        json << "  \"sats\": " << static_cast<int>(frame.satellitesVisible);

        if (frame.temperatureC.has_value()) {
            json << ",\n  \"temp\": " << std::setprecision(2) << *frame.temperatureC;
        }
        if (frame.relativeHumidityPercent.has_value()) {
            json << ",\n  \"humidity\": " << std::setprecision(1) << *frame.relativeHumidityPercent;
        }
        if (frame.pressureHpa.has_value()) {
            json << ",\n  \"pressure\": " << std::setprecision(1) << *frame.pressureHpa;
        }
        if (frame.batteryVoltageV.has_value()) {
            json << ",\n  \"batt\": " << std::setprecision(2) << *frame.batteryVoltageV;
        }
        if (frame.frequencyHz > 0) {
            json << ",\n  \"frequency\": " << frame.frequencyHz;
        }

        json << "\n}";
        return json.str();
    }

private:
    StationConfig m_config;
};

} // namespace RadiosondePI::Uplink
