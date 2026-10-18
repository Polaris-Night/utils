#include <chrono>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "TaskScheduler.h"
#include "ThreadPoolExecutor.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

using SteadyTime = TaskScheduler::SteadyTime;

const SteadyTime kSteady0{ std::chrono::seconds( 1000 ) };  // 假时间轴原点

Timestamp WallUtc( int year, int month, int day, int hour = 0, int minute = 0, int second = 0 ) {
    DateTimeParts parts;
    parts.year   = year;
    parts.month  = month;
    parts.day    = day;
    parts.hour   = hour;
    parts.minute = minute;
    parts.second = second;
    return DateTime::FromParts( parts, TimeZone::Utc() ).value().ToTimestamp();
}

// 手动执行器：收集提交不执行，用于测试在途/合并语义
class QueuedExecutor final : public TaskExecutor {
public:
    bool Submit( std::function<void()> job ) override {
        jobs.push_back( std::move( job ) );
        return true;
    }

    std::vector<std::function<void()>> jobs;
};

}  // namespace

// ============================ IntervalTrigger（steady 轴）============================

TEST( TaskSchedulerTest, IntervalFiresOnFixedGrid ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );  // 设定时间轴起点

    int  fire_count = 0;
    auto id         = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 5 ), kSteady0 ),
                                     [&]( const DateTime & ) { ++fire_count; } );
    ASSERT_TRUE( id.has_value() );
    EXPECT_EQ( scheduler.TaskCount(), 1u );

    // 未到点：不触发
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::milliseconds( 4999 ) );
    EXPECT_EQ( fire_count, 0 );

    // 到点：触发
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 5 ) );
    EXPECT_EQ( fire_count, 1 );

    // 下次触发 = 网格点 10s（锚点换算的墙钟）
    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 1 ) + std::chrono::seconds( 10 ) );

    // 石英钟推进：一次推进跨越两个网格点 → 只补跑一次（合并错过点），并重新对齐到 20s
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 15 ) );
    EXPECT_EQ( fire_count, 2 );

    const auto catch_up = scheduler.NextFireTime( *id );
    ASSERT_TRUE( catch_up.has_value() );
    EXPECT_EQ( catch_up->ToTimestamp(), WallUtc( 2024, 1, 1 ) + std::chrono::seconds( 20 ) );  // 网格不漂移
}

TEST( TaskSchedulerTest, LateBeyondGraceRunsOnceByDefault ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    int  fire_count = 0;
    auto id         = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 5 ), kSteady0 ),
                                     [&]( const DateTime & ) { ++fire_count; } );
    ASSERT_TRUE( id );

    // 模拟阻塞 17 秒：已错过 5s/10s/15s 三个点
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 17 ) );
    EXPECT_EQ( fire_count, 1 );  // CatchUp：只补跑一次

    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 1 ) + std::chrono::seconds( 20 ) );

    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 1u );
    EXPECT_EQ( stats->late_count, 1u );
    EXPECT_EQ( stats->skip_count, 0u );
}

TEST( TaskSchedulerTest, LateSkipPolicy ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    TaskPolicy policy;
    policy.late  = LatePolicy::Skip;
    policy.grace = std::chrono::seconds( 2 );

    int  fire_count = 0;
    auto id         = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 5 ), kSteady0 ),
                                     [&]( const DateTime & ) { ++fire_count; }, policy );
    ASSERT_TRUE( id );

    // 迟到 1 秒（容忍窗口内）：正常执行
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 6 ) );
    EXPECT_EQ( fire_count, 1 );

    // 迟到 6 秒（超过窗口）：跳过本次
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 16 ) );
    EXPECT_EQ( fire_count, 1 );  // 10s 的点被跳过

    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 1u );
    EXPECT_EQ( stats->skip_count, 1u );
    EXPECT_EQ( stats->late_count, 2u );  // 第一次迟到 1s 执行、第二次迟到 6s 被跳过

    // 下一次仍保持网格（20s）
    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 1 ) + std::chrono::seconds( 20 ) );
}

// ============================ AtTrigger / 生命周期 ============================

TEST( TaskSchedulerTest, OneShotFiresOnceAndCompletes ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    const auto fire_at  = WallUtc( 2024, 1, 1, 12, 0, 0 );
    int        fires    = 0;
    auto       id       = scheduler.Add( std::make_shared<AtTrigger>( DateTime( fire_at, TimeZone::Utc() ) ),
                                         [&]( const DateTime &scheduled ) {
                                             ++fires;
                                             EXPECT_EQ( scheduled.ToTimestamp(), fire_at );
                                         } );
    ASSERT_TRUE( id.has_value() );

    scheduler.AdvanceTo( fire_at - std::chrono::seconds( 1 ), kSteady0 + std::chrono::seconds( 1 ) );
    EXPECT_EQ( fires, 0 );

    scheduler.AdvanceTo( fire_at, kSteady0 + std::chrono::seconds( 1 ) );
    EXPECT_EQ( fires, 1 );

    // 一次性任务完成后：无下次触发、不再计入计划数
    EXPECT_FALSE( scheduler.NextFireTime( *id ).has_value() );
    EXPECT_EQ( scheduler.TaskCount(), 0u );

    // 已过期的一次性任务无法注册
    EXPECT_FALSE( scheduler.Add( std::make_shared<AtTrigger>( DateTime( fire_at, TimeZone::Utc() ) ),
                                 []( const DateTime & ) {} ).has_value() );

    // 已完成任务取消：无计划可取消
    EXPECT_FALSE( scheduler.Cancel( *id ) );

    // 统计仍可查询
    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 1u );
}

TEST( TaskSchedulerTest, CancelStopsFutureFires ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    int  fires = 0;
    auto id    = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ), kSteady0 ),
                                [&]( const DateTime & ) { ++fires; } );
    ASSERT_TRUE( id );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 1 ) );
    EXPECT_EQ( fires, 1 );

    EXPECT_TRUE( scheduler.Cancel( *id ) );
    EXPECT_FALSE( scheduler.NextFireTime( *id ).has_value() );
    EXPECT_EQ( scheduler.TaskCount(), 0u );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 100 ) );
    EXPECT_EQ( fires, 1 );  // 取消后不再触发

    EXPECT_FALSE( scheduler.Cancel( *id ) );  // 未知 id
}

// ============================ 执行器与在途（coalesce）============================

TEST( TaskSchedulerTest, CoalesceSkipsWhileInFlight ) {
    QueuedExecutor executor;
    TaskScheduler  scheduler( executor );
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    int  fires = 0;
    auto id    = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ), kSteady0 ),
                                [&]( const DateTime & ) { ++fires; } );
    ASSERT_TRUE( id );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 1 ) );
    ASSERT_EQ( executor.jobs.size(), 1u );  // 第 1 次已提交（在途）

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 2 ) );
    EXPECT_EQ( executor.jobs.size(), 1u );  // 在途未完成 → 跳过本次

    auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 1u );
    EXPECT_EQ( stats->skip_count, 1u );

    // 执行完成
    executor.jobs.front()();
    executor.jobs.clear();
    EXPECT_EQ( fires, 1 );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 3 ) );
    EXPECT_EQ( executor.jobs.size(), 1u );  // 恢复提交
    stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 2u );
}

TEST( TaskSchedulerTest, CoalesceDisabledAllowsOverlap ) {
    QueuedExecutor executor;
    TaskScheduler  scheduler( executor );
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    TaskPolicy policy;
    policy.coalesce = false;

    int  fires = 0;
    auto id    = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ), kSteady0 ),
                                [&]( const DateTime & ) { ++fires; }, policy );
    ASSERT_TRUE( id );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 1 ) );
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 2 ) );
    EXPECT_EQ( executor.jobs.size(), 2u );  // 允许并行在途

    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 2u );
    EXPECT_EQ( stats->skip_count, 0u );
}

TEST( TaskSchedulerTest, ExecutorRejectionCountsAsBackpressure ) {
    class RejectingExecutor final : public TaskExecutor {
    public:
        bool Submit( std::function<void()> ) override { return false; }
    };

    RejectingExecutor rejecting;
    TaskScheduler     scheduler( rejecting );
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    auto id = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ), kSteady0 ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    const auto fired = scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 1 ) );
    EXPECT_EQ( fired, 0u );

    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 0u );
    EXPECT_EQ( stats->reject_count, 1u );
}

TEST( TaskSchedulerTest, ActionExceptionsAreSwallowed ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 );

    int  fires = 0;
    auto id    = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ), kSteady0 ),
                                [&]( const DateTime & ) {
                                    ++fires;
                                    throw std::runtime_error( "boom" );
                                } );
    ASSERT_TRUE( id );

    EXPECT_NO_THROW( scheduler.AdvanceTo( WallUtc( 2024, 1, 1 ), kSteady0 + std::chrono::seconds( 1 ) ) );
    EXPECT_EQ( fires, 1 );

    const auto stats = scheduler.GetTaskStats( *id );
    ASSERT_TRUE( stats.has_value() );
    EXPECT_EQ( stats->fire_count, 1u );
}

TEST( TaskSchedulerTest, UnknownTaskQueriesReturnNullopt ) {
    TaskScheduler scheduler;
    EXPECT_FALSE( scheduler.NextFireTime( 12345 ).has_value() );
    EXPECT_FALSE( scheduler.GetTaskStats( 12345 ).has_value() );
    EXPECT_EQ( scheduler.TaskCount(), 0u );
}

// ============================ CalendarTrigger（wall 轴）============================

TEST( TaskSchedulerTest, CalendarDaily ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1, 10, 0 ), kSteady0 );

    int  fires = 0;
    auto id    = scheduler.Add( std::make_shared<CalendarTrigger>(
                                    CalendarTrigger::Daily( 9, 30, 0, TimeZone::Utc() ) ),
                                [&]( const DateTime &scheduled ) {
                                    ++fires;
                                    EXPECT_EQ( scheduled.Hour(), 9 );
                                    EXPECT_EQ( scheduled.Minute(), 30 );
                                } );
    ASSERT_TRUE( id );

    // 当天 9:30 已过 → 次日 9:30
    auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 2, 9, 30 ) );

    scheduler.AdvanceTo( WallUtc( 2024, 1, 2, 9, 30 ), kSteady0 + std::chrono::seconds( 1 ) );
    EXPECT_EQ( fires, 1 );

    // 下一天
    next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 3, 9, 30 ) );
}

TEST( TaskSchedulerTest, CalendarDailySameDayAndOffsetZone ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1, 9, 0 ), kSteady0 );

    // 当天 9:30 尚未到 → 当天 9:30
    auto id = scheduler.Add( std::make_shared<CalendarTrigger>(
                                 CalendarTrigger::Daily( 9, 30, 0, TimeZone::Utc() ) ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );
    auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 1, 9, 30 ) );

    // 固定偏移时区：+08:00 的每日 02:30 = 前一日 18:30 UTC
    auto offset_id = scheduler.Add( std::make_shared<CalendarTrigger>(
                                        CalendarTrigger::Daily( 2, 30, 0, TimeZone::OffsetMinutes( 480 ) ) ),
                                    []( const DateTime & ) {} );
    ASSERT_TRUE( offset_id );
    auto offset_next = scheduler.NextFireTime( *offset_id );
    ASSERT_TRUE( offset_next.has_value() );
    EXPECT_EQ( offset_next->ToTimestamp(), WallUtc( 2024, 1, 1, 18, 30 ) );  // 1 月 2 日 02:30 +08:00
}

TEST( TaskSchedulerTest, CalendarWeekly ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1, 0, 0 ), kSteady0 );  // 周一

    auto id = scheduler.Add( std::make_shared<CalendarTrigger>(
                                 CalendarTrigger::Weekly( Wednesday, 8, 0, 0, TimeZone::Utc() ) ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    // 周一 → 本周三 8:00
    auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 3, 8, 0 ) );
    EXPECT_EQ( next->GetWeekday(), Wednesday );

    // 周三 8:00 触发后 → 下周三
    scheduler.AdvanceTo( WallUtc( 2024, 1, 3, 8, 0 ), kSteady0 + std::chrono::seconds( 1 ) );
    next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 10, 8, 0 ) );
}

TEST( TaskSchedulerTest, CalendarMonthlySkipsMissingDay ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 15 ), kSteady0 );

    auto id = scheduler.Add( std::make_shared<CalendarTrigger>(
                                 CalendarTrigger::Monthly( 31, 6, 0, 0, TimeZone::Utc() ) ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    // 1 月 15 日 → 1 月 31 日
    auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 31, 6, 0 ) );

    // 1 月 31 日之后 → 2 月没有 31 日 → 3 月 31 日
    scheduler.AdvanceTo( WallUtc( 2024, 1, 31, 6, 0 ), kSteady0 + std::chrono::seconds( 1 ) );
    next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 3, 31, 6, 0 ) );
}

TEST( TaskSchedulerTest, FunctionalWallTrigger ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1, 0, 0 ), kSteady0 );

    // 每 2 小时一次（自定义规则）
    auto trigger = std::make_shared<FunctionalWallTrigger>(
        []( const DateTime &after ) { return std::optional<DateTime>( after.Add<Hours>( 2 ) ); } );
    auto id = scheduler.Add( trigger, []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->ToTimestamp(), WallUtc( 2024, 1, 1, 2, 0 ) );
}

// ============================ 夏令时（隔离 TZ 环境）============================

#if defined( __unix__ ) || defined( __APPLE__ )
namespace {

// 临时切换进程时区（RAII 恢复）
class ScopedTimeZone {
public:
    explicit ScopedTimeZone( const char *tz ) {
        const char *old = std::getenv( "TZ" );
        if ( old ) {
            old_ = old;
            had_ = true;
        }
        setenv( "TZ", tz, 1 );
        tzset();
    }
    ~ScopedTimeZone() {
        if ( had_ ) {
            setenv( "TZ", old_.c_str(), 1 );
        }
        else {
            unsetenv( "TZ" );
        }
        tzset();
    }

private:
    std::string old_;
    bool        had_{ false };
};

}  // namespace

TEST( TaskSchedulerTest, DstSpringForwardSkipsNonexistent ) {
    ScopedTimeZone tz( "America/New_York" );

    TaskScheduler scheduler;
    // 2024-03-09 12:00（本地）；3 月 10 日 02:30 不存在（02:00 → 03:00）
    const auto start = DateTime::FromParts( { 2024, 3, 9, 12, 0, 0, 0, 0, 0 } ).value();
    scheduler.AdvanceTo( start.ToTimestamp(), kSteady0 );

    auto id = scheduler.Add( std::make_shared<CalendarTrigger>( CalendarTrigger::Daily( 2, 30 ) ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    // 3 月 10 日的 02:30 不存在 → 跳过；下一次为 3 月 11 日 02:30
    EXPECT_EQ( next->Day(), 11 );
    EXPECT_EQ( next->Month(), 3 );
    EXPECT_EQ( next->Hour(), 2 );
    EXPECT_EQ( next->Minute(), 30 );
}

TEST( TaskSchedulerTest, DstFallBackFiresOnceAtEarlierInstant ) {
    ScopedTimeZone tz( "America/New_York" );

    TaskScheduler scheduler;
    // 2024-11-03 02:00 EDT 回拨为 01:00 EST：本地 01:30 出现两次（EDT 与 EST）
    const auto start = DateTime::FromParts( { 2024, 11, 2, 12, 0, 0, 0, 0, 0 } ).value();
    scheduler.AdvanceTo( start.ToTimestamp(), kSteady0 );

    auto id = scheduler.Add( std::make_shared<CalendarTrigger>( CalendarTrigger::Daily( 1, 30 ) ),
                             []( const DateTime & ) {} );
    ASSERT_TRUE( id );

    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    EXPECT_EQ( next->Day(), 3 );
    EXPECT_EQ( next->Hour(), 1 );
    EXPECT_EQ( next->Minute(), 30 );

    // 较早者：EDT（UTC-4）→ 05:30 UTC；较晚者 EST 为 06:30 UTC
    const auto earlier = DateTime::FromParts( { 2024, 11, 3, 1, 30, 0, 0, 0, 0 },
                                              TimeZone::OffsetMinutes( -240 ) )
                             .value();
    EXPECT_EQ( next->ToTimestamp(), earlier.ToTimestamp() );

    // 只触发一次：推进到较早时刻之后，下一个计划点应为次日（较晚的重复时刻不再触发）
    scheduler.AdvanceTo( earlier.ToTimestamp(), kSteady0 + std::chrono::seconds( 1 ) );
    const auto after_fire = scheduler.NextFireTime( *id );
    ASSERT_TRUE( after_fire.has_value() );
    EXPECT_EQ( after_fire->Day(), 4 );
}
#endif  // __unix__ || __APPLE__

// ============================ 抖动 ============================
TEST( TaskSchedulerTest, JitterShiftsOnlyFirstOccurrence ) {
    TaskScheduler scheduler;
    scheduler.AdvanceTo( WallUtc( 2024, 1, 1, 0, 0 ), kSteady0 );

    TaskPolicy policy;
    policy.jitter = Milliseconds( 500 );

    auto id = scheduler.Add( std::make_shared<CalendarTrigger>(
                                 CalendarTrigger::Daily( 0, 0, 0, TimeZone::Utc() ) ),
                             []( const DateTime & ) {}, policy );
    ASSERT_TRUE( id );

    // 首触发在 [基准, 基准 + 抖动] 内
    const auto next = scheduler.NextFireTime( *id );
    ASSERT_TRUE( next.has_value() );
    const auto base = WallUtc( 2024, 1, 2, 0, 0 );
    EXPECT_GE( next->ToTimestamp(), base );
    EXPECT_LE( next->ToTimestamp(), base + std::chrono::milliseconds( 500 ) );
}

TEST( TaskSchedulerTest, InvalidTriggerArgumentsThrow ) {
    EXPECT_THROW( IntervalTrigger( Nanoseconds( 0 ) ), std::invalid_argument );
    EXPECT_THROW( CalendarTrigger::Daily( 24, 0 ), std::invalid_argument );
    EXPECT_THROW( CalendarTrigger::Weekly( static_cast<Weekday>( 7 ), 0, 0 ), std::invalid_argument );
    EXPECT_THROW( CalendarTrigger::Monthly( 0, 0, 0 ), std::invalid_argument );
    EXPECT_THROW( CalendarTrigger::Monthly( 32, 0, 0 ), std::invalid_argument );
}

// ============================ 线程适配器（真实时钟冒烟）============================

namespace {

bool WaitUntil( const std::function<bool()> &predicate, Milliseconds timeout = std::chrono::seconds( 2 ) ) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while ( std::chrono::steady_clock::now() < deadline ) {
        if ( predicate() ) {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
    return predicate();
}

}  // namespace

TEST( TaskSchedulerTest, ThreadAdapterStartStop ) {
    TaskScheduler    scheduler;
    std::atomic<int> fires{ 0 };

    auto id = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::milliseconds( 20 ) ),
                             [&]( const DateTime & ) { fires.fetch_add( 1 ); } );
    ASSERT_TRUE( id );

    scheduler.Start();
    EXPECT_TRUE( scheduler.IsRunning() );
    EXPECT_TRUE( WaitUntil( [&] { return fires.load() >= 2; } ) );

    EXPECT_TRUE( scheduler.Stop( std::chrono::seconds( 2 ) ) );
    EXPECT_FALSE( scheduler.IsRunning() );

    const int settled = fires.load();
    std::this_thread::sleep_for( std::chrono::milliseconds( 60 ) );
    EXPECT_EQ( fires.load(), settled );  // 停止后不再触发
}

TEST( TaskSchedulerTest, ThreadAdapterStopTimeoutWithSlowInlineTask ) {
    TaskScheduler     scheduler;
    std::atomic<bool> action_started{ false };

    auto id = scheduler.Add( std::make_shared<AtTrigger>( DateTime::Now().Add<Milliseconds>( 20 ) ),
                             [&]( const DateTime & ) {
                                 action_started.store( true );
                                 std::this_thread::sleep_for( std::chrono::milliseconds( 400 ) );
                             } );
    ASSERT_TRUE( id );

    scheduler.Start();
    ASSERT_TRUE( WaitUntil( [&] { return action_started.load(); } ) );

    EXPECT_FALSE( scheduler.Stop( std::chrono::milliseconds( 50 ) ) );  // 超时：任务未返回
    EXPECT_TRUE( scheduler.Stop( std::chrono::seconds( 3 ) ) );         // 任务结束后成功
    EXPECT_FALSE( scheduler.IsRunning() );
}

TEST( TaskSchedulerTest, ThreadAdapterRestartAndIdempotentStart ) {
    TaskScheduler scheduler;

    scheduler.Start();
    scheduler.Start();  // 幂等
    EXPECT_TRUE( scheduler.IsRunning() );

    EXPECT_TRUE( scheduler.Stop() );
    EXPECT_FALSE( scheduler.IsRunning() );

    scheduler.Start();  // 可重启
    EXPECT_TRUE( scheduler.IsRunning() );
    EXPECT_TRUE( scheduler.Stop() );

    EXPECT_TRUE( scheduler.Stop() );  // 未运行时 Stop 视为成功
}

TEST( TaskSchedulerTest, ThreadAdapterSyncedWallClock ) {
    TaskScheduler scheduler;
    scheduler.SetSyncedWallClock( true );

    // 种子 = 当前时间 + 1 小时：校准时间在未来；一次性任务按校准时间在 30ms 后触发。
    // 若线程适配器误用系统时间，该任务需要等约 1 小时才会触发（本测试将超时失败）
    const Timestamp seed = DateTime::NowTimestamp() + std::chrono::hours( 1 );
    DateTime::SetTimeSeed( seed );

    std::atomic<int> fires{ 0 };
    auto             id = scheduler.Add( std::make_shared<AtTrigger>( DateTime( seed + std::chrono::milliseconds( 30 ) ) ),
                                         [&]( const DateTime & ) { fires.fetch_add( 1 ); } );
    ASSERT_TRUE( id );

    scheduler.Start();
    EXPECT_TRUE( WaitUntil( [&] { return fires.load() >= 1; } ) );
    EXPECT_TRUE( scheduler.Stop() );

    DateTime::ResetTimeSeed();
}

TEST( TaskSchedulerTest, ThreadAdapterThreadPoolKeepsDispatchingDuringSlowTask ) {
    ThreadPoolExecutor pool( 2 );
    TaskScheduler      scheduler( pool );

    std::atomic<int>  fast_fires{ 0 };
    std::atomic<bool> slow_started{ false };

    // 慢任务：一次性，占用池线程 ~300ms
    auto slow_id = scheduler.Add( std::make_shared<AtTrigger>( DateTime::Now().Add<Milliseconds>( 10 ) ),
                                  [&]( const DateTime & ) {
                                      slow_started.store( true );
                                      std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
                                  } );
    ASSERT_TRUE( slow_id );

    // 快任务：20ms 固定周期
    auto fast_id = scheduler.Add( std::make_shared<IntervalTrigger>( std::chrono::milliseconds( 20 ) ),
                                  [&]( const DateTime & ) { fast_fires.fetch_add( 1 ); } );
    ASSERT_TRUE( fast_id );

    scheduler.Start();
    ASSERT_TRUE( WaitUntil( [&] { return slow_started.load(); } ) );

    // 慢任务占用池线程期间，快任务照常触发 → 派发线程未被慢任务阻塞
    EXPECT_TRUE( WaitUntil( [&] { return fast_fires.load() >= 3; } ) );

    // 对照 InlineExecutor 的慢任务用例（Stop 在任务执行期间超时返回 false）：
    // 线程池下派发线程始终空闲，Stop 立即成功
    EXPECT_TRUE( scheduler.Stop( std::chrono::milliseconds( 500 ) ) );
    EXPECT_FALSE( scheduler.IsRunning() );
}
