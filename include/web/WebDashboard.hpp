#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace RadiosondePI::Web {

struct WebServerConfig {
    uint16_t port{8080};
    std::string bindAddress{"0.0.0.0"};
    std::string webRoot{"web"};
};

class WebDashboard {
public:
    explicit WebDashboard(const WebServerConfig& config = WebServerConfig{})
        : m_config(config) {}

    void updateTelemetry(const Telemetry::TelemetryFrame& frame) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latestFrame = frame;
        m_history.push_back(frame);
        if (m_history.size() > 500) {
            m_history.erase(m_history.begin());
        }
    }

    [[nodiscard]] std::string getLatestTelemetryJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& f = m_latestFrame;

        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "{\n";
        json << "  \"type\": \"" << Telemetry::sondeTypeToString(f.type) << "\",\n";
        json << "  \"serial\": \"" << f.serialNumber << "\",\n";
        json << "  \"frame\": " << f.frameNumber << ",\n";
        json << "  \"freq_hz\": " << f.frequencyHz << ",\n";
        json << "  \"lat\": " << f.latitude << ",\n";
        json << "  \"lon\": " << f.longitude << ",\n";
        json << "  \"alt\": " << std::setprecision(1) << f.altitudeMeters << ",\n";
        json << "  \"climb\": " << std::setprecision(2) << f.climbRateMps << ",\n";
        json << "  \"speed\": " << std::setprecision(2) << f.speedMps * 3.6f << ",\n";
        json << "  \"heading\": " << std::setprecision(1) << f.headingDeg << ",\n";
        json << "  \"temp\": " << (f.temperatureC.has_value() ? std::to_string(*f.temperatureC) : "null") << ",\n";
        json << "  \"rh\": " << (f.relativeHumidityPercent.has_value() ? std::to_string(*f.relativeHumidityPercent) : "null") << ",\n";
        json << "  \"batt\": " << (f.batteryVoltageV.has_value() ? std::to_string(*f.batteryVoltageV) : "null") << ",\n";
        json << "  \"gps_valid\": " << (f.gpsValid ? "true" : "false") << "\n";
        json << "}";
        return json.str();
    }

    [[nodiscard]] std::string getFlightPathJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::stringstream json;
        json << std::fixed << std::setprecision(5);
        json << "[\n";
        for (size_t i = 0; i < m_history.size(); ++i) {
            const auto& pt = m_history[i];
            if (!pt.gpsValid) continue;
            json << "  {\"lat\": " << pt.latitude 
                 << ", \"lon\": " << pt.longitude 
                 << ", \"alt\": " << std::setprecision(1) << pt.altitudeMeters << "}";
            if (i + 1 < m_history.size()) json << ",";
            json << "\n";
        }
        json << "]";
        return json.str();
    }

private:
    WebServerConfig m_config;
    mutable std::mutex m_mutex;
    Telemetry::TelemetryFrame m_latestFrame;
    std::vector<Telemetry::TelemetryFrame> m_history;
};

} // namespace RadiosondePI::Web
