#include <chrono>
#include <type_traits>

#include "DateTime.h"
#include "DeadlineTimer.h"
#include "TestClock.h"
#include "gtest/gtest.h"

namespace {

using namespace utils;
using namespace std::chrono_literals;

using TestDeadlineTimer = DeadlineTimer<test::TestClock>;

static_assert( !std::is_default_constructible_v<SteadyDeadlineTimer> );
static_assert( !std::is_default_constructible_v<SystemDeadlineTimer> );
static_assert( std::is_copy_constructible_v<SteadyDeadlineTimer> );
static_assert( std::is_copy_assignable_v<SystemDeadlineTimer> );

using DeadlineTimerTypes = ::testing::Types<SteadyDeadlineTimer, SystemDeadlineTimer>;

template <typename>
class DeadlineTimerTypedTest : public ::testing::Test {};

TYPED_TEST_SUITE( DeadlineTimerTypedTest, DeadlineTimerTypes );

TYPED_TEST( DeadlineTimerTypedTest, ConstructAndQuery ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), 500ms );

    EXPECT_EQ( timer.StartedAt(), TP( 0ms ) );
    EXPECT_EQ( timer.ExpiresAt(), TP( 500ms ) );
    EXPECT_EQ( timer.Budget(), 500ms );
    EXPECT_EQ( timer.ElapsedAt( TP( 0ms ) ), 0ms );
    EXPECT_EQ( timer.RemainingAt( TP( 0ms ) ), 500ms );
    EXPECT_FALSE( timer.IsExpiredAt( TP( 0ms ) ) );
    EXPECT_DOUBLE_EQ( timer.ProgressAt( TP( 0ms ) ), 0.0 );
}

TYPED_TEST( DeadlineTimerTypedTest, ElapsedAndRemaining ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer      timer( TP( 0ms ), 500ms );
    const auto snapshot = TP( 200ms );

    EXPECT_EQ( timer.ElapsedAt( snapshot ), 200ms );
    EXPECT_EQ( timer.RemainingAt( snapshot ), 300ms );
    EXPECT_DOUBLE_EQ( timer.ProgressAt( snapshot ), 0.4 );
    EXPECT_FALSE( timer.IsExpiredAt( snapshot ) );
}

TYPED_TEST( DeadlineTimerTypedTest, ExactBoundaryIsExpired ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), 500ms );

    EXPECT_TRUE( timer.IsExpiredAt( timer.ExpiresAt() ) );
    EXPECT_EQ( timer.RemainingAt( timer.ExpiresAt() ), 0ms );
    EXPECT_DOUBLE_EQ( timer.ProgressAt( timer.ExpiresAt() ), 1.0 );
}

TYPED_TEST( DeadlineTimerTypedTest, RemainingIsSignedAfterExpiry ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer      timer( TP( 0ms ), 500ms );
    const auto snapshot = TP( 525ms );

    EXPECT_TRUE( timer.IsExpiredAt( snapshot ) );
    EXPECT_EQ( timer.RemainingAt( snapshot ), -25ms );
    EXPECT_EQ( timer.ElapsedAt( snapshot ), 525ms );
    EXPECT_DOUBLE_EQ( timer.ProgressAt( snapshot ), 1.0 );
}

TYPED_TEST( DeadlineTimerTypedTest, ResetWithStartAndBudget ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), 500ms );
    timer.Reset( TP( 600ms ), 50ms );

    EXPECT_EQ( timer.StartedAt(), TP( 600ms ) );
    EXPECT_EQ( timer.ExpiresAt(), TP( 650ms ) );
    EXPECT_EQ( timer.ElapsedAt( TP( 625ms ) ), 25ms );
    EXPECT_EQ( timer.RemainingAt( TP( 625ms ) ), 25ms );
}

TYPED_TEST( DeadlineTimerTypedTest, ExtendAndShorten ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), 100ms );

    timer.Extend( 50ms );
    EXPECT_EQ( timer.Budget(), 150ms );
    EXPECT_EQ( timer.RemainingAt( TP( 40ms ) ), 110ms );

    timer.Extend( -200ms );
    EXPECT_EQ( timer.Budget(), -50ms );
    EXPECT_TRUE( timer.IsExpiredAt( TP( 40ms ) ) );
    EXPECT_EQ( timer.RemainingAt( TP( 40ms ) ), -90ms );
}

TYPED_TEST( DeadlineTimerTypedTest, SubNanosecondBudgetRoundsUp ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), std::chrono::duration<double, std::nano>( 0.5 ) );

    EXPECT_EQ( timer.Budget(), 1ns );
    EXPECT_FALSE( timer.IsExpiredAt( TP( 0ms ) ) );
}

TYPED_TEST( DeadlineTimerTypedTest, ZeroBudgetIsExpiredImmediately ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 0ms ), 0ms );

    EXPECT_EQ( timer.Budget(), 0ms );
    EXPECT_TRUE( timer.IsExpiredAt( TP( 0ms ) ) );
    EXPECT_DOUBLE_EQ( timer.ProgressAt( TP( 0ms ) ), 1.0 );
}

TYPED_TEST( DeadlineTimerTypedTest, EarliestPicksTightest ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    const auto later   = Timer::Until( TP( 100ms ) );
    const auto earlier = Timer::Until( TP( 50ms ) );

    EXPECT_EQ( later.ExpiresAt(), TP( 100ms ) );
    EXPECT_EQ( earlier.ExpiresAt(), TP( 50ms ) );

    const auto tightest = Timer::Earliest( later, earlier );
    EXPECT_EQ( tightest.ExpiresAt(), TP( 50ms ) );
}

template <typename Timer>
void ExpectDefaultConstructionUsesNow() {
    const auto before = Timer::Now();
    Timer      timer( 500ms );
    const auto after = Timer::Now();

    EXPECT_GE( timer.StartedAt(), before );
    EXPECT_LE( timer.StartedAt(), after );
    EXPECT_EQ( timer.ExpiresAt(), timer.StartedAt() + 500ms );
    EXPECT_EQ( timer.Budget(), 500ms );
}

template <typename Timer>
void ExpectAfterUsesNow() {
    const auto before = Timer::Now();
    const auto timer  = Timer::After( 500ms );
    const auto after  = Timer::Now();

    EXPECT_GE( timer.StartedAt(), before );
    EXPECT_LE( timer.StartedAt(), after );
    EXPECT_EQ( timer.ExpiresAt(), timer.StartedAt() + 500ms );
}

template <typename Timer>
void ExpectUntilUsesNow() {
    using TP = typename Timer::TimePoint;

    const auto expires_at = TP( 1700000000000ms );
    const auto before     = Timer::Now();
    const auto timer      = Timer::Until( expires_at );
    const auto after      = Timer::Now();

    EXPECT_GE( timer.StartedAt(), before );
    EXPECT_LE( timer.StartedAt(), after );
    EXPECT_EQ( timer.ExpiresAt(), expires_at );
    EXPECT_EQ( timer.Budget(), expires_at - timer.StartedAt() );
}

}  // namespace

TEST( DeadlineTimerTest, DefaultConstructionUsesClockNow ) {
    ExpectDefaultConstructionUsesNow<SteadyDeadlineTimer>();
    ExpectDefaultConstructionUsesNow<SystemDeadlineTimer>();
}

TEST( DeadlineTimerTest, AfterUsesClockNow ) {
    ExpectAfterUsesNow<SteadyDeadlineTimer>();
    ExpectAfterUsesNow<SystemDeadlineTimer>();
}

TEST( DeadlineTimerTest, UntilUsesClockNow ) {
    ExpectUntilUsesNow<SteadyDeadlineTimer>();
    ExpectUntilUsesNow<SystemDeadlineTimer>();
}

TEST( DeadlineTimerTest, TestClockConstructAndQuery ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();
    test::TestClock::Advance( 100ms );

    TestDeadlineTimer timer( 500ms );

    EXPECT_EQ( timer.StartedAt(), TestDeadlineTimer::TimePoint( 100ms ) );
    EXPECT_EQ( timer.ExpiresAt(), TestDeadlineTimer::TimePoint( 600ms ) );
    EXPECT_EQ( timer.Budget(), 500ms );
    EXPECT_EQ( timer.Elapsed(), 0ms );
    EXPECT_EQ( timer.Remaining(), 500ms );
    EXPECT_FALSE( timer.IsExpired() );
    EXPECT_DOUBLE_EQ( timer.Progress(), 0.0 );
}

TEST( DeadlineTimerTest, TestClockElapsedRemainingAndProgress ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 500ms );
    test::TestClock::Advance( 200ms );

    EXPECT_EQ( timer.Elapsed(), 200ms );
    EXPECT_EQ( timer.Remaining(), 300ms );
    EXPECT_EQ( timer.RemainingMillis(), 300 );
    EXPECT_DOUBLE_EQ( timer.Progress(), 0.4 );
    EXPECT_FALSE( timer.IsExpired() );
}

TEST( DeadlineTimerTest, ResetReopensFullBudget ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 500ms );
    test::TestClock::Advance( 525ms );
    EXPECT_TRUE( timer.IsExpired() );

    timer.Reset();
    EXPECT_EQ( timer.StartedAt(), TestDeadlineTimer::TimePoint( 525ms ) );
    EXPECT_EQ( timer.ExpiresAt(), TestDeadlineTimer::TimePoint( 1025ms ) );
    EXPECT_EQ( timer.Remaining(), 500ms );
    EXPECT_FALSE( timer.IsExpired() );
}

TEST( DeadlineTimerTest, ResetWithBudget ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();
    test::TestClock::Advance( 100ms );

    TestDeadlineTimer timer( 500ms );
    timer.Reset( 2s );

    EXPECT_EQ( timer.StartedAt(), TestDeadlineTimer::TimePoint( 100ms ) );
    EXPECT_EQ( timer.Budget(), 2s );
    EXPECT_EQ( timer.ExpiresAt(), TestDeadlineTimer::TimePoint( 2100ms ) );

    test::TestClock::Advance( 50ms );
    EXPECT_EQ( timer.Remaining(), 1950ms );
}

TEST( DeadlineTimerTest, ResetWithExpiresAt ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();
    test::TestClock::Advance( 625ms );

    TestDeadlineTimer timer( 500ms );
    timer.Reset( TestDeadlineTimer::TimePoint( 1000ms ) );

    EXPECT_EQ( timer.StartedAt(), TestDeadlineTimer::TimePoint( 625ms ) );
    EXPECT_EQ( timer.ExpiresAt(), TestDeadlineTimer::TimePoint( 1000ms ) );
    EXPECT_EQ( timer.Budget(), 375ms );
    EXPECT_EQ( timer.Remaining(), 375ms );
}

TEST( DeadlineTimerTest, AfterUntilAndEarliest ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    const auto later   = TestDeadlineTimer::After( 100ms );
    const auto earlier = TestDeadlineTimer::Until( TestDeadlineTimer::TimePoint( 50ms ) );

    EXPECT_EQ( later.ExpiresAt(), TestDeadlineTimer::TimePoint( 100ms ) );
    EXPECT_EQ( earlier.ExpiresAt(), TestDeadlineTimer::TimePoint( 50ms ) );
    EXPECT_EQ( earlier.Budget(), 50ms );

    const auto tightest = TestDeadlineTimer::Earliest( later, earlier );
    EXPECT_EQ( tightest.ExpiresAt(), TestDeadlineTimer::TimePoint( 50ms ) );
}

TEST( DeadlineTimerTest, UnitConversions ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 5000ms );
    test::TestClock::Advance( 3725ms );

    EXPECT_EQ( timer.ElapsedMillis(), 3725 );
    EXPECT_EQ( timer.ElapsedSeconds(), 3 );
    EXPECT_EQ( timer.ElapsedAs<Microseconds>(), 3725000 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Seconds, double>() ), 3.725 );

    EXPECT_EQ( timer.RemainingMillis(), 1275 );
    EXPECT_EQ( timer.RemainingSeconds(), 1 );
    EXPECT_EQ( timer.RemainingAs<Microseconds>(), 1275000 );
    EXPECT_DOUBLE_EQ( ( timer.RemainingAs<Seconds, double>() ), 1.275 );
}

TEST( DeadlineTimerTest, TemplateUnitConversionMatrix ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 5000ms );
    test::TestClock::Advance( 3725ms );

    EXPECT_EQ( timer.RemainingAs<Nanoseconds>(), 1275000000LL );
    EXPECT_EQ( timer.RemainingAs<Microseconds>(), 1275000 );
    EXPECT_EQ( timer.RemainingAs<Milliseconds>(), 1275 );
    EXPECT_EQ( timer.RemainingAs<Seconds>(), 1 );
    EXPECT_EQ( timer.RemainingAs<Minutes>(), 0 );

    EXPECT_EQ( timer.ElapsedAs<Nanoseconds>(), 3725000000LL );
    EXPECT_EQ( timer.ElapsedAs<Microseconds>(), 3725000 );
    EXPECT_EQ( timer.ElapsedAs<Minutes>(), 0 );
    EXPECT_DOUBLE_EQ( timer.ElapsedMillis<double>(), 3725.0 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Milliseconds, double>() ), 3725.0 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Minutes, double>() ), 3725.0 / 60000.0 );

    EXPECT_DOUBLE_EQ( timer.RemainingMillis<double>(), 1275.0 );
    EXPECT_DOUBLE_EQ( timer.RemainingSeconds<double>(), 1.275 );
    EXPECT_DOUBLE_EQ( ( timer.RemainingAs<Milliseconds, double>() ), 1275.0 );
    EXPECT_DOUBLE_EQ( ( timer.RemainingAs<Minutes, double>() ), 1275.0 / 60000.0 );
}

TEST( DeadlineTimerTest, NegativeDurationConversion ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 500ms );
    test::TestClock::Advance( 800ms );

    EXPECT_EQ( timer.RemainingMillis(), -300 );
    EXPECT_EQ( timer.RemainingAs<Seconds>(), 0 );
    EXPECT_DOUBLE_EQ( timer.RemainingSeconds<double>(), -0.3 );
}

TEST( DeadlineTimerTest, BudgetAcceptsDifferentDurationTypes ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 2min );
    EXPECT_EQ( timer.Budget(), 120s );

    timer.Reset( 1h );
    EXPECT_EQ( timer.Budget(), 3600s );

    timer.Reset( std::chrono::duration<double, std::micro>( 1500.5 ) );
    EXPECT_EQ( timer.Budget(), 1500500ns );
}

TEST( DeadlineTimerTest, ProgressWithNegativeElapsedAndBudget ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestDeadlineTimer timer( 1000ms );

    test::TestClock::Set( -500ms );  // 墙钟回拨 / 起点在未来
    EXPECT_EQ( timer.Elapsed(), -500ms );
    EXPECT_DOUBLE_EQ( timer.Progress(), 0.0 );

    timer.Extend( -2000ms );  // 预算为负，视为已完成
    EXPECT_LT( timer.Budget(), 0ns );
    EXPECT_DOUBLE_EQ( timer.Progress(), 1.0 );
}

TEST( DeadlineTimerTest, UntilInThePastIsExpired ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();
    test::TestClock::Advance( 100ms );

    const auto timer = TestDeadlineTimer::Until( TestDeadlineTimer::TimePoint( 40ms ) );

    EXPECT_EQ( timer.Budget(), -60ms );
    EXPECT_LT( timer.Remaining(), 0ns );
    EXPECT_TRUE( timer.IsExpired() );
}

TEST( DeadlineTimerTest, SystemClockTimestamps ) {
    const auto          start = SystemDeadlineTimer::TimePoint( Milliseconds( 1700000000000 ) );
    SystemDeadlineTimer timer( start, 1500ms );

    EXPECT_EQ( timer.ExpiresAtUnixMillis(), 1700000001500 );
    EXPECT_EQ( timer.ExpiresAtUnixSeconds(), 1700000001 );

    const auto dt = timer.ExpiresAtDateTime( TimeZone::Utc() );
    EXPECT_EQ( dt.ToUnixMillis(), 1700000001500 );
}

TEST( DeadlineTimerTest, SystemTimestampFloorBeforeEpoch ) {
    const auto                start = SystemDeadlineTimer::TimePoint( Milliseconds( -1500 ) );
    const SystemDeadlineTimer timer( start, 0ms );

    EXPECT_EQ( timer.ExpiresAtUnixMillis(), -1500 );
    EXPECT_EQ( timer.ExpiresAtUnixSeconds(), -2 );
}
