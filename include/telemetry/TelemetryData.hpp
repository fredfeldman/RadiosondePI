#pragma once

#include <string>
#include <cstdint>
#include <chrono>
#include <optional>

namespace RadiosondePI::Telemetry {

enum class SondeType {
    Unknown = 0,
    RS41,
    DFM06,
    DFM09,
    DFM17,
    M10,
    M20,
    LMS6
};

inline std::string sondeTypeToString(SondeType type) {
    switch (type) {
        case SondeType::RS41:  return "RS41";
        case SondeType::DFM06: return "DFM06";
        case SondeType::DFM09: return "DFM09";
        case SondeType::DFM17: return "DFM17";
        case SondeType::M10:   return "M10";
        case SondeType::M20:   return "M20";
        case SondeType::LMS6:  return "LMS6";
        default:               return "UNKNOWN";
    }
}

struct TelemetryFrame {
    SondeType type{SondeType::Unknown};
    std::string serialNumber;
    uint32_t frameNumber{0};
    std::chrono::system_clock::time_point timestamp;

    uint32_t frequencyHz{0};
    float snrDb{0.0f};

    // GPS Position
    double latitude{0.0};
    double longitude{0.0};
    double altitudeMeters{0.0};
    float speedMps{0.0f};
    float climbRateMps{0.0f};
    float headingDeg{0.0f};
    uint8_t satellitesVisible{0};

    // PTU (Pressure, Temperature, Humidity) Sensors
    std::optional<float> temperatureC;
    std::optional<float> relativeHumidityPercent;
    std::optional<float> pressureHpa;
    std::optional<float> batteryVoltageV;
    std::optional<float> internalTempC;

    // Status / Health
    bool gpsValid{false};
    bool eccCorrected{false};
    uint16_t eccErrorsCorrected{0};
    bool burstKillDetected{false};
};

} // namespace RadiosondePI::Telemetry
