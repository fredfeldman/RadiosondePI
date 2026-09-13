#pragma once

#include "telemetry/TelemetryData.hpp"
#include <vector>
#include <cstdint>
#include <functional>
#include <array>

namespace RadiosondePI::Decoders {

/**
 * @brief Decoder for GRAW DFM-06, DFM-09, and DFM-17 radiosondes.
 * Standard 2400 baud FSK, Manchester encoded, de-interleaving matrix.
 */
class DFMDecoder {
public:
    using FrameCallback = std::function<void(const Telemetry::TelemetryFrame&)>;

    DFMDecoder();
    ~DFMDecoder() = default;

    void setFrameCallback(FrameCallback callback);
    void processBit(uint8_t bit);
    void reset();

private:
    void processRawFrame(const uint8_t* frameData, size_t length);
    void deinterleave(const uint8_t* input, uint8_t* output);
    static uint16_t computeCrc16(const uint8_t* buffer, size_t length);

    uint64_t m_shiftRegister{0};
    std::vector<uint8_t> m_bitBuffer;
    bool m_inFrame{false};
    uint8_t m_lastManchesterBit{0};
    bool m_manchesterPhase{false};

    FrameCallback m_frameCallback;
};

} // namespace RadiosondePI::Decoders
