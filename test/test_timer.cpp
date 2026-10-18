#include <chrono>
#include <thread>
#include "Timer.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {
// 调度抖动容忍上限（毫秒）
constexpr int64_t kToleranceMs = 200;
}  // namespace

TEST( ElapsedTimerTest, NotStartedIsZero ) {
    ElapsedTimer timer;
    EXPECT_EQ( timer.GetElapsed<Milliseconds>(), 0 );
}

TEST( ElapsedTimerTest, StartAndElapsed ) {
    ElapsedTimer timer;
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

    auto elapsed = timer.GetElapsed<Milliseconds>();
    EXPECT_GE( elapsed, 40 );
    EXPECT_LE( elapsed, 50 + kToleranceMs );
}

TEST( ElapsedTimerTest, StopWithReset ) {
    ElapsedTimer timer;
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    timer.Stop( true );

    // 回归测试：Stop(reset=true) 后必须清零
    EXPECT_EQ( timer.GetElapsed<Milliseconds>(), 0 );
}

TEST( ElapsedTimerTest, StopWithoutResetFreezes ) {
    ElapsedTimer timer;
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
    timer.Stop( false );

    auto first = timer.GetElapsed<Milliseconds>();
    EXPECT_GE( first, 20 );

    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    // 回归测试：Stop(reset=false) 后计时必须冻结，不再继续增长
    EXPECT_EQ( timer.GetElapsed<Milliseconds>(), first );
}

TEST( ElapsedTimerTest, PauseAndResume ) {
    ElapsedTimer timer;
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
    timer.Pause();

    auto paused = timer.GetElapsed<Milliseconds>();
    EXPECT_GE( paused, 10 );

    std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
    EXPECT_EQ( timer.GetElapsed<Milliseconds>(), paused );  // 暂停期间不增长

    timer.Resume();
    std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
    EXPECT_GT( timer.GetElapsed<Milliseconds>(), paused );
}

TEST( ElapsedTimerTest, Restart ) {
    ElapsedTimer timer;
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 30 ) );
    timer.Restart();

    EXPECT_LT( timer.GetElapsed<Milliseconds>(), 20 );
}

TEST( ElapsedTimerTest, FloatingSeconds ) {
    ElapsedTimer timer;
    EXPECT_DOUBLE_EQ( ( timer.GetElapsed<Seconds, double>() ), 0.0 );

    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 60 ) );

    auto elapsed = timer.GetElapsed<Seconds, double>();
    EXPECT_GT( elapsed, 0.03 );
    EXPECT_LT( elapsed, 1.0 );
}

TEST( TimerTest, StartAndTimeout ) {
    Timer timer( 40 );
    EXPECT_FALSE( timer.IsRunning() );
    EXPECT_FALSE( timer.IsTimeout() );

    timer.Start();
    EXPECT_TRUE( timer.IsRunning() );
    EXPECT_FALSE( timer.IsTimeout() );

    std::this_thread::sleep_for( std::chrono::milliseconds( 80 ) );
    EXPECT_TRUE( timer.IsTimeout() );
    EXPECT_TRUE( timer.IsRunning() );
}

TEST( TimerTest, Restart ) {
    Timer timer( 20 );
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    EXPECT_TRUE( timer.IsTimeout() );

    timer.Restart();
    EXPECT_TRUE( timer.IsRunning() );
    EXPECT_FALSE( timer.IsTimeout() );
}

TEST( TimerTest, Stop ) {
    Timer timer( 20 );
    timer.Start();
    std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
    timer.Stop();

    EXPECT_FALSE( timer.IsRunning() );
    EXPECT_FALSE( timer.IsTimeout() );
}

TEST( TimerTest, SetInterval ) {
    Timer timer( 10000 );
    timer.Start();
    EXPECT_FALSE( timer.IsTimeout() );

    timer.SetInterval( 10 );
    std::this_thread::sleep_for( std::chrono::milliseconds( 40 ) );
    EXPECT_TRUE( timer.IsTimeout() );
}
