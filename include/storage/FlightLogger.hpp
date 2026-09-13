#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace RadiosondePI::Storage {

class FlightLogger {
public:
    FlightLogger() = default;

    void logFrame(const Telemetry::TelemetryFrame& frame) {
        if (!frame.gpsValid || frame.serialNumber.empty()) return;
        m_frames.push_back(frame);
    }

    [[nodiscard]] size_t getRecordCount() const {
        return m_frames.size();
    }

    [[nodiscard]] std::string exportCsv() const {
        std::stringstream ss;
        ss << "timestamp,type,serial,frame,lat,lon,alt_m,climb_mps,speed_kmh,heading_deg,temp_c,rh_pct,batt_v\n";

        for (const auto& f : m_frames) {
            auto timeT = std::chrono::system_clock::to_time_t(f.timestamp);
            std::tm gm{};
#if defined(_WIN32)
            gmtime_s(&gm, &timeT);
#else
            gmtime_r(&timeT, &gm);
#endif
            ss << std::put_time(&gm, "%Y-%m-%dT%H:%M:%SZ") << ","
               << Telemetry::sondeTypeToString(f.type) << ","
               << f.serialNumber << ","
               << f.frameNumber << ","
               << std::fixed << std::setprecision(5)
               << f.latitude << ","
               << f.longitude << ","
               << std::setprecision(1) << f.altitudeMeters << ","
               << std::setprecision(2) << f.climbRateMps << ","
               << std::setprecision(2) << f.speedMps * 3.6f << ","
               << std::setprecision(1) << f.headingDeg << ",";

            if (f.temperatureC.has_value()) ss << std::setprecision(2) << *f.temperatureC;
            ss << ",";
            if (f.relativeHumidityPercent.has_value()) ss << std::setprecision(1) << *f.relativeHumidityPercent;
            ss << ",";
            if (f.batteryVoltageV.has_value()) ss << std::setprecision(2) << *f.batteryVoltageV;
            ss << "\n";
        }

        return ss.str();
    }

    [[nodiscard]] std::string exportKml(const std::string& flightName = "Radiosonde Flight") const {
        std::stringstream ss;
        ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        ss << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
        ss << "  <Document>\n";
        ss << "    <name>" << flightName << "</name>\n";
        ss << "    <Placemark>\n";
        ss << "      <name>Flight Trajectory</name>\n";
        ss << "      <LineString>\n";
        ss << "        <extrude>1</extrude>\n";
        ss << "        <tessellate>1</tessellate>\n";
        ss << "        <altitudeMode>absolute</altitudeMode>\n";
        ss << "        <coordinates>\n";

        for (const auto& f : m_frames) {
            ss << std::fixed << std::setprecision(6)
               << "          " << f.longitude << "," << f.latitude << "," << std::setprecision(1) << f.altitudeMeters << "\n";
        }

        ss << "        </coordinates>\n";
        ss << "      </LineString>\n";
        ss << "    </Placemark>\n";
        ss << "  </Document>\n";
        ss << "</kml>\n";

        return ss.str();
    }

    void clear() {
        m_frames.clear();
    }

private:
    std::vector<Telemetry::TelemetryFrame> m_frames;
};

} // namespace RadiosondePI::Storage
