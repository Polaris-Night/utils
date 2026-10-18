#pragma once

#include <cmath>
#include <limits>
#include <type_traits>

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
 * @note 使用绝对误差比较，大数值场景（如 1e15 附近）可能失效，
 *       此类场景建议使用 IsClose()。
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
 * @note 使用绝对误差比较，大数值场景可能失效，此类场景建议使用 IsClose()。
 *
 * @tparam
 */
template <>
inline bool IsEqual<float>( float a, float b ) {
    return std::fabs( a - b ) < std::numeric_limits<float>::epsilon();
}

/**
 * @brief 相对+绝对容差比较两个浮点数是否接近
 *
 * 相对容差随数值量级放大，适合大数值场景；绝对容差用于处理接近 0 的数值。
 *
 * @tparam T 浮点类型（float / double / long double）
 * @param a 值 a
 * @param b 值 b
 * @param rel_tol 相对容差（默认约为 8 个机器精度）
 * @param abs_tol 绝对容差下限（默认 0）
 * @return true 两值接近
 * @return false 两值不接近（含 NaN 及不同号的无穷）
 *
 * @code{.cpp}
 *   IsClose(1e15, 1e15 + 1.0);        // true（相对容差）
 *   IsClose(1e15, 1e15 + 100.0);      // false（超出默认相对容差）
 *   IsClose(0.0, 1e-18, 1e-9, 1e-16); // true（绝对容差）
 * @endcode
 */
template <typename T>
bool IsClose( T a, T b, T rel_tol = std::numeric_limits<T>::epsilon() * 8, T abs_tol = static_cast<T>( 0 ) ) {
    static_assert( std::is_floating_point_v<T>, "IsClose requires floating point types" );

    if ( a == b ) {
        return true;  // 处理完全相同以及同号无穷
    }
    if ( !std::isfinite( a ) || !std::isfinite( b ) ) {
        return false;  // NaN 或不同号的无穷
    }

    const T diff  = std::fabs( a - b );
    const T scale = std::fabs( a ) > std::fabs( b ) ? std::fabs( a ) : std::fabs( b );
    return diff <= abs_tol || diff <= rel_tol * scale;
}

}  // namespace utils