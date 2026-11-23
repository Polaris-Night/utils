#include <chrono>
#include <type_traits>

#include "Clock.h"
#include "DateTime.h"
#include "ElapsedTimer.h"
#include "TestClock.h"
#include "gtest/gtest.h"

namespace {

using namespace utils;
using namespace std::chrono_literals;

/**
 * @brief 验证 ClockTraits 扩展点：自定义时钟也能解锁时间戳接口
 */
class UnixEpochClock final {
public:
    using rep        = std::int64_t;
    using period     = std::nano;
    using duration   = Nanoseconds;
    using time_point = std::chrono::time_point<UnixEpochClock, Nanoseconds>;

    static time_point now() noexcept { return time_point( Nanoseconds::zero() ); }
};

}  // namespace

template <>
struct utils::ClockTraits<UnixEpochClock> {
    static constexpr bool unix_epoch = true;
};

namespace {

static_assert( !IsUnixEpochClock_v<std::chrono::steady_clock> );
static_assert( IsUnixEpochClock_v<std::chrono::system_clock> );
static_assert( !IsUnixEpochClock_v<test::TestClock> );
static_assert( IsUnixEpochClock_v<UnixEpochClock> );

static_assert( detail::IsDuration_v<Milliseconds> );
static_assert( !detail::IsDuration_v<int> );

TEST( ClockTest, CustomUnixEpochClockEnablesTimestamp ) {
    using Timer = ElapsedTimer<UnixEpochClock>;
    using TP    = Timer::TimePoint;

    const Timer timer( TP( 1500ms ) );

    EXPECT_EQ( timer.StartedAtUnixMillis(), 1500 );
    EXPECT_EQ( timer.StartedAtUnixSeconds(), 1 );
    EXPECT_EQ( Timer::ToUnixMillis( TP( -1500ms ) ), -1500 );
    EXPECT_EQ( Timer::ToUnixSeconds( TP( -1500ms ) ), -2 );
    EXPECT_EQ( timer.StartedAtDateTime( TimeZone::Utc() ).ToUnixMillis(), 1500 );
    EXPECT_EQ( Timer::ToDateTime( TP( 1500ms ), TimeZone::Utc() ).ToUnixMillis(), 1500 );
}

TEST( ClockTest, DetailHelpers ) {
    EXPECT_EQ( ( detail::ConvertDuration<Milliseconds, int>( 1500ms ) ), 1500 );
    EXPECT_DOUBLE_EQ( ( detail::ConvertDuration<Milliseconds, double>( 1500ms ) ), 1500.0 );
    EXPECT_EQ( detail::BudgetToNanoseconds( std::chrono::duration<double, std::nano>( 0.5 ) ), 1ns );
    EXPECT_EQ( detail::BudgetToNanoseconds( 1500us ), 1500us );
    EXPECT_EQ( detail::ToUnixMillis( std::chrono::time_point<std::chrono::system_clock, Nanoseconds>( -1500ms ) ),
               -1500 );
    EXPECT_EQ( detail::ToUnixSeconds( std::chrono::time_point<std::chrono::system_clock, Nanoseconds>( -1500ms ) ),
               -2 );
}

TEST( ClockTest, TestClockStateAndScope ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    EXPECT_EQ( test::TestClock::Current(), 0ns );
    EXPECT_EQ( test::TestClock::now(), test::TestClock::time_point( 0ns ) );

    test::TestClock::Set( 100ms );
    EXPECT_EQ( test::TestClock::Current(), 100ms );

    test::TestClock::Advance( 50ms );
    EXPECT_EQ( test::TestClock::now(), test::TestClock::time_point( 150ms ) );

    {
        test::TestClock::Scope inner;
        test::TestClock::Advance( 1s );
        EXPECT_EQ( test::TestClock::Current(), 1150ms );
    }
    EXPECT_EQ( test::TestClock::Current(), 150ms );
}

}  // namespace
