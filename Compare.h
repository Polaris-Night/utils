#pragma once

#include <cmath>
#include <limits>

namespace utils {

/**
 * @brief 通用比较相等
 *
 * @tparam T
 */
template <typename T>
bool IsEqual( T a, T b ) {
    return a == b;
}

/**
 * @brief double比较相等
 *
 * @tparam
 */
template <>
inline bool IsEqual<double>( double a, double b ) {
    return std::fabs( a - b ) < std::numeric_limits<double>::epsilon();
}

/**
 * @brief float比较相等
 *
 * @tparam
 */
template <>
inline bool IsEqual<float>( float a, float b ) {
    return std::fabs( a - b ) < std::numeric_limits<float>::epsilon();
}

}  // namespace utils