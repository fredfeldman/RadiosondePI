#include "decoders/M10Decoder.hpp"
#include <iostream>
#include <cstring>
#include <cmath>

namespace RadiosondePI::Decoders {

M10Decoder::M10Decoder() {
    reset();
}

void M10Decoder::setFrameCallback(FrameCallback callback) {
    m_frameCallback = std::move(callback);
}

void M10Decoder::reset() {
    m_shiftRegister = 0;
    m_byteBuffer.clear();
    m_inFrame = false;
    m_bitCount = 0;
    m_currentByte = 0;
}

uint16_t M10Decoder::computeCrc16(const uint8_t* buffer, size_t length) {
    uint16_t crc = 0;
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

void M10Decoder::processBit(uint8_t bit) {
    m_shiftRegister = (m_shiftRegister << 1) | (bit & 1);

    if (!m_inFrame) {
        // M10 32-bit Sync Word: 0x66666666 followed by 0x5555 or 0xAAAA (Differential Manchester)
        // Header pattern: 0x0000000000000066 or 0x66666666
        if ((m_shiftRegister & 0xFFFFFFULL) == 0x666666ULL ||
            (m_shiftRegister & 0xFFFFFFFFULL) == 0x66666666ULL) {
            m_inFrame = true;
            m_byteBuffer.clear();
            m_byteBuffer.push_back(0x66);
            m_bitCount = 0;
            m_currentByte = 0;
        }
    } else {
        m_currentByte = (m_currentByte << 1) | (bit & 1);
        m_bitCount++;

        if (m_bitCount == 8) {
            m_byteBuffer.push_back(m_currentByte);
            m_bitCount = 0;
            m_currentByte = 0;

            // Typical M10 frame is 100 bytes
            if (m_byteBuffer.size() >= 100) {
                processRawFrame(m_byteBuffer.data(), m_byteBuffer.size());
                m_inFrame = false;
                m_byteBuffer.clear();
            }
        }
    }
}

void M10Decoder::processRawFrame(const uint8_t* frameData, size_t length) {
    if (length < 100) return;

    Telemetry::TelemetryFrame telemetry{};
    telemetry.type = Telemetry::SondeType::M10;
    telemetry.timestamp = std::chrono::system_clock::now();

    // M10 Frame Counter
    telemetry.frameNumber = (frameData[3] << 8) | frameData[4];

    // M10 Serial Number parsing (typically encoded in bytes 0x5E..0x63)
    char serial[16]{};
    std::snprintf(serial, sizeof(serial), "M10-%02X%02X%02X", frameData[94], frameData[95], frameData[96]);
    telemetry.serialNumber = serial;

    // GPS ECEF / Coordinates parsing
    int32_t latRaw = (frameData[14] << 24) | (frameData[15] << 16) | (frameData[16] << 8) | frameData[17];
    int32_t lonRaw = (frameData[18] << 24) | (frameData[19] << 16) | (frameData[20] << 8) | frameData[21];
    int32_t altRaw = (frameData[22] << 24) | (frameData[23] << 16) | (frameData[24] << 8) | frameData[25];

    double lat = latRaw * (180.0 / 2147483648.0);
    double lon = lonRaw * (360.0 / 4294967296.0);
    double alt = altRaw / 1000.0;

    if (std::abs(lat) <= 90.0 && std::abs(lon) <= 180.0 && alt > -500.0 && alt < 50000.0) {
        telemetry.latitude = lat;
        telemetry.longitude = lon;
        telemetry.altitudeMeters = alt;
        telemetry.gpsValid = true;
    }

    if (m_frameCallback && (telemetry.gpsValid || !telemetry.serialNumber.empty())) {
        m_frameCallback(telemetry);
    }
}

} // namespace RadiosondePI::Decoders
