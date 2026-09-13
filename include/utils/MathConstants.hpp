#pragma once

#include <cmath>

#if __has_include(<numbers>)
#include <numbers>
#endif

namespace RadiosondePI::Math {

#if defined(__cpp_lib_math_constants) && __cpp_lib_math_constants >= 201907L
    inline constexpr double Pi = std::numbers::pi;
    inline constexpr float PiF = std::numbers::pi_v<float>;
#else
    inline constexpr double Pi = 3.14159265358979323846;
    inline constexpr float PiF = 3.14159265358979323846f;
#endif

} // namespace RadiosondePI::Math
