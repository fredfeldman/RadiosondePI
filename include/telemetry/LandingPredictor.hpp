#pragma once

#include "telemetry/TelemetryData.hpp"
#include <vector>
#include <deque>
#include <cmath>
#include <numbers>

namespace RadiosondePI::Telemetry {

struct PredictionPoint {
    double latitude{0.0};
    double longitude{0.0};
    double altitudeMeters{0.0};
    int timeToLandSeconds{0};
};

/**
 * @brief Predictive trajectory and landing calculator using descent rate and wind vectors.
 */
class LandingPredictor {
public:
    LandingPredictor() = default;

    void addPoint(const TelemetryFrame& frame) {
        if (!frame.gpsValid) return;

        m_history.push_back(frame);
        if (m_history.size() > 60) {
            m_history.pop_front();
        }
    }

    [[nodiscard]] std::vector<PredictionPoint> predictLanding(double groundElevationMeters = 0.0) const {
        std::vector<PredictionPoint> path;
        if (m_history.empty()) return path;

        const auto& latest = m_history.back();
        if (!latest.gpsValid || latest.climbRateMps >= -0.5f || latest.altitudeMeters <= groundElevationMeters) {
            // Not descending or already on the ground
            return path;
        }

        double currLat = latest.latitude;
        double currLon = latest.longitude;
        double currAlt = latest.altitudeMeters;
        float descentRate = std::abs(latest.climbRateMps); // m/s downwards
        float speed = latest.speedMps;                      // m/s ground speed
        float headingRad = latest.headingDeg * (std::numbers::pi_v<float> / 180.0f);

        int timeElapsed = 0;
        constexpr int timeStepSec = 10;
        constexpr double metersPerDegreeLat = 111320.0;

        while (currAlt > groundElevationMeters && timeElapsed < 7200) { // Max 2 hours
            timeElapsed += timeStepSec;
            currAlt -= descentRate * timeStepSec;
            if (currAlt < groundElevationMeters) currAlt = groundElevationMeters;

            double distanceTraveled = speed * timeStepSec;
            double dLat = (distanceTraveled * std::cos(headingRad)) / metersPerDegreeLat;
            double dLon = (distanceTraveled * std::sin(headingRad)) / (metersPerDegreeLat * std::cos(currLat * std::numbers::pi / 180.0));

            currLat += dLat;
            currLon += dLon;

            path.push_back(PredictionPoint{currLat, currLon, currAlt, timeElapsed});
        }

        return path;
    }

    void reset() {
        m_history.clear();
    }

private:
    std::deque<TelemetryFrame> m_history;
};

} // namespace RadiosondePI::Telemetry
