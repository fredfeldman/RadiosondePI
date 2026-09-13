#include "decoders/DFMDecoder.hpp"
#include <iostream>
#include <cstring>
#include <cmath>

namespace RadiosondePI::Decoders {

DFMDecoder::DFMDecoder() {
    reset();
}

void DFMDecoder::setFrameCallback(FrameCallback callback) {
    m_frameCallback = std::move(callback);
}

void DFMDecoder::reset() {
    m_shiftRegister = 0;
    m_bitBuffer.clear();
    m_inFrame = false;
    m_manchesterPhase = false;
    m_lastManchesterBit = 0;
}

uint16_t DFMDecoder::computeCrc16(const uint8_t* buffer, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(buffer[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

void DFMDecoder::deinterleave(const uint8_t* input, uint8_t* output) {
    // Standard DFM 13x8 bit de-interleaving matrix
    std::memset(output, 0, 13);
    for (int col = 0; col < 8; ++col) {
        for (int row = 0; row < 13; ++row) {
            int srcBitIdx = col * 13 + row;
            int srcByte = srcBitIdx / 8;
            int srcBit = 7 - (srcBitIdx % 8);

            int bit = (input[srcByte] >> srcBit) & 1;

            if (bit) {
                output[row] |= (1 << (7 - col));
            }
        }
    }
}

void DFMDecoder::processBit(uint8_t bit) {
    m_shiftRegister = (m_shiftRegister << 1) | (bit & 1);

    if (!m_inFrame) {
        // DFM sync preamble: 0x9A9A9A9A or inverted 0x65656565 (Manchester encoded)
        if ((m_shiftRegister & 0xFFFFFFFFULL) == 0x9A9A9A9AULL ||
            (m_shiftRegister & 0xFFFFFFFFULL) == 0x65656565ULL) {
            m_inFrame = true;
            m_bitBuffer.clear();
            m_manchesterPhase = false;
        }
    } else {
        // Manchester decode: pair of bits (01 -> 0, 10 -> 1)
        if (!m_manchesterPhase) {
            m_lastManchesterBit = bit & 1;
            m_manchesterPhase = true;
        } else {
            m_manchesterPhase = false;
            uint8_t currentBit = bit & 1;
            if (m_lastManchesterBit == 0 && currentBit == 1) {
                m_bitBuffer.push_back(0);
            } else if (m_lastManchesterBit == 1 && currentBit == 0) {
                m_bitBuffer.push_back(1);
            } else {
                // Manchester violation
                if (m_bitBuffer.size() > 200) {
                    // Assemble bytes from bit buffer and process
                    std::vector<uint8_t> frameBytes(m_bitBuffer.size() / 8, 0);
                    for (size_t i = 0; i < m_bitBuffer.size(); ++i) {
                        if (m_bitBuffer[i]) {
                            frameBytes[i / 8] |= (1 << (7 - (i % 8)));
                        }
                    }
                    processRawFrame(frameBytes.data(), frameBytes.size());
                }
                m_inFrame = false;
                m_bitBuffer.clear();
                return;
            }

            // DFM sub-frame length is typically 26 to 52 decoded bytes (208 to 416 bits)
            if (m_bitBuffer.size() >= 416) {
                std::vector<uint8_t> frameBytes(52, 0);
                for (size_t i = 0; i < 416; ++i) {
                    if (m_bitBuffer[i]) {
                        frameBytes[i / 8] |= (1 << (7 - (i % 8)));
                    }
                }
                processRawFrame(frameBytes.data(), frameBytes.size());
                m_inFrame = false;
                m_bitBuffer.clear();
            }
        }
    }
}

void DFMDecoder::processRawFrame(const uint8_t* frameData, size_t length) {
    if (length < 26) return;

    uint8_t deinterleaved[26]{};
    deinterleave(frameData, deinterleaved);

    // Channel ID / Type byte (high nibble)
    uint8_t typeNibble = (deinterleaved[0] >> 4) & 0x0F;
    
    Telemetry::TelemetryFrame telemetry{};
    telemetry.type = Telemetry::SondeType::DFM09;
    telemetry.timestamp = std::chrono::system_clock::now();

    // DFM GPS position block (Channel 0x0 / 0x1)
    if (typeNibble == 0x0 || typeNibble == 0x1) {
        int32_t latRaw = (deinterleaved[1] << 24) | (deinterleaved[2] << 16) | (deinterleaved[3] << 8) | deinterleaved[4];
        int32_t lonRaw = (deinterleaved[5] << 24) | (deinterleaved[6] << 16) | (deinterleaved[7] << 8) | deinterleaved[8];
        int32_t altRaw = (deinterleaved[9] << 24) | (deinterleaved[10] << 16) | (deinterleaved[11] << 8) | deinterleaved[12];

        telemetry.latitude = latRaw / 1000000.0;
        telemetry.longitude = lonRaw / 1000000.0;
        telemetry.altitudeMeters = altRaw / 100.0;

        if (std::abs(telemetry.latitude) <= 90.0 && std::abs(telemetry.longitude) <= 180.0) {
            telemetry.gpsValid = true;
        }
    }

    // DFM ID / Serial block (Channel 0x2 / 0x3)
    if (typeNibble == 0x2 || typeNibble == 0x3) {
        uint32_t serialNum = (deinterleaved[1] << 24) | (deinterleaved[2] << 16) | (deinterleaved[3] << 8) | deinterleaved[4];
        telemetry.serialNumber = "DFM-" + std::to_string(serialNum);
    }

    if (m_frameCallback && (telemetry.gpsValid || !telemetry.serialNumber.empty())) {
        m_frameCallback(telemetry);
    }
}

} // namespace RadiosondePI::Decoders
