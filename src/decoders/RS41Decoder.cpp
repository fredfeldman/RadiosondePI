#include "decoders/RS41Decoder.hpp"
#include "utils/MathConstants.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <array>

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

// Galois Field GF(2^8) tables for Reed-Solomon (255, 231) decoding
// Polynomial: x^8 + x^4 + x^3 + x^2 + 1 (0x11D, generator root alpha = 2)
namespace GF {
    static uint8_t expTable[512];
    static uint8_t logTable[256];
    static bool initialized = false;

    static void initTables() {
        if (initialized) return;
        uint16_t x = 1;
        for (int i = 0; i < 255; ++i) {
            expTable[i] = static_cast<uint8_t>(x);
            expTable[i + 255] = static_cast<uint8_t>(x);
            logTable[x] = static_cast<uint8_t>(i);
            x <<= 1;
            if (x & 0x100) {
                x ^= 0x11D; // Poly 0x11D
            }
        }
        logTable[0] = 0; // undefined, set to 0
        initialized = true;
    }

    inline uint8_t mul(uint8_t a, uint8_t b) {
        if (a == 0 || b == 0) return 0;
        return expTable[logTable[a] + logTable[b]];
    }

    inline uint8_t div(uint8_t a, uint8_t b) {
        if (a == 0) return 0;
        if (b == 0) return 0; // Error / div by 0
        return expTable[(logTable[a] - logTable[b] + 255) % 255];
    }

    inline uint8_t inv(uint8_t a) {
        if (a == 0) return 0;
        return expTable[255 - logTable[a]];
    }
} // namespace GF

RS41Decoder::RS41Decoder() {
    GF::initTables();
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

    lon = std::atan2(y, x) * (180.0 / RadiosondePI::Math::Pi);

    double p = std::sqrt(x * x + y * y);
    double theta = std::atan2(z * a, p * b);

    double s_theta = std::sin(theta);
    double c_theta = std::cos(theta);

    double phi = std::atan2(
        z + ep2 * b * s_theta * s_theta * s_theta,
        p - e2 * a * c_theta * c_theta * c_theta
    );

    lat = phi * (180.0 / RadiosondePI::Math::Pi);

    double s_phi = std::sin(phi);
    double n = a / std::sqrt(1.0 - e2 * s_phi * s_phi);
    alt = (p / std::cos(phi)) - n;
}

bool RS41Decoder::decodeReedSolomon(uint8_t* block, int& errorsCorrected) {
    // RS(255, 231) parameters: N=255, K=231, 2t=24 parity bytes
    GF::initTables();
    errorsCorrected = 0;
    constexpr int N = 255;
    constexpr int NPAR = 24;
    constexpr int MAXERR = 12;

    // 1. Calculate Syndromes S[0..23] = block(alpha^i)
    uint8_t S[NPAR]{};
    bool hasErrors = false;

    for (int i = 0; i < NPAR; ++i) {
        uint8_t sum = 0;
        uint8_t alpha_i = GF::expTable[i];
        for (int j = 0; j < N; ++j) {
            sum = block[j] ^ GF::mul(sum, alpha_i);
        }
        S[i] = sum;
        if (sum != 0) hasErrors = true;
    }

    if (!hasErrors) {
        return true; // No errors in block
    }

    // 2. Berlekamp-Massey Algorithm for Error Locator Polynomial Lambda
    std::array<uint8_t, NPAR + 1> lambda{};
    std::array<uint8_t, NPAR + 1> b{};
    lambda[0] = 1;
    b[0] = 1;

    int L = 0;
    int k = 1;
    uint8_t gamma = 1;

    for (int r = 0; r < NPAR; ++r) {
        // Discrepancy delta = S[r] + sum_{i=1}^L lambda[i] * S[r-i]
        uint8_t delta = S[r];
        for (int i = 1; i <= L; ++i) {
            delta ^= GF::mul(lambda[i], S[r - i]);
        }

        if (delta == 0) {
            k++;
        } else {
            std::array<uint8_t, NPAR + 1> temp = lambda;
            uint8_t factor = GF::div(delta, gamma);

            for (int i = 0; i + k <= NPAR; ++i) {
                temp[i + k] ^= GF::mul(factor, b[i]);
            }

            if (2 * L <= r) {
                L = r + 1 - L;
                b = lambda;
                gamma = delta;
                k = 1;
            } else {
                k++;
            }
            lambda = temp;
        }
    }

    if (L > MAXERR) {
        return false; // Too many errors
    }

    // 3. Chien Search: find roots of Lambda(x)
    std::vector<int> errorPos;
    errorPos.reserve(L);

    for (int i = 0; i < N; ++i) {
        uint8_t alpha_inv = GF::expTable[(255 - i) % 255];
        uint8_t sum = 1;
        uint8_t term = 1;

        for (int j = 1; j <= L; ++j) {
            term = GF::mul(term, alpha_inv);
            sum ^= GF::mul(lambda[j], term);
        }

        if (sum == 0) {
            errorPos.push_back(N - 1 - i);
        }
    }

    if (static_cast<int>(errorPos.size()) != L) {
        return false; // Root count mismatch -> uncorrectable error pattern
    }

    // 4. Forney Algorithm: compute error magnitudes
    // Omega(x) = [S(x) * Lambda(x)] mod x^(NPAR)
    std::array<uint8_t, NPAR> omega{};
    for (int i = 0; i < NPAR; ++i) {
        for (int j = 0; j <= i && j <= L; ++j) {
            omega[i] ^= GF::mul(S[i - j], lambda[j]);
        }
    }

    // Apply error corrections
    for (int pos : errorPos) {
        int i = N - 1 - pos;
        uint8_t Xk_inv = GF::expTable[(255 - i) % 255];

        // Omega(Xk_inv)
        uint8_t num = 0;
        uint8_t term = 1;
        for (int j = 0; j < NPAR; ++j) {
            num ^= GF::mul(omega[j], term);
            term = GF::mul(term, Xk_inv);
        }

        // Lambda'(Xk_inv) (formal derivative of Lambda)
        uint8_t denom = 0;
        term = 1;
        for (int j = 1; j <= L; j += 2) {
            denom ^= GF::mul(lambda[j], term);
            term = GF::mul(term, GF::mul(Xk_inv, Xk_inv));
        }

        if (denom == 0) return false;

        uint8_t errorVal = GF::div(num, denom);
        block[pos] ^= errorVal;
    }

    errorsCorrected = L;
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

    // Perform Reed-Solomon error correction if full 518 byte frame
    if (frame.size() >= 518) {
        // RS41 interleaved codewords
        uint8_t block1[255]{};
        uint8_t block2[255]{};
        for (size_t i = 0; i < 255; ++i) {
            if (8 + 2 * i < frame.size()) block1[i] = frame[8 + 2 * i];
            if (9 + 2 * i < frame.size()) block2[i] = frame[9 + 2 * i];
        }

        int err1 = 0, err2 = 0;
        bool ok1 = decodeReedSolomon(block1, err1);
        bool ok2 = decodeReedSolomon(block2, err2);

        if (ok1 && ok2) {
            for (size_t i = 0; i < 255; ++i) {
                if (8 + 2 * i < frame.size()) frame[8 + 2 * i] = block1[i];
                if (9 + 2 * i < frame.size()) frame[9 + 2 * i] = block2[i];
            }
            telemetry.eccCorrected = (err1 > 0 || err2 > 0);
            telemetry.eccErrorsCorrected = static_cast<uint16_t>(err1 + err2);
        }
    }

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
                    telemetry.headingDeg = std::atan2(vx, vy) * (180.0f / RadiosondePI::Math::PiF);
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
