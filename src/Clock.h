#pragma once

#include <chrono>
#include <cstdint>
#include <type_traits>

#include "DateTime.h"

namespace utils {

// ==================== 时钟萃取 ====================

/**
 * @brief 时钟特性：判断一个时钟的纪元是否为 Unix 纪元
 *
 * 只有 Unix 纪元时钟才能给出“时间戳 / 日历时刻”。system_clock 内置为 true；
 * 自定义时钟若以 Unix 纪元为起点，可特化本模板：
 *
 * @code
 * template <>
 * struct utils::ClockTraits<MyUnixClock> {
 *     static constexpr bool unix_epoch = true;
 * };
 * @endcode
 */
template <typename Clock>
struct ClockTraits {
    static constexpr bool unix_epoch = false;
};

template <>
struct ClockTraits<std::chrono::system_clock> {
    static constexpr bool unix_epoch = true;
};

template <typename Clock>
inline constexpr bool IsUnixEpochClock_v = ClockTraits<Clock>::unix_epoch;

// ==================== 内部辅助 ====================

namespace detail {

template <typename T>
struct IsDuration : std::false_type {};

template <typename Rep, typename Period>
struct IsDuration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <typename T>
inline constexpr bool IsDuration_v = IsDuration<T>::value;

/**
 * @brief 把一个 duration 换算为目标单位
 *
 * @tparam Unit 目标 duration 类型
 * @tparam T    返回类型；整型截断，浮点保留小数
 */
template <typename Unit, typename T>
[[nodiscard]] T ConvertDuration( Nanoseconds value ) noexcept {
    static_assert( std::is_arithmetic_v<T>, "返回类型必须是算术类型" );

    if constexpr ( std::is_integral_v<T> ) {
        return static_cast<T>( std::chrono::duration_cast<Unit>( value ).count() );
    }
    else {
        return static_cast<T>( std::chrono::duration<double, typename Unit::period>( value ).count() );
    }
}

/**
 * @brief 预算向上取整到纳秒，避免 0.5ns 被截断成 0 而“构造即到期”
 */
template <typename Rep, typename Period>
[[nodiscard]] constexpr Nanoseconds BudgetToNanoseconds( std::chrono::duration<Rep, Period> value ) noexcept {
    return std::chrono::ceil<Nanoseconds>( value );
}

/**
 * @brief Unix 纪元时刻 -> Unix 毫秒（向下取整）
 */
template <typename Clock>
[[nodiscard]] std::int64_t ToUnixMillis( std::chrono::time_point<Clock, Nanoseconds> tp ) {
    static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
    return std::chrono::floor<Milliseconds>( tp.time_since_epoch() ).count();
}

/**
 * @brief Unix 纪元时刻 -> Unix 秒（向下取整）
 */
template <typename Clock>
[[nodiscard]] std::int64_t ToUnixSeconds( std::chrono::time_point<Clock, Nanoseconds> tp ) {
    static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
    return std::chrono::floor<Seconds>( tp.time_since_epoch() ).count();
}

/**
 * @brief Unix 纪元时刻 -> DateTime
 */
template <typename Clock>
[[nodiscard]] DateTime ToDateTime( std::chrono::time_point<Clock, Nanoseconds> tp,
                                   const TimeZone                             &zone = TimeZone::Local() ) {
    static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有日历含义，无法转换为 DateTime；请使用 system_clock" );
    using SysDuration = Timestamp::duration;
    return DateTime( Timestamp( std::chrono::duration_cast<SysDuration>( tp.time_since_epoch() ) ), zone );
}

}  // namespace detail

}  // namespace utils
