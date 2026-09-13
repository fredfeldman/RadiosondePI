#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "dsp/NeonOptimizations.hpp"
#include "storage/FlightLogger.hpp"

void testNeonDspDotProduct() {
    std::vector<RadiosondePI::DSP::Complex32> samples = {
        {1.0f, 2.0f},
        {3.0f, 4.0f},
        {5.0f, 6.0f},
        {7.0f, 8.0f}
    };
    std::vector<float> taps = {0.5f, 0.5f, 0.5f, 0.5f};

    auto result = RadiosondePI::DSP::NeonDSP::firDotProduct(samples.data(), taps.data(), 4);
    assert(result.real() == 8.0f);
    assert(result.imag() == 10.0f);

    std::cout << "[TEST PASSED] NEON/SIMD Vector Dot Product." << std::endl;
}

void testFlightLoggerExport() {
    RadiosondePI::Storage::FlightLogger logger;

    RadiosondePI::Telemetry::TelemetryFrame f1{};
    f1.type = RadiosondePI::Telemetry::SondeType::RS41;
    f1.serialNumber = "V99887766";
    f1.frameNumber = 101;
    f1.timestamp = std::chrono::system_clock::now();
    f1.latitude = 51.5074;
    f1.longitude = -0.1278;
    f1.altitudeMeters = 25000.0;
    f1.climbRateMps = 6.0f;
    f1.speedMps = 30.0f;
    f1.headingDeg = 270.0f;
    f1.gpsValid = true;
    f1.temperatureC = -55.0f;
    f1.relativeHumidityPercent = 5.0f;
    f1.batteryVoltageV = 3.05f;

    logger.logFrame(f1);
    assert(logger.getRecordCount() == 1);

    std::string csv = logger.exportCsv();
    assert(csv.find("V99887766") != std::string::npos);
    assert(csv.find("51.50740") != std::string::npos);

    std::string kml = logger.exportKml("RS41 Test Flight");
    assert(kml.find("<kml xmlns=\"http://www.opengis.net/kml/2.2\">") != std::string::npos);
    assert(kml.find("-0.127800,51.507400,25000.0") != std::string::npos);

    std::cout << "[TEST PASSED] Flight Logger CSV & KML export." << std::endl;
}

int main() {
    std::cout << "--- Running Sprint 4 Optimization & Storage Tests ---" << std::endl;
    testNeonDspDotProduct();
    testFlightLoggerExport();
    std::cout << "--- All Sprint 4 Tests Passed ---" << std::endl;
    return 0;
}
