#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "DateTime.h"

namespace utils {
namespace test {

/**
 * @brief 仅供测试使用的可手动推进时钟
 *
 * 不随库安装，也不属于公共 API。它满足 std::chrono 时钟的基本形状，
 * 可注入 ElapsedTimer / DeadlineTimer，用于确定性地验证“取当前时刻”的路径。
 */
class TestClock final {
public:
    using rep        = std::int64_t;
    using period     = std::nano;
    using duration   = Nanoseconds;
    using time_point = std::chrono::time_point<TestClock, Nanoseconds>;

    static constexpr bool is_steady = true;

    [[nodiscard]] static time_point now() noexcept {
        return time_point( Nanoseconds( current_.load( std::memory_order_relaxed ) ) );
    }

    [[nodiscard]] static Nanoseconds Current() noexcept {
        return Nanoseconds( current_.load( std::memory_order_relaxed ) );
    }

    static void Set( Nanoseconds value ) noexcept { current_.store( value.count(), std::memory_order_relaxed ); }

    static void Advance( Nanoseconds delta ) noexcept {
        current_.fetch_add( delta.count(), std::memory_order_relaxed );
    }

    static void Reset() noexcept { Set( Nanoseconds::zero() ); }

    /**
     * @brief RAII 保存/恢复当前时刻，隔离各测试用例
     */
    class Scope final {
    public:
        Scope() noexcept : saved_( Current() ) {}
        ~Scope() { Set( saved_ ); }

        Scope( const Scope & )            = delete;
        Scope &operator=( const Scope & ) = delete;
        Scope( Scope && )                 = delete;
        Scope &operator=( Scope && )      = delete;

    private:
        Nanoseconds saved_;
    };

private:
    static inline std::atomic<std::int64_t> current_{ 0 };
};

}  // namespace test
}  // namespace utils
