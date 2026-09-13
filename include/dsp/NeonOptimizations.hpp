#pragma once

#include <cstddef>
#include <complex>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define HAS_ARM_NEON 1
#else
#define HAS_ARM_NEON 0
#endif

namespace RadiosondePI::DSP {

using Complex32 = std::complex<float>;

class NeonDSP {
public:
    /**
     * @brief NEON-accelerated FIR dot product for complex input samples and real taps.
     */
    static Complex32 firDotProduct(const Complex32* history, const float* taps, size_t count) {
#if HAS_ARM_NEON
        float32x4_t sumReal = vdupq_n_f32(0.0f);
        float32x4_t sumImag = vdupq_n_f32(0.0f);

        size_t i = 0;
        // Process in 4-sample chunks
        for (; i + 3 < count; i += 4) {
            float32x4_t tapVec = vld1q_f32(&taps[i]);

            // Deinterleave I and Q
            float32x4x2_t sampleVec = vld2q_f32(reinterpret_cast<const float*>(&history[i]));

            sumReal = vmlaq_f32(sumReal, sampleVec.val[0], tapVec);
            sumImag = vmlaq_f32(sumImag, sampleVec.val[1], tapVec);
        }

        // Horizontal add across vector lanes
        float r[4], img[4];
        vst1q_f32(r, sumReal);
        vst1q_f32(img, sumImag);

        float totalReal = r[0] + r[1] + r[2] + r[3];
        float totalImag = img[0] + img[1] + img[2] + img[3];

        // Process tail elements
        for (; i < count; ++i) {
            totalReal += history[i].real() * taps[i];
            totalImag += history[i].imag() * taps[i];
        }

        return Complex32(totalReal, totalImag);
#else
        // Portable Scalar fallback
        float totalReal = 0.0f;
        float totalImag = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            totalReal += history[i].real() * taps[i];
            totalImag += history[i].imag() * taps[i];
        }
        return Complex32(totalReal, totalImag);
#endif
    }
};

} // namespace RadiosondePI::DSP
