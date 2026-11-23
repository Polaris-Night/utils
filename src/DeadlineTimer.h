#pragma once

#include <chrono>
#include <cstdint>
#include <type_traits>

#include "Clock.h"

namespace utils {

/**
 * @brief 还剩多久 / 是否到期：起点 + 预算 = 截止点
 *
 * - 默认使用单调时钟（steady_clock），适合超时、重试、预算传递；
 * - 需要“何时截止”的墙钟时间戳 / 日历时刻时使用 system_clock 版本；
 * - 构造即开始，没有 Start/Stop；重置用 Reset()；
 * - `Remaining()` 有符号：正数表示还剩，负数表示已超期；
 * - 值类型、可拷贝；Reset/Extend 是写操作，跨线程共享需自行同步。
 *
 * @tparam Clock 时钟类型，需提供 now() 与 time_point
 */
template <typename Clock = std::chrono::steady_clock>
class DeadlineTimer final {
public:
    using ClockType = Clock;
    using TimePoint = std::chrono::time_point<Clock, Nanoseconds>;

    DeadlineTimer() = delete;

    /**
     * @brief 以当前时刻为起点，预算为 budget
     */
    template <typename Rep, typename Period>
    explicit DeadlineTimer( std::chrono::duration<Rep, Period> budget ) noexcept :
        start_( Now() ), budget_( detail::BudgetToNanoseconds( budget ) ) {}

    /**
     * @brief 显式指定起点与预算
     */
    template <typename Rep, typename Period>
    DeadlineTimer( TimePoint start, std::chrono::duration<Rep, Period> budget ) noexcept :
        start_( start ), budget_( detail::BudgetToNanoseconds( budget ) ) {}

    /**
     * @brief 以当前时刻为起点，budget 之后到期
     */
    template <typename Rep, typename Period>
    [[nodiscard]] static DeadlineTimer After( std::chrono::duration<Rep, Period> budget ) noexcept {
        return DeadlineTimer( budget );
    }

    /**
     * @brief 以当前时刻为起点，在 expires_at 到期
     */
    [[nodiscard]] static DeadlineTimer Until( TimePoint expires_at ) noexcept {
        const TimePoint now = Now();
        return DeadlineTimer( now, expires_at - now );
    }

    /**
     * @brief 完整重开：起点 = 当前时刻，截止点 = 当前时刻 + 当前预算
     */
    void Reset() noexcept { start_ = Now(); }

    /**
     * @brief 起点 = 当前时刻，使用新预算
     */
    template <typename Rep, typename Period>
    void Reset( std::chrono::duration<Rep, Period> budget ) noexcept {
        start_  = Now();
        budget_ = detail::BudgetToNanoseconds( budget );
    }

    /**
     * @brief 截止点 = 指定时间点（起点取当前时刻，预算自动变为 expires_at - now）
     */
    void Reset( TimePoint expires_at ) noexcept {
        start_  = Now();
        budget_ = expires_at - start_;
    }

    /**
     * @brief 显式指定起点与预算
     */
    template <typename Rep, typename Period>
    void Reset( TimePoint start, std::chrono::duration<Rep, Period> budget ) noexcept {
        start_  = start;
        budget_ = detail::BudgetToNanoseconds( budget );
    }

    /**
     * @brief 起点
     */
    [[nodiscard]] TimePoint StartedAt() const noexcept { return start_; }

    /**
     * @brief 截止点
     */
    [[nodiscard]] TimePoint ExpiresAt() const noexcept { return start_ + budget_; }

    /**
     * @brief 当前时钟时间点
     */
    [[nodiscard]] static TimePoint Now() noexcept { return std::chrono::time_point_cast<Nanoseconds>( Clock::now() ); }

    /**
     * @brief 预算
     */
    [[nodiscard]] Nanoseconds Budget() const noexcept { return budget_; }

    /**
     * @brief 从起点到当前时刻经过的时长
     */
    [[nodiscard]] Nanoseconds Elapsed() const noexcept { return ElapsedAt( Now() ); }

    /**
     * @brief 从起点到指定时刻经过的时长
     */
    [[nodiscard]] Nanoseconds ElapsedAt( TimePoint now ) const noexcept {
        return std::chrono::duration_cast<Nanoseconds>( now - start_ );
    }

    /**
     * @brief 距截止点还剩多久；负数表示已超期
     */
    [[nodiscard]] Nanoseconds Remaining() const noexcept { return RemainingAt( Now() ); }

    /**
     * @brief 距截止点还剩多久；负数表示已超期
     */
    [[nodiscard]] Nanoseconds RemainingAt( TimePoint now ) const noexcept {
        return std::chrono::duration_cast<Nanoseconds>( ExpiresAt() - now );
    }

    /**
     * @brief 是否已到期（now >= 截止点）
     */
    [[nodiscard]] bool IsExpired() const noexcept { return IsExpiredAt( Now() ); }

    /**
     * @brief 在指定时刻是否已到期（now >= 截止点）
     */
    [[nodiscard]] bool IsExpiredAt( TimePoint now ) const noexcept { return now >= ExpiresAt(); }

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
     * @brief 剩余毫秒数（默认 int64_t；负数表示已超期）
     */
    template <typename T = std::int64_t>
    [[nodiscard]] T RemainingMillis() const noexcept {
        return RemainingAs<Milliseconds, T>();
    }

    /**
     * @brief 剩余秒数（默认 int64_t；负数表示已超期）
     */
    template <typename T = std::int64_t>
    [[nodiscard]] T RemainingSeconds() const noexcept {
        return RemainingAs<Seconds, T>();
    }

    /**
     * @brief 以任意时间单位返回经过时长
     */
    template <typename Unit, typename T = std::int64_t>
    [[nodiscard]] T ElapsedAs() const noexcept {
        static_assert( detail::IsDuration_v<Unit>, "Unit 必须是 std::chrono::duration 类型" );
        return detail::ConvertDuration<Unit, T>( Elapsed() );
    }

    /**
     * @brief 以任意时间单位返回剩余时长（负数表示已超期）
     */
    template <typename Unit, typename T = std::int64_t>
    [[nodiscard]] T RemainingAs() const noexcept {
        static_assert( detail::IsDuration_v<Unit>, "Unit 必须是 std::chrono::duration 类型" );
        return detail::ConvertDuration<Unit, T>( Remaining() );
    }

    // ---- 以下接口仅 system_clock（或 Unix 纪元自定义时钟）可用 ----

    /**
     * @brief 截止点的 Unix 毫秒时间戳（向下取整）
     */
    [[nodiscard]] std::int64_t ExpiresAtUnixMillis() const {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixMillis( ExpiresAt() );
    }

    /**
     * @brief 截止点的 Unix 秒时间戳（向下取整）
     */
    [[nodiscard]] std::int64_t ExpiresAtUnixSeconds() const {
        static_assert( IsUnixEpochClock_v<Clock>, "该时钟没有 Unix 纪元，无法给出时间戳；请使用 system_clock" );
        return detail::ToUnixSeconds( ExpiresAt() );
    }

    /**
     * @brief 截止点对应的日期时间
     */
    [[nodiscard]] DateTime ExpiresAtDateTime( const TimeZone &zone = TimeZone::Local() ) const {
        return detail::ToDateTime( ExpiresAt(), zone );
    }

    /**
     * @brief 已用比例，裁剪到 [0, 1]；预算非正时视为已完成
     */
    [[nodiscard]] double Progress() const noexcept { return ProgressAt( Now() ); }

    /**
     * @brief 在指定时刻的已用比例，裁剪到 [0, 1]
     */
    [[nodiscard]] double ProgressAt( TimePoint now ) const noexcept {
        if ( budget_ <= Nanoseconds::zero() ) {
            return 1.0;
        }
        const Nanoseconds elapsed = ElapsedAt( now );
        if ( elapsed <= Nanoseconds::zero() ) {
            return 0.0;
        }
        const double ratio =
            std::chrono::duration<double>( elapsed ).count() / std::chrono::duration<double>( budget_ ).count();
        return ratio >= 1.0 ? 1.0 : ratio;
    }

    /**
     * @brief 延长（delta > 0）或缩短（delta < 0）预算
     */
    template <typename Rep, typename Period>
    void Extend( std::chrono::duration<Rep, Period> delta ) noexcept {
        budget_ += std::chrono::duration_cast<Nanoseconds>( delta );
    }

    /**
     * @brief 取更早到期的那个（预算嵌套时使用）
     */
    [[nodiscard]] static DeadlineTimer Earliest( const DeadlineTimer &a, const DeadlineTimer &b ) noexcept {
        return a.ExpiresAt() <= b.ExpiresAt() ? a : b;
    }

private:
    TimePoint   start_;
    Nanoseconds budget_;
};

using SteadyDeadlineTimer = DeadlineTimer<std::chrono::steady_clock>;
using SystemDeadlineTimer = DeadlineTimer<std::chrono::system_clock>;

}  // namespace utils
