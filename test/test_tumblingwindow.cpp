#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include "DateTime.h"
#include "TaskScheduler.h"
#include "TumblingWindow.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

using SteadyPoint    = std::chrono::steady_clock::time_point;
using SteadyDuration = std::chrono::seconds;

SteadyPoint At( int seconds ) {
    return SteadyPoint{} + std::chrono::seconds( seconds );
}

using IntWindow = TumblingWindow<int, SteadyPoint>;
using IntPolicy = IntWindow::Policy;

IntPolicy TimePolicy( int span_seconds ) {
    IntPolicy policy;
    policy.span   = SteadyDuration( span_seconds );
    policy.key_fn = []( const int &value ) { return At( value ); };  // 元素值即事件时间（秒）
    return policy;
}

struct CloseRecord {
    SteadyPoint      start;
    std::vector<int> items;
};

void Capture( IntWindow &window, std::vector<CloseRecord> &records ) {
    window.OnClose( [&records]( SteadyPoint start, const IntWindow::ItemList &items ) {
        records.push_back( CloseRecord{ start, items } );
    } );
}

}  // namespace

// ============================ 时间桶 ============================

TEST( TumblingWindowTest, PollClosesTimeBucket ) {
    IntWindow window( TimePolicy( 60 ) );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 10 );
    window.Add( 20 );
    window.Add( 30 );
    EXPECT_EQ( window.Size(), 3u );
    EXPECT_TRUE( closes.empty() );

    EXPECT_EQ( window.Poll( At( 60 ) ), 1u );  // 推进到 [0,60) 的右边界，收尾 1 个桶

    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 0 ) );
    EXPECT_EQ( closes[0].items, ( std::vector<int>{ 10, 20, 30 } ) );
    EXPECT_EQ( window.Size(), 0u );
    EXPECT_EQ( window.BucketStart(), At( 60 ) );
}

TEST( TumblingWindowTest, AddAcrossBoundaryClosesPreviousBucket ) {
    IntWindow window( TimePolicy( 60 ) );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 10 );
    window.Add( 70 );  // 跨桶：先收尾 [0,60)，70 属于 [60,120)

    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 0 ) );
    EXPECT_EQ( closes[0].items.size(), 1u );
    EXPECT_EQ( window.BucketStart(), At( 60 ) );
    EXPECT_EQ( window.Size(), 1u );
}

TEST( TumblingWindowTest, LateDataIsDropped ) {
    IntWindow window( TimePolicy( 60 ) );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 70 );  // 当前桶 [60,120)
    window.Add( 10 );  // 早于桶起点 → 迟到丢弃

    EXPECT_EQ( window.LateCount(), 1u );
    EXPECT_EQ( window.Size(), 1u );
    EXPECT_EQ( closes.size(), 0u );
}

TEST( TumblingWindowTest, AlignFirstItem ) {
    IntPolicy policy = TimePolicy( 20 );
    policy.align     = WindowAlign::FirstItem;

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 7 );   // 锚点 7 → 桶 [7,27)
    window.Add( 18 );
    EXPECT_EQ( window.BucketStart(), At( 7 ) );

    window.Add( 28 );  // 跨桶 → 收尾 [7,27)
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 7 ) );
    EXPECT_EQ( closes[0].items.size(), 2u );
    EXPECT_EQ( window.BucketStart(), At( 27 ) );
}

// ============================ 空桶与追赶 ============================

TEST( TumblingWindowTest, PollEmitsEmptyBucketsWhenEnabled ) {
    IntPolicy policy = TimePolicy( 60 );
    policy.emit_empty = true;

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Poll( At( 0 ) );  // 建立锚点（不输出）
    EXPECT_TRUE( closes.empty() );

    EXPECT_EQ( window.Poll( At( 180 ) ), 3u );  // 关闭 [0,60) [60,120) [120,180)

    ASSERT_EQ( closes.size(), 3u );
    for ( const auto &record : closes ) {
        EXPECT_TRUE( record.items.empty() );
    }
    EXPECT_EQ( window.BucketStart(), At( 180 ) );
}

TEST( TumblingWindowTest, PollCatchUpIsCapped ) {
    IntPolicy policy = TimePolicy( 60 );
    policy.emit_empty  = true;
    policy.max_catchup = 2;

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Poll( At( 0 ) );
    window.Poll( At( 600 ) );  // 跨 10 个桶：当前空桶 + 最多 2 个补发

    EXPECT_EQ( closes.size(), 3u );
    EXPECT_EQ( window.BucketStart(), At( 600 ) );
}

// ============================ 数据驱动收尾 ============================

TEST( TumblingWindowTest, CountTrigger ) {
    IntPolicy policy;
    policy.count  = 3;
    policy.key_fn = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 1 );
    window.Add( 2 );
    EXPECT_EQ( closes.size(), 0u );

    window.Add( 3 );
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 1 ) );  // 无时间桶：以本批首条数据为标签
    EXPECT_EQ( closes[0].items, ( std::vector<int>{ 1, 2, 3 } ) );
    EXPECT_EQ( window.Size(), 0u );
}

TEST( TumblingWindowTest, TimeOrCountCombination ) {
    IntPolicy policy = TimePolicy( 60 );
    policy.count    = 2;
    policy.combine  = RuleCombine::Any;

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 10 );
    window.Add( 20 );  // 条数先到 → 立即收尾（桶起点仍为 0）
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 0 ) );
    EXPECT_EQ( closes[0].items.size(), 2u );

    window.Add( 30 );
    window.Poll( At( 60 ) );  // 时间边界到达 → 收尾剩余数据（同一桶起点）
    ASSERT_EQ( closes.size(), 2u );
    EXPECT_EQ( closes[1].start, At( 0 ) );
    EXPECT_EQ( closes[1].items.size(), 1u );
}

TEST( TumblingWindowTest, TimeAndCountRequiresBoth ) {
    IntPolicy policy = TimePolicy( 60 );
    policy.count    = 5;
    policy.combine  = RuleCombine::All;

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    for ( int i = 0; i < 4; ++i ) {
        window.Add( 10 + i );  // 条数不足，即使跨过时间边界也不收尾
    }
    window.Poll( At( 60 ) );   // 时间边界（硬边界）仍会收尾
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].items.size(), 4u );
}

TEST( TumblingWindowTest, WeightTrigger ) {
    IntPolicy policy;
    policy.weight    = 10.0;
    policy.weight_fn = []( const int &value ) { return static_cast<double>( value ); };
    policy.key_fn    = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 4 );
    window.Add( 4 );
    EXPECT_EQ( closes.size(), 0u );

    window.Add( 4 );  // 累计 12 >= 10
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].items.size(), 3u );
}

TEST( TumblingWindowTest, PredicateTrigger ) {
    IntPolicy policy;
    policy.predicate = []( const int &value ) { return value < 0; };  // 负值是结束标记
    policy.key_fn    = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 1 );
    window.Add( 2 );
    window.Add( -1 );

    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].items, ( std::vector<int>{ 1, 2, -1 } ) );
}

TEST( TumblingWindowTest, CustomTrigger ) {
    IntPolicy policy;
    policy.custom = [count = 0]( const int &, SteadyPoint ) mutable { return ( ++count % 2 ) == 0; };
    policy.key_fn = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 1 );
    window.Add( 2 );  // 第 2 条 → 收尾
    window.Add( 3 );
    window.Add( 4 );  // 第 4 条 → 收尾

    ASSERT_EQ( closes.size(), 2u );
    EXPECT_EQ( closes[0].items.size(), 2u );
    EXPECT_EQ( closes[1].items.size(), 2u );
}

// ============================ 会话与显式收尾 ============================

TEST( TumblingWindowTest, SessionGapSplitsBuckets ) {
    IntPolicy policy;
    policy.session_gap = SteadyDuration( 300 );
    policy.key_fn      = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 10 );
    window.Add( 20 );
    window.Add( 400 );  // 与 20 相隔 380s > 300s → 先收尾旧会话

    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 10 ) );
    EXPECT_EQ( closes[0].items.size(), 2u );
    EXPECT_EQ( window.Size(), 1u );

    window.Poll( At( 1000 ) );  // 空闲超时 → 收尾当前会话
    ASSERT_EQ( closes.size(), 2u );
    EXPECT_EQ( closes[1].items, ( std::vector<int>{ 400 } ) );

    window.Poll( At( 2000 ) );  // 已空，不重复通知
    EXPECT_EQ( closes.size(), 2u );
}

TEST( TumblingWindowTest, ExplicitClose) {
    IntWindow window( TimePolicy( 60 ) );
    std::vector<CloseRecord> closes;
    Capture( window, closes );

    window.Add( 10 );
    window.Close();  // 手动收尾

    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].start, At( 0 ) );
    EXPECT_FALSE( window.BucketStart().has_value() );

    window.Add( 70 );  // 重新建立锚点
    EXPECT_EQ( window.BucketStart(), At( 60 ) );
}

TEST( TumblingWindowTest, ClearDiscardsWithoutOutput ) {
    IntWindow window( TimePolicy( 60 ) );
    std::vector<CloseRecord> closes;
    Capture( window, closes );
    std::vector<int> removed;
    window.OnRemove( [&removed]( const IntWindow::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    window.Add( 10 );
    window.Clear();

    EXPECT_TRUE( closes.empty() );
    EXPECT_EQ( removed, ( std::vector<int>{ 10 } ) );
    EXPECT_EQ( window.Size(), 0u );
    EXPECT_EQ( window.BucketStart(), At( 0 ) );  // 时间桶边界保持

    window.Add( 20 );
    window.Poll( At( 60 ) );
    ASSERT_EQ( closes.size(), 1u );
    EXPECT_EQ( closes[0].items, ( std::vector<int>{ 20 } ) );
}

TEST( TumblingWindowTest, InvalidPolicyThrows ) {
    IntPolicy zero_span;
    zero_span.span = SteadyDuration( 0 );
    EXPECT_THROW( IntWindow{ zero_span }, std::invalid_argument );

    IntPolicy zero_count;
    zero_count.count = 0;
    EXPECT_THROW( IntWindow{ zero_count }, std::invalid_argument );

    IntPolicy negative_weight;
    negative_weight.weight = -1.0;
    EXPECT_THROW( IntWindow{ negative_weight }, std::invalid_argument );
}

// ============================ 与 TaskScheduler 组合 ============================

TEST( TumblingWindowTest, TaskSchedulerDrivesPoll ) {
    using WallWindow = TumblingWindow<std::int64_t, Timestamp>;

    WallWindow::Policy policy;
    policy.span   = Milliseconds( 60000 );
    policy.key_fn = []( const std::int64_t &value ) {
        return Timestamp( std::chrono::milliseconds( value ) );
    };

    WallWindow window( policy );
    std::vector<std::size_t> closed_sizes;
    window.OnClose( [&closed_sizes]( Timestamp, const WallWindow::ItemList &items ) {
        closed_sizes.push_back( items.size() );
    } );

    TaskScheduler scheduler;
    const Timestamp t0 = Timestamp( std::chrono::milliseconds( 1700000000000LL ) );
    scheduler.Poll( t0, TaskScheduler::SteadyTime{ std::chrono::seconds( 1 ) } );

    // 每分钟触发一次：把"计划时刻"传给 Poll，桶边界不会被抖动/迟到带偏
    auto trigger = std::make_shared<FunctionalWallTrigger>( []( const DateTime &after ) {
        return std::optional<DateTime>( DateTime( after.ToTimestamp() + Milliseconds( 60000 ), after.Zone() ) );
    } );
    scheduler.Schedule( trigger, [&window]( const DateTime &scheduled ) { window.Poll( scheduled.ToTimestamp() ); } );

    scheduler.Poll( t0 + Milliseconds( 60000 ), TaskScheduler::SteadyTime{ std::chrono::seconds( 2 ) } );
    EXPECT_TRUE( closed_sizes.empty() );  // 首次触发只建立锚点

    const auto item_ms = std::chrono::duration_cast<Milliseconds>(
                             ( t0 + Milliseconds( 90000 ) ).time_since_epoch() ).count();
    window.Add( item_ms );

    scheduler.Poll( t0 + Milliseconds( 120000 ), TaskScheduler::SteadyTime{ std::chrono::seconds( 3 ) } );
    ASSERT_EQ( closed_sizes.size(), 1u );
    EXPECT_EQ( closed_sizes[0], 1u );
}
