#pragma once

#include <chrono>
#include <cstdint>
#include <type_traits>

#include "Clock.h"

namespace utils {

/**
 * @brief 经过多久：记录一个起点，随时计算从起点到“现在”的时长
 *
 * - 默认使用单调时钟（steady_clock），不受系统改时 / NTP 校时影响；
 * - 需要墙钟时间戳或日历时刻时使用 system_clock 版本；
 * - 构造即开始，没有 Start/Stop；重置用 Reset()；
 * - 值类型、可拷贝；Reset 是写操作，跨线程共享需自行同步。
 *
 * @tparam Clock 时钟类型，需提供 now() 与 time_point
 */
template <typename Clock = std::chrono::steady_clock>
class ElapsedTimer final {
public:
    using ClockType = Clock;
    using TimePoint = std::chrono::time_point<Clock, Nanoseconds>;

    /**
     * @brief 以当前时刻为起点
     */
    ElapsedTimer() noexcept : start_( Now() ) {}

    /**
     * @brief 以指定时间点为起点
     */
    explicit ElapsedTimer( TimePoint start ) noexcept : start_( start ) {}

    /**
     * @brief 起点重置为当前时刻
     */
    void Reset() noexcept { start_ = Now(); }

    /**
     * @brief 起点重置为指定时间点
     */
    void Reset( TimePoint start ) noexcept { start_ = start; }

    /**
     * @brief 起点
     */
    [[nodiscard]] TimePoint StartedAt() const noexcept { return start_; }

    /**
     * @brief 当前时钟时间点
     */
    [[nodiscard]] static TimePoint Now() noexcept { return std::chrono::time_point_cast<Nanoseconds>( Clock::now() ); }

    /**
     * @brief 从起点到当前时刻经过的时长
     */
    [[nodiscard]] Nanoseconds Elapsed() const noexcept { return ElapsedAt( Now() ); }

    /**
     * @brief 从起点到指定时刻经过的时长（快照式，便于事件循环与测试）
     */
    [[nodiscard]] Nanoseconds ElapsedAt( TimePoint now ) const noexcept {
        return std::chrono::duration_cast<Nanoseconds>( now - start_ );
    }

    /**
     * @brief 经过的毫秒数（默认 int64_t，可显式指定 double 等类型）
     */
    template <typename T = std::int64_t>
    [[nodiscard]] T ElapsedMillis() const noexcept {
        return ElapsedAs<Milliseconds, T>();
    }

    /**
     * @brief 经过的秒数（默认 int64_t，可显式指定 double 等类型）
     */
    template <typename T = std::int64_t>
    [[nodiscard]] T ElapsedSeconds() const noexcept {
        return ElapsedAs<Seconds, T>();
    }

    /**
     * @brief 以任意时间单位返回经过时长
     *
     * @tparam Unit 目标单位，如 Microseconds、Minutes、Hours
     * @tparam T    返回类型；整型截断，浮点保留小数
     */
    template <typename Unit, typename T = std::int64_t>
    [[nodiscard]] T ElapsedAs() const noexcept {
        static_assert( detail::IsDuration_v<Unit>, "Unit 必须是 std::chrono::duration 类型" );
        return detail::ConvertDuration<Unit, T>( Elapsed() );
    }

    // ---- 以下接口仅 system_clock（或 Unix 纪元自定义时钟）可用 ----

    /**
     * @brief 起点的 Unix 毫秒时间戳（向下取整）
     */
    [[nodiscard]] std::int64_t StartedAtUnixMillis() const {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixMillis( start_ );
    }

    /**
     * @brief 起点的 Unix 秒时间戳（向下取整）
     */
    [[nodiscard]] std::int64_t StartedAtUnixSeconds() const {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixSeconds( start_ );
    }

    /**
     * @brief 起点对应的日期时间
     */
    [[nodiscard]] DateTime StartedAtDateTime( const TimeZone &zone = TimeZone::Local() ) const {
        return ToDateTime( start_, zone );
    }

    /**
     * @brief 任意时间点 -> Unix 毫秒（向下取整）
     */
    [[nodiscard]] static std::int64_t ToUnixMillis( TimePoint tp ) {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixMillis( tp );
    }

    /**
     * @brief 任意时间点 -> Unix 秒（向下取整）
     */
    [[nodiscard]] static std::int64_t ToUnixSeconds( TimePoint tp ) {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixSeconds( tp );
    }

    /**
     * @brief 任意时间点 -> 日期时间
     */
    [[nodiscard]] static DateTime ToDateTime( TimePoint tp, const TimeZone &zone = TimeZone::Local() ) {
        return detail::ToDateTime( tp, zone );
    }

private:
    TimePoint start_;
};

using SteadyElapsedTimer = ElapsedTimer<std::chrono::steady_clock>;
using SystemElapsedTimer = ElapsedTimer<std::chrono::system_clock>;

}  // namespace utils
