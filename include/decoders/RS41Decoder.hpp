#pragma once

#include "telemetry/TelemetryData.hpp"
#include <vector>
#include <cstdint>
#include <functional>
#include <array>

namespace RadiosondePI::Decoders {

class RS41Decoder {
public:
    using FrameCallback = std::function<void(const Telemetry::TelemetryFrame&)>;

    RS41Decoder();
    ~RS41Decoder() = default;

    void setFrameCallback(FrameCallback callback);
    void processBit(uint8_t bit);
    void processBytes(const uint8_t* data, size_t length);
    void reset();

    // Reed-Solomon (255, 231) FEC decoder for RS41
    static bool decodeReedSolomon(uint8_t* block, int& errorsCorrected);

    // Coordinate transformations
    static void ecefToGeodetic(double x, double y, double z, double& lat, double& lon, double& alt);

private:
    void processRawFrame(const uint8_t* frameData, size_t length);
    void dewhiten(uint8_t* buffer, size_t length);

    uint64_t m_shiftRegister{0};
    std::vector<uint8_t> m_byteBuffer;
    int m_bitCount{0};
    uint8_t m_currentByte{0};
    bool m_inFrame{false};

    FrameCallback m_frameCallback;

    static const std::array<uint8_t, 518> s_prbsMask;
};

} // namespace RadiosondePI::Decoders
