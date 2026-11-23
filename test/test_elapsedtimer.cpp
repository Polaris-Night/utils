#include <chrono>
#include <thread>

#include "DateTime.h"
#include "ElapsedTimer.h"
#include "TestClock.h"
#include "gtest/gtest.h"

namespace {

using namespace utils;
using namespace std::chrono_literals;

using TestElapsedTimer = ElapsedTimer<test::TestClock>;

using ElapsedTimerTypes = ::testing::Types<SteadyElapsedTimer, SystemElapsedTimer>;

template <typename>
class ElapsedTimerTypedTest : public ::testing::Test {};

TYPED_TEST_SUITE( ElapsedTimerTypedTest, ElapsedTimerTypes );

TYPED_TEST( ElapsedTimerTypedTest, ElapsedFromExplicitStart ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 100ms ) );

    EXPECT_EQ( timer.StartedAt(), TP( 100ms ) );
    EXPECT_EQ( timer.ElapsedAt( TP( 350ms ) ), 250ms );
    EXPECT_EQ( timer.ElapsedAt( TP( 100ms ) ), 0ms );
    EXPECT_EQ( timer.ElapsedAt( TP( 50ms ) ), -50ms );
}

TYPED_TEST( ElapsedTimerTypedTest, ResetToTimePoint ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 500ms ) );
    timer.Reset( TP( 400ms ) );

    EXPECT_EQ( timer.StartedAt(), TP( 400ms ) );
    EXPECT_EQ( timer.ElapsedAt( TP( 550ms ) ), 150ms );
}

TYPED_TEST( ElapsedTimerTypedTest, NegativeElapsedOnFutureStart ) {
    using Timer = TypeParam;
    using TP    = typename Timer::TimePoint;

    Timer timer( TP( 300ms ) );

    EXPECT_EQ( timer.ElapsedAt( TP( 100ms ) ), -200ms );
}

template <typename Timer>
void ExpectDefaultConstructorCapturesNow() {
    const auto before = Timer::Now();
    Timer      timer;
    const auto after = Timer::Now();

    EXPECT_GE( timer.StartedAt(), before );
    EXPECT_LE( timer.StartedAt(), after );
}

}  // namespace

TEST( ElapsedTimerTest, DefaultConstructorUsesClockNow ) {
    ExpectDefaultConstructorCapturesNow<SteadyElapsedTimer>();
    ExpectDefaultConstructorCapturesNow<SystemElapsedTimer>();
}

TEST( ElapsedTimerTest, TestClockDefaultNowAndElapsed ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();
    test::TestClock::Advance( 100ms );

    TestElapsedTimer timer;
    EXPECT_EQ( timer.StartedAt(), TestElapsedTimer::TimePoint( 100ms ) );

    test::TestClock::Advance( 250ms );
    EXPECT_EQ( timer.Elapsed(), 250ms );
    EXPECT_EQ( timer.ElapsedMillis(), 250 );
    EXPECT_DOUBLE_EQ( timer.ElapsedSeconds<double>(), 0.25 );
}

TEST( ElapsedTimerTest, ResetToNowUsesClockNow ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestElapsedTimer timer;
    test::TestClock::Advance( 80ms );
    EXPECT_EQ( timer.Elapsed(), 80ms );

    timer.Reset();
    EXPECT_EQ( timer.StartedAt(), TestElapsedTimer::TimePoint( 80ms ) );
    EXPECT_EQ( timer.Elapsed(), 0ms );

    test::TestClock::Advance( 20ms );
    EXPECT_EQ( timer.Elapsed(), 20ms );
}

TEST( ElapsedTimerTest, ArbitraryUnits ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestElapsedTimer timer;
    test::TestClock::Advance( 3725ms );

    EXPECT_EQ( timer.Elapsed(), 3725ms );
    EXPECT_EQ( timer.ElapsedMillis(), 3725 );
    EXPECT_EQ( timer.ElapsedSeconds(), 3 );
    EXPECT_EQ( timer.ElapsedAs<Microseconds>(), 3725000 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Seconds, double>() ), 3.725 );
}

TEST( ElapsedTimerTest, TemplateUnitConversionMatrix ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    TestElapsedTimer timer;
    test::TestClock::Advance( 3725ms );

    EXPECT_EQ( timer.ElapsedAs<Nanoseconds>(), 3725000000LL );
    EXPECT_EQ( timer.ElapsedAs<Microseconds>(), 3725000 );
    EXPECT_EQ( timer.ElapsedAs<Milliseconds>(), 3725 );
    EXPECT_EQ( timer.ElapsedAs<Seconds>(), 3 );
    EXPECT_EQ( timer.ElapsedAs<Minutes>(), 0 );
    EXPECT_EQ( timer.ElapsedAs<Hours>(), 0 );
    EXPECT_DOUBLE_EQ( timer.ElapsedMillis<double>(), 3725.0 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Milliseconds, double>() ), 3725.0 );
    EXPECT_DOUBLE_EQ( ( timer.ElapsedAs<Minutes, double>() ), 3725.0 / 60000.0 );
}

TEST( ElapsedTimerTest, NegativeDurationConversion ) {
    test::TestClock::Scope scope;
    test::TestClock::Reset();

    const TestElapsedTimer timer( TestElapsedTimer::TimePoint( 1000ms ) );

    EXPECT_EQ( timer.ElapsedMillis(), -1000 );
    EXPECT_EQ( timer.ElapsedAs<Seconds>(), -1 );
    EXPECT_DOUBLE_EQ( timer.ElapsedSeconds<double>(), -1.0 );
}

TEST( ElapsedTimerTest, SystemClockTimestamp ) {
    const auto         start = SystemElapsedTimer::TimePoint( Milliseconds( 1700000000123 ) );
    SystemElapsedTimer timer( start );

    EXPECT_EQ( timer.StartedAtUnixMillis(), 1700000000123 );
    EXPECT_EQ( timer.StartedAtUnixSeconds(), 1700000000 );
    EXPECT_EQ( SystemElapsedTimer::ToUnixMillis( start ), 1700000000123 );
    EXPECT_EQ( SystemElapsedTimer::ToUnixSeconds( start ), 1700000000 );

    const auto dt = timer.StartedAtDateTime( TimeZone::Utc() );
    EXPECT_EQ( dt.ToUnixMillis(), 1700000000123 );
    EXPECT_EQ( SystemElapsedTimer::ToDateTime( start, TimeZone::Utc() ).ToUnixMillis(), 1700000000123 );
}

TEST( ElapsedTimerTest, TimestampFloorBeforeEpoch ) {
    const auto               before_epoch = SystemElapsedTimer::TimePoint( Milliseconds( -1500 ) );
    const SystemElapsedTimer timer( before_epoch );

    EXPECT_EQ( SystemElapsedTimer::ToUnixMillis( before_epoch ), -1500 );
    EXPECT_EQ( SystemElapsedTimer::ToUnixSeconds( before_epoch ), -2 );
    EXPECT_EQ( timer.StartedAtUnixMillis(), -1500 );
    EXPECT_EQ( timer.StartedAtUnixSeconds(), -2 );
}

TEST( ElapsedTimerTest, SteadyClockMeasuresRealTime ) {
    SteadyElapsedTimer timer;
    std::this_thread::sleep_for( 10ms );

    EXPECT_GE( timer.ElapsedMillis(), 5 );
}

TEST( ElapsedTimerTest, SystemClockMeasuresRealTime ) {
    SystemElapsedTimer timer;
    std::this_thread::sleep_for( 10ms );

    EXPECT_GE( timer.ElapsedMillis(), 5 );
}
