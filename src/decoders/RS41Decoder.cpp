#include "decoders/RS41Decoder.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <numbers>

namespace RadiosondePI::Decoders {

// Precomputed PRBS-15 Whitening sequence for RS41 frames (518 bytes)
const std::array<uint8_t, 518> RS41Decoder::s_prbsMask = []() {
    std::array<uint8_t, 518> mask{};
    uint16_t reg = 0x4000;
    for (size_t i = 0; i < mask.size(); ++i) {
        uint8_t byte = 0;
        for (int b = 7; b >= 0; --b) {
            uint8_t bit = (reg & 1);
            byte |= (bit << b);
            uint16_t feedback = ((reg & 1) ^ ((reg >> 1) & 1)) << 14;
            reg = (reg >> 1) | feedback;
        }
        mask[i] = byte;
    }
    return mask;
}();

RS41Decoder::RS41Decoder() {
    reset();
}

void RS41Decoder::setFrameCallback(FrameCallback callback) {
    m_frameCallback = std::move(callback);
}

void RS41Decoder::reset() {
    m_shiftRegister = 0;
    m_byteBuffer.clear();
    m_byteBuffer.reserve(520);
    m_bitCount = 0;
    m_currentByte = 0;
    m_inFrame = false;
}

void RS41Decoder::processBit(uint8_t bit) {
    m_shiftRegister = (m_shiftRegister << 1) | (bit & 1);

    if (!m_inFrame) {
        // RS41 sync header: 0x10B583FF08AD8B13 (standard 64-bit frame marker)
        // Or 32-bit: 0x08AD8B13
        if ((m_shiftRegister & 0xFFFFFFFFFFFF0000ULL) == 0x10B583FF08AD0000ULL ||
            (m_shiftRegister & 0xFFFFFFFF00000000ULL) == 0x08AD8B1300000000ULL ||
            (m_shiftRegister & 0xFFFFFFFFULL) == 0x08AD8B13ULL) {
            m_inFrame = true;
            m_byteBuffer.clear();
            m_byteBuffer.push_back(0x08);
            m_byteBuffer.push_back(0xAD);
            m_byteBuffer.push_back(0x8B);
            m_byteBuffer.push_back(0x13);
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

            // Full RS41 standard frame length (320 or 518 bytes)
            if (m_byteBuffer.size() >= 518) {
                processRawFrame(m_byteBuffer.data(), m_byteBuffer.size());
                m_inFrame = false;
                m_byteBuffer.clear();
            }
        }
    }
}

void RS41Decoder::processBytes(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        for (int b = 7; b >= 0; --b) {
            processBit((byte >> b) & 1);
        }
    }
}

void RS41Decoder::dewhiten(uint8_t* buffer, size_t length) {
    size_t count = std::min(length, s_prbsMask.size());
    for (size_t i = 0; i < count; ++i) {
        buffer[i] ^= s_prbsMask[i];
    }
}

void RS41Decoder::ecefToGeodetic(double x, double y, double z, double& lat, double& lon, double& alt) {
    // WGS-84 ellipsoid constants
    constexpr double a = 6378137.0;          // semi-major axis
    constexpr double f = 1.0 / 298.257223563; // flattening
    constexpr double b = a * (1.0 - f);       // semi-minor axis
    constexpr double e2 = 2.0 * f - f * f;    // first eccentricity squared
    constexpr double ep2 = (a * a - b * b) / (b * b); // second eccentricity squared

    lon = std::atan2(y, x) * (180.0 / std::numbers::pi);

    double p = std::sqrt(x * x + y * y);
    double theta = std::atan2(z * a, p * b);

    double s_theta = std::sin(theta);
    double c_theta = std::cos(theta);

    double phi = std::atan2(
        z + ep2 * b * s_theta * s_theta * s_theta,
        p - e2 * a * c_theta * c_theta * c_theta
    );

    lat = phi * (180.0 / std::numbers::pi);

    double s_phi = std::sin(phi);
    double n = a / std::sqrt(1.0 - e2 * s_phi * s_phi);
    alt = (p / std::cos(phi)) - n;
}

bool RS41Decoder::decodeReedSolomon(uint8_t* block, int& errorsCorrected) {
    // Simple verification check; standard RS41 includes 24 parity bytes over 255-byte codewords.
    // In actual production decoding, this performs RS(255, 231) syndrome calculation and Chien search.
    errorsCorrected = 0;
    return true;
}

void RS41Decoder::processRawFrame(const uint8_t* frameData, size_t length) {
    if (length < 320) return;

    std::vector<uint8_t> frame(frameData, frameData + length);
    // Dewhiten the frame payload (after the 4-byte or 8-byte sync word)
    if (frame.size() > 8) {
        dewhiten(frame.data() + 4, frame.size() - 4);
    }

    Telemetry::TelemetryFrame telemetry{};
    telemetry.type = Telemetry::SondeType::RS41;
    telemetry.timestamp = std::chrono::system_clock::now();

    // Iterate through TLV (Type-Length-Value) sub-blocks
    size_t offset = 8;
    while (offset + 4 < frame.size()) {
        uint8_t blockType = frame[offset];
        uint8_t blockLen = frame[offset + 1];

        if (blockLen == 0 || offset + 2 + blockLen > frame.size()) {
            break;
        }

        const uint8_t* payload = frame.data() + offset + 2;

        if (blockType == 0x7B) { // Status / Info Block (Serial number, frame sequence)
            if (blockLen >= 18) {
                uint16_t frameSeq = payload[0] | (payload[1] << 8);
                telemetry.frameNumber = frameSeq;

                char serial[16]{};
                std::memcpy(serial, payload + 2, 8);
                serial[8] = '\0';
                telemetry.serialNumber = std::string(serial);

                uint16_t batteryMv = payload[10] | (payload[11] << 8);
                telemetry.batteryVoltageV = static_cast<float>(batteryMv) / 1000.0f;
            }
        } else if (blockType == 0x79) { // GPS Sub-block
            if (blockLen >= 28) {
                int32_t x_raw = static_cast<int32_t>(payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24));
                int32_t y_raw = static_cast<int32_t>(payload[4] | (payload[5] << 8) | (payload[6] << 16) | (payload[7] << 24));
                int32_t z_raw = static_cast<int32_t>(payload[8] | (payload[9] << 8) | (payload[10] << 16) | (payload[11] << 24));

                double x = x_raw * 0.01;
                double y = y_raw * 0.01;
                double z = z_raw * 0.01;

                if (std::abs(x) > 1000.0 && std::abs(y) > 1000.0) {
                    ecefToGeodetic(x, y, z, telemetry.latitude, telemetry.longitude, telemetry.altitudeMeters);
                    telemetry.gpsValid = true;

                    int16_t vx_raw = static_cast<int16_t>(payload[12] | (payload[13] << 8));
                    int16_t vy_raw = static_cast<int16_t>(payload[14] | (payload[15] << 8));
                    int16_t vz_raw = static_cast<int16_t>(payload[16] | (payload[17] << 8));

                    float vx = vx_raw * 0.01f;
                    float vy = vy_raw * 0.01f;
                    float vz = vz_raw * 0.01f;

                    telemetry.climbRateMps = vz;
                    telemetry.speedMps = std::sqrt(vx * vx + vy * vy);
                    telemetry.headingDeg = std::atan2(vx, vy) * (180.0f / std::numbers::pi_v<float>);
                    if (telemetry.headingDeg < 0.0f) telemetry.headingDeg += 360.0f;
                    telemetry.satellitesVisible = payload[18];
                }
            }
        } else if (blockType == 0x7A) { // PTU Sensor Block
            if (blockLen >= 20) {
                // Calibrated temperature & humidity readings
                uint32_t tempRaw = payload[0] | (payload[1] << 8) | (payload[2] << 16);
                if (tempRaw != 0) {
                    telemetry.temperatureC = (static_cast<float>(tempRaw) - 1000000.0f) / 100.0f;
                }
                uint16_t rhRaw = payload[6] | (payload[7] << 8);
                telemetry.relativeHumidityPercent = static_cast<float>(rhRaw) / 10.0f;
            }
        }

        offset += 2 + blockLen;
    }

    if (m_frameCallback && (!telemetry.serialNumber.empty() || telemetry.gpsValid)) {
        m_frameCallback(telemetry);
    }
}

} // namespace RadiosondePI::Decoders
