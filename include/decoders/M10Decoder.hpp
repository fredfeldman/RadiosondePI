#pragma once

#include "telemetry/TelemetryData.hpp"
#include <vector>
#include <cstdint>
#include <functional>

namespace RadiosondePI::Decoders {

/**
 * @brief Decoder for Meteomodem M10 / M20 radiosondes.
 * 9600 baud GFSK, Differential Manchester / Manchester encoded.
 */
class M10Decoder {
public:
    using FrameCallback = std::function<void(const Telemetry::TelemetryFrame&)>;

    M10Decoder();
    ~M10Decoder() = default;

    void setFrameCallback(FrameCallback callback);
    void processBit(uint8_t bit);
    void reset();

private:
    void processRawFrame(const uint8_t* frameData, size_t length);
    static uint16_t computeCrc16(const uint8_t* buffer, size_t length);

    uint64_t m_shiftRegister{0};
    std::vector<uint8_t> m_byteBuffer;
    bool m_inFrame{false};
    int m_bitCount{0};
    uint8_t m_currentByte{0};

    FrameCallback m_frameCallback;
};

} // namespace RadiosondePI::Decoders
