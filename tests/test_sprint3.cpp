#include <cassert>
#include <iostream>
#include <string>

#include "uplink/SondeHubUplink.hpp"
#include "uplink/AprsGateway.hpp"
#include "telemetry/LandingPredictor.hpp"
#include "web/WebDashboard.hpp"

void testSondeHubPayloadFormatting() {
    RadiosondePI::Uplink::StationConfig cfg{};
    cfg.callsign = "W1AW";
    cfg.antenna = "Yagi 403MHz";
    cfg.stationLat = 41.7148;
    cfg.stationLon = -72.7272;
    cfg.stationAltMeters = 50.0;

    RadiosondePI::Uplink::SondeHubUplink uplink(cfg);

    RadiosondePI::Telemetry::TelemetryFrame frame{};
    frame.type = RadiosondePI::Telemetry::SondeType::RS41;
    frame.serialNumber = "V1234567";
    frame.frameNumber = 100;
    frame.timestamp = std::chrono::system_clock::now();
    frame.latitude = 42.0;
    frame.longitude = -71.0;
    frame.altitudeMeters = 15000.0;
    frame.speedMps = 25.0f;
    frame.climbRateMps = 5.2f;
    frame.headingDeg = 180.0f;
    frame.gpsValid = true;
    frame.temperatureC = -45.2f;
    frame.relativeHumidityPercent = 12.0f;

    std::string json = uplink.formatTelemetryJson(frame);
    assert(json.find("\"uploader_callsign\": \"W1AW\"") != std::string::npos);
    assert(json.find("\"payload_callsign\": \"V1234567\"") != std::string::npos);
    assert(json.find("\"type\": \"RS41\"") != std::string::npos);

    std::cout << "[TEST PASSED] SondeHub v2 JSON payload formatter." << std::endl;
}

void testAprsFormatting() {
    RadiosondePI::Uplink::AprsGateway aprs("W1AW-11");

    RadiosondePI::Telemetry::TelemetryFrame frame{};
    frame.type = RadiosondePI::Telemetry::SondeType::RS41;
    frame.serialNumber = "V1234567";
    frame.latitude = 42.3601;
    frame.longitude = -71.0589;
    frame.altitudeMeters = 12000.0;
    frame.speedMps = 20.0f;
    frame.climbRateMps = -4.5f;
    frame.headingDeg = 90.0f;
    frame.gpsValid = true;

    std::string packet = aprs.formatAprsPacket(frame);
    assert(packet.find("W1AW-11>APRS,TCPIP*:") != std::string::npos);
    assert(packet.find("V1234567") != std::string::npos);
    assert(packet.find("/A=") != std::string::npos);

    std::cout << "[TEST PASSED] APRS-IS packet formatter." << std::endl;
}

void testLandingPredictor() {
    RadiosondePI::Telemetry::LandingPredictor predictor;

    RadiosondePI::Telemetry::TelemetryFrame frame{};
    frame.gpsValid = true;
    frame.latitude = 40.0;
    frame.longitude = -75.0;
    frame.altitudeMeters = 3000.0;
    frame.climbRateMps = -10.0f; // 10 m/s descent
    frame.speedMps = 15.0f;      // 15 m/s horizontal drift
    frame.headingDeg = 90.0f;

    predictor.addPoint(frame);
    auto trajectory = predictor.predictLanding(0.0);

    assert(!trajectory.empty());
    assert(trajectory.back().altitudeMeters == 0.0);
    // At 10 m/s descent from 3000m, time to land should be ~300 seconds
    assert(trajectory.back().timeToLandSeconds == 300);

    std::cout << "[TEST PASSED] Landing Predictor descending trajectory calculation." << std::endl;
}

void testWebDashboardSerialization() {
    RadiosondePI::Web::WebDashboard dashboard;

    RadiosondePI::Telemetry::TelemetryFrame frame{};
    frame.type = RadiosondePI::Telemetry::SondeType::DFM09;
    frame.serialNumber = "DFM-998877";
    frame.latitude = 45.0;
    frame.longitude = 10.0;
    frame.altitudeMeters = 8000.0;
    frame.gpsValid = true;

    dashboard.updateTelemetry(frame);
    std::string json = dashboard.getLatestTelemetryJson();
    assert(json.find("\"type\": \"DFM09\"") != std::string::npos);
    assert(json.find("\"serial\": \"DFM-998877\"") != std::string::npos);

    std::cout << "[TEST PASSED] Web Dashboard telemetry JSON API." << std::endl;
}

int main() {
    std::cout << "--- Running Sprint 3 Web UI & Uplink Tests ---" << std::endl;
    testSondeHubPayloadFormatting();
    testAprsFormatting();
    testLandingPredictor();
    testWebDashboardSerialization();
    std::cout << "--- All Sprint 3 Tests Passed ---" << std::endl;
    return 0;
}
