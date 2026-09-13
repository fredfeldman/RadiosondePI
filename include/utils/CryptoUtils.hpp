#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace RadiosondePI::Utils {

class CryptoUtils {
public:
    static std::string base64Encode(const uint8_t* data, size_t len) {
        static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);

        for (size_t i = 0; i < len; i += 3) {
            uint32_t val = (static_cast<uint32_t>(data[i]) << 16);
            if (i + 1 < len) val |= (static_cast<uint32_t>(data[i + 1]) << 8);
            if (i + 2 < len) val |= static_cast<uint32_t>(data[i + 2]);

            out.push_back(kTable[(val >> 18) & 0x3F]);
            out.push_back(kTable[(val >> 12) & 0x3F]);
            out.push_back((i + 1 < len) ? kTable[(val >> 6) & 0x3F] : '=');
            out.push_back((i + 2 < len) ? kTable[val & 0x3F] : '=');
        }

        return out;
    }

    static void sha1(const uint8_t* msg, size_t len, uint8_t hash[20]) {
        uint32_t h0 = 0x67452301;
        uint32_t h1 = 0xEFCDAB89;
        uint32_t h2 = 0x98BADCFE;
        uint32_t h3 = 0x10325476;
        uint32_t h4 = 0xC3D2E1F0;

        size_t paddedLen = ((len + 8) / 64 + 1) * 64;
        std::vector<uint8_t> p(paddedLen, 0);
        std::memcpy(p.data(), msg, len);
        p[len] = 0x80;

        uint64_t bitLen = len * 8;
        for (int i = 0; i < 8; ++i) {
            p[paddedLen - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF);
        }

        for (size_t chunk = 0; chunk < paddedLen; chunk += 64) {
            uint32_t w[80];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(p[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(p[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(p[chunk + i * 4 + 2]) << 8) |
                       static_cast<uint32_t>(p[chunk + i * 4 + 3]);
            }
            for (int i = 16; i < 80; ++i) {
                uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
                w[i] = (val << 1) | (val >> 31);
            }

            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

            for (int i = 0; i < 80; ++i) {
                uint32_t f, k;
                if (i < 20) {
                    f = (b & c) | ((~b) & d);
                    k = 0x5A827999;
                } else if (i < 40) {
                    f = b ^ c ^ d;
                    k = 0x6ED9EBA1;
                } else if (i < 60) {
                    f = (b & c) | (b & d) | (c & d);
                    k = 0x8F1BBCDC;
                } else {
                    f = b ^ c ^ d;
                    k = 0xCA62C1D6;
                }

                uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
                e = d;
                d = c;
                c = (b << 30) | (b >> 2);
                b = a;
                a = temp;
            }

            h0 += a;
            h1 += b;
            h2 += c;
            h3 += d;
            h4 += e;
        }

        uint32_t h[5] = {h0, h1, h2, h3, h4};
        for (int i = 0; i < 5; ++i) {
            hash[i * 4] = static_cast<uint8_t>((h[i] >> 24) & 0xFF);
            hash[i * 4 + 1] = static_cast<uint8_t>((h[i] >> 16) & 0xFF);
            hash[i * 4 + 2] = static_cast<uint8_t>((h[i] >> 8) & 0xFF);
            hash[i * 4 + 3] = static_cast<uint8_t>(h[i] & 0xFF);
        }
    }

    static std::string generateWebSocketAccept(const std::string& clientKey) {
        std::string combined = clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint8_t hash[20];
        sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size(), hash);
        return base64Encode(hash, 20);
    }
};

} // namespace RadiosondePI::Utils
