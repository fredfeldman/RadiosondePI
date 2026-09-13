#pragma once

#include "telemetry/TelemetryData.hpp"
#include <string>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace RadiosondePI::Uplink {

class AprsGateway {
public:
    explicit AprsGateway(std::string callsign = "N0CALL-11")
        : m_myCall(std::move(callsign)) {}

    void setCallsign(std::string callsign) {
        m_myCall = std::move(callsign);
    }

    [[nodiscard]] std::string formatAprsPacket(const Telemetry::TelemetryFrame& frame) const {
        if (!frame.gpsValid || frame.serialNumber.empty()) {
            return "";
        }

        // Format APRS coordinates: DDMM.hhN/DDDMM.hhW
        double lat = frame.latitude;
        char latHemisphere = (lat >= 0.0) ? 'N' : 'S';
        lat = std::abs(lat);
        int latDeg = static_cast<int>(lat);
        double latMin = (lat - latDeg) * 60.0;

        double lon = frame.longitude;
        char lonHemisphere = (lon >= 0.0) ? 'E' : 'W';
        lon = std::abs(lon);
        int lonDeg = static_cast<int>(lon);
        double lonMin = (lon - lonDeg) * 60.0;

        std::stringstream ss;
        // APRS Object format: MYCALL>APRS,TCPIP*:)OBJECT_NAME*DDHHMMzDDMM.hhN/DDDMM.hhW/A=AAAAAA comments
        ss << m_myCall << ">APRS,TCPIP*:;" 
           << std::setw(9) << std::left << frame.serialNumber << "*";

        // Timestamp in DDHHMMz
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        std::tm gm{};
#if defined(_WIN32)
        gmtime_s(&gm, &timeT);
#else
        gmtime_r(&timeT, &gm);
#endif
        ss << std::setfill('0') << std::setw(2) << gm.tm_mday
           << std::setw(2) << gm.tm_hour
           << std::setw(2) << gm.tm_min << "z";

        // Latitude DDMM.hhN
        ss << std::setw(2) << latDeg << std::fixed << std::setprecision(2) << std::setw(5) << latMin << latHemisphere;
        // APRS Symbol: /O (Balloon) or /'
        ss << "/";
        // Longitude DDDMM.hhW
        ss << std::setw(3) << lonDeg << std::fixed << std::setprecision(2) << std::setw(5) << lonMin << lonHemisphere;
        ss << "O"; // Balloon symbol

        // Course / Speed / Altitude: CSE/SPD/A=altitude_in_feet
        int course = static_cast<int>(frame.headingDeg) % 360;
        int speedKnots = static_cast<int>(frame.speedMps * 1.94384f);
        int altFeet = static_cast<int>(frame.altitudeMeters * 3.28084);

        ss << std::setw(3) << course << "/" << std::setw(3) << speedKnots << "/A=" << std::setw(6) << altFeet;

        // Telemetry comment
        ss << " " << Telemetry::sondeTypeToString(frame.type) 
           << " Clmb=" << std::fixed << std::setprecision(1) << frame.climbRateMps << "m/s";
        if (frame.temperatureC.has_value()) {
            ss << " T=" << std::setprecision(1) << *frame.temperatureC << "C";
        }
        if (frame.relativeHumidityPercent.has_value()) {
            ss << " RH=" << std::setprecision(0) << *frame.relativeHumidityPercent << "%";
        }
        if (frame.frequencyHz > 0) {
            ss << " Freq=" << std::setprecision(3) << (frame.frequencyHz / 1e6) << "MHz";
        }

        return ss.str();
    }

private:
    std::string m_myCall;
};

} // namespace RadiosondePI::Uplink
