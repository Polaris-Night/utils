#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "SlidingWindow.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

struct Event {
    int       id;
    long long ts;
};

using SteadyPoint = std::chrono::steady_clock::time_point;

SteadyPoint At( int seconds ) {
    return SteadyPoint{} + std::chrono::seconds( seconds );
}

}  // namespace

// ============================ CountWindow ============================

TEST( CountWindowTest, BasicEviction ) {
    CountWindow<int> window( 3 );
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( window.Capacity(), 3u );

    for ( int i = 1; i <= 5; ++i ) {
        window.Add( i );
    }

    EXPECT_EQ( window.Size(), 3u );
    EXPECT_FALSE( window.Empty() );

    auto recent = window.GetRecent( 10 );
    ASSERT_EQ( recent.size(), 3u );
    EXPECT_EQ( recent[0], 3 );
    EXPECT_EQ( recent[1], 4 );
    EXPECT_EQ( recent[2], 5 );
}

TEST( CountWindowTest, CallbacksAndClear ) {
    CountWindow<int> window( 2 );
    std::vector<int> adds, removes;

    window.OnAdd( [&]( const SlidingWindow<int>::ItemList &items ) {
        for ( int v : items ) adds.push_back( v );
    } );
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        for ( int v : items ) removes.push_back( v );
    } );

    window.Add( 1 );
    window.Add( 2 );
    window.Add( 3 );  // 淘汰 1

    EXPECT_EQ( adds, ( std::vector<int>{ 1, 2, 3 } ) );
    EXPECT_EQ( removes, ( std::vector<int>{ 1 } ) );

    // Clear 应作为批量移除通知
    window.Clear();
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( removes, ( std::vector<int>{ 1, 2, 3 } ) );

    // 空窗口 Clear 不触发通知
    window.Clear();
    EXPECT_EQ( removes.size(), 3u );
}

TEST( CountWindowTest, GetRecentEdges ) {
    CountWindow<int> window( 5 );
    for ( int i = 1; i <= 3; ++i ) {
        window.Add( i );
    }

    EXPECT_TRUE( window.GetRecent( 0 ).empty() );
    EXPECT_EQ( window.GetRecent( 3 ).size(), 3u );
    EXPECT_EQ( window.GetRecent( 100 ).size(), 3u );

    // 迭代支持
    int sum = 0;
    for ( int v : window ) {
        sum += v;
    }
    EXPECT_EQ( sum, 6 );
}

TEST( CountWindowTest, ZeroCapacity ) {
    CountWindow<int> window( 0 );
    int              add_count = 0, remove_count = 0;
    window.OnAdd( [&]( const SlidingWindow<int>::ItemList &items ) { add_count += static_cast<int>( items.size() ); } );
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        remove_count += static_cast<int>( items.size() );
    } );

    window.Add( 42 );
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( add_count, 1 );
    EXPECT_EQ( remove_count, 1 );
}

TEST( CountWindowTest, AddBeforeRemoveNotificationOrder ) {
    CountWindow<int>      window( 1 );
    std::vector<std::string> order;
    window.OnAdd( [&]( const SlidingWindow<int>::ItemList & ) { order.push_back( "add" ); } );
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList & ) { order.push_back( "remove" ); } );

    window.Add( 1 );
    window.Add( 2 );

    EXPECT_EQ( order, ( std::vector<std::string>{ "add", "add", "remove" } ) );
}

// ============================ TimeWindow ============================

TEST( TimeWindowTest, ChronoAddAndQuery ) {
    TimeWindow<Event, SteadyPoint> window( std::chrono::seconds( 60 ),
                                           []( const Event &e ) { return At( static_cast<int>( e.ts ) ); } );

    window.Add( { 1, 0 } );
    window.Add( { 2, 10 } );
    window.Add( { 3, 20 } );

    ASSERT_EQ( window.Size(), 3u );
    ASSERT_TRUE( window.LatestTime().has_value() );
    EXPECT_EQ( *window.LatestTime(), At( 20 ) );

    auto all = window.Query( std::nullopt, std::nullopt );
    EXPECT_EQ( all.size(), 3u );

    auto ranged = window.Query( At( 10 ), At( 20 ) );
    ASSERT_EQ( ranged.size(), 2u );
    EXPECT_EQ( ranged[0].id, 2 );
    EXPECT_EQ( ranged[1].id, 3 );

    auto open_ended = window.Query( At( 10 ), std::nullopt );
    EXPECT_EQ( open_ended.size(), 2u );

    // start > end 返回空，而不是未定义行为
    EXPECT_TRUE( window.Query( At( 20 ), At( 10 ) ).empty() );

    // 最近 n 条
    auto recent = window.GetRecent( 2 );
    ASSERT_EQ( recent.size(), 2u );
    EXPECT_EQ( recent[0].id, 2 );
    EXPECT_TRUE( window.GetRecent( 0 ).empty() );
}

TEST( TimeWindowTest, OutOfOrderAddReportsCorrectPayload ) {
    // 回归测试：乱序插入时，on_add 载荷必须是被插入的元素（而不是窗口最后一个元素）
    TimeWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );

    std::vector<int> added_ids;
    window.OnAdd( [&]( const SlidingWindow<Event>::ItemList &items ) {
        for ( const auto &e : items ) added_ids.push_back( e.id );
    } );

    window.Add( { 1, 100 } );
    window.Add( { 2, 300 } );
    window.Add( { 3, 200 } );  // 乱序插入到中间

    EXPECT_EQ( added_ids, ( std::vector<int>{ 1, 2, 3 } ) );

    // 窗口仍保持时间有序
    std::vector<long long> times;
    for ( const auto &e : window ) {
        times.push_back( e.ts );
    }
    EXPECT_EQ( times, ( std::vector<long long>{ 100, 200, 300 } ) );
}

TEST( TimeWindowTest, PruneRemovesExpiredInBatch ) {
    TimeWindow<Event, long long> window( 30, []( const Event &e ) { return e.ts; } );

    std::vector<int> removed_ids;
    window.OnRemove( [&]( const SlidingWindow<Event>::ItemList &items ) {
        for ( const auto &e : items ) removed_ids.push_back( e.id );
    } );

    window.Add( { 1, 0 } );
    window.Add( { 2, 10 } );
    window.Add( { 3, 20 } );
    window.Add( { 4, 100 } );  // cutoff=70，0/10/20 全部过期

    EXPECT_EQ( removed_ids, ( std::vector<int>{ 1, 2, 3 } ) );
    EXPECT_EQ( window.Size(), 1u );
}

TEST( TimeWindowTest, CutoffIsInclusive ) {
    TimeWindow<Event, long long> window( 10, []( const Event &e ) { return e.ts; } );
    window.Add( { 1, 90 } );
    window.Add( { 2, 100 } );  // cutoff=90：边界上的数据保留
    EXPECT_EQ( window.Size(), 2u );
}

TEST( TimeWindowTest, PruneCooldown ) {
    TimeWindow<Event, long long> window( 10, []( const Event &e ) { return e.ts; }, 1000 );

    window.Add( { 1, 0 } );   // 首次添加即检查：无可裁剪
    window.Add( { 2, 50 } );  // 距上次裁剪仅 50 < 1000，跳过（t=0 已过期但仍保留）
    EXPECT_EQ( window.Size(), 2u );

    window.Add( { 3, 1000 } );  // 冷却期满，批量清理过期数据
    EXPECT_EQ( window.Size(), 1u );
    ASSERT_TRUE( window.LatestTime().has_value() );
    EXPECT_EQ( *window.LatestTime(), 1000 );
}

TEST( TimeWindowTest, UnsignedTickNoWrapRegression ) {
    // 回归测试：uint32_t tick 小于窗口跨度时，cutoff 计算曾下溢回绕导致整窗被误清空
    TimeWindow<Event, uint32_t> window( 300u, []( const Event &e ) { return static_cast<uint32_t>( e.ts ); } );

    window.Add( { 1, 100u } );
    EXPECT_EQ( window.Size(), 1u );  // 修复前此处会被清空

    window.Add( { 2, 200u } );
    EXPECT_EQ( window.Size(), 2u );

    // 正常裁剪仍然生效
    window.Add( { 3, 1000u } );  // cutoff=700，前两条过期
    EXPECT_EQ( window.Size(), 1u );
}

TEST( TimeWindowTest, TimeTCompileVariants ) {
    // time_t（秒级 Unix 时间戳）
    TimeWindow<Event, std::time_t> time_t_window( 3600, []( const Event &e ) { return static_cast<std::time_t>( e.ts ); } );
    time_t_window.Add( { 1, 1700000000 } );
    time_t_window.Add( { 2, 1700000010 } );
    EXPECT_EQ( time_t_window.Size(), 2u );

    // int64_t（毫秒时间戳）
    TimeWindow<Event, int64_t> ms_window( 5000, []( const Event &e ) { return e.ts; } );
    ms_window.Add( { 1, 1700000000000LL } );
    ms_window.Add( { 2, 1700000001000LL } );
    EXPECT_EQ( ms_window.Size(), 2u );
}

TEST( TimeWindowTest, ClearResetsPruneState ) {
    TimeWindow<Event, long long> window( 10, []( const Event &e ) { return e.ts; } );
    std::vector<int>             removed_ids;
    window.OnRemove( [&]( const SlidingWindow<Event>::ItemList &items ) {
        for ( const auto &e : items ) removed_ids.push_back( e.id );
    } );

    window.Add( { 1, 100 } );
    window.Clear();
    EXPECT_TRUE( window.Empty() );
    EXPECT_FALSE( window.LatestTime().has_value() );
    EXPECT_EQ( removed_ids, ( std::vector<int>{ 1 } ) );

    // 清空后重新添加：裁剪状态已重置，不应残留旧数据
    window.Add( { 2, 300 } );
    EXPECT_EQ( window.Size(), 1u );
}

// ============================ ThresholdWindow ============================

TEST( ThresholdWindowTest, BatchEviction ) {
    ThresholdWindow<std::string> window(
        100.0, []( const std::string &s ) { return static_cast<double>( s.size() ); } );

    std::vector<std::string> removed;
    window.OnRemove( [&]( const SlidingWindow<std::string>::ItemList &items ) {
        removed = items;
    } );

    window.Add( std::string( 40, 'a' ) );
    window.Add( std::string( 40, 'b' ) );
    EXPECT_EQ( window.CurrentWeight(), 80.0 );

    window.Add( std::string( 30, 'c' ) );  // 110 > 100，批量淘汰最早的 40
    EXPECT_EQ( window.CurrentWeight(), 70.0 );
    EXPECT_EQ( window.Size(), 2u );
    ASSERT_EQ( removed.size(), 1u );
    EXPECT_EQ( removed[0], std::string( 40, 'a' ) );

    auto recent = window.GetRecent( 1 );
    ASSERT_EQ( recent.size(), 1u );
    EXPECT_EQ( recent[0], std::string( 30, 'c' ) );
}

TEST( ThresholdWindowTest, SingleOversizeItemPassesThrough ) {
    ThresholdWindow<int> window( 100.0, []( int v ) { return static_cast<double>( v ); } );

    int remove_count = 0;
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        remove_count += static_cast<int>( items.size() );
    } );

    window.Add( 150 );  // 单条即超限：即进即出
    EXPECT_TRUE( window.Empty() );
    EXPECT_DOUBLE_EQ( window.CurrentWeight(), 0.0 );
    EXPECT_EQ( remove_count, 1 );
}

TEST( ThresholdWindowTest, ClearNotifiesAndResetsWeight ) {
    ThresholdWindow<int> window( 1000.0, []( int v ) { return static_cast<double>( v ); } );
    window.Add( 10 );
    window.Add( 20 );
    EXPECT_DOUBLE_EQ( window.CurrentWeight(), 30.0 );

    int removed_total = 0;
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        removed_total += static_cast<int>( items.size() );
    } );

    window.Clear();
    EXPECT_TRUE( window.Empty() );
    EXPECT_DOUBLE_EQ( window.CurrentWeight(), 0.0 );
    EXPECT_EQ( removed_total, 2 );
}

// ============================ SessionWindow ============================

TEST( SessionWindowTest, BreakArchivesOldSession ) {
    SessionWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );

    std::vector<Event> archived;
    window.OnRemove( [&]( const SlidingWindow<Event>::ItemList &items ) {
        archived.insert( archived.end(), items.begin(), items.end() );
    } );

    window.Add( { 1, 0 } );
    window.Add( { 2, 500 } );
    EXPECT_EQ( window.Size(), 2u );

    window.Add( { 3, 2000 } );  // 间隔 1500 > 1000：归档旧会话
    ASSERT_EQ( archived.size(), 2u );
    EXPECT_EQ( archived[0].id, 1 );
    EXPECT_EQ( archived[1].id, 2 );
    EXPECT_EQ( window.Size(), 1u );  // 新会话只含新数据

    window.Add( { 4, 2100 } );  // 新会话内的连续数据
    EXPECT_EQ( window.Size(), 2u );
    EXPECT_EQ( archived.size(), 2u );
}

TEST( SessionWindowTest, OutOfOrderWithinSession ) {
    SessionWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );

    window.Add( { 1, 100 } );
    window.Add( { 2, 80 } );   // 会话内乱序：按到达顺序保存，不延长会话基准
    window.Add( { 3, 900 } );  // 与基准 100 的间隔 800 <= 1000，不分裂
    EXPECT_EQ( window.Size(), 3u );

    window.Add( { 4, 2500 } );  // 与基准 900 的间隔 1600 > 1000：分裂
    EXPECT_EQ( window.Size(), 1u );
}

TEST( SessionWindowTest, FlushClosesCurrentSession ) {
    SessionWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );
    window.Add( { 1, 0 } );
    window.Add( { 2, 100 } );

    int removed_total = 0;
    window.OnRemove( [&]( const SlidingWindow<Event>::ItemList &items ) {
        removed_total += static_cast<int>( items.size() );
    } );

    auto flushed = window.Close();
    ASSERT_EQ( flushed.size(), 2u );
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( removed_total, 2 );

    // 空会话 Flush：返回空且不触发回调
    auto empty_flush = window.Close();
    EXPECT_TRUE( empty_flush.empty() );
    EXPECT_EQ( removed_total, 2 );
}

TEST( SessionWindowTest, ClearClosesSession ) {
    SessionWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );
    window.Add( { 1, 0 } );

    int removed_total = 0;
    int closed_total  = 0;
    window.OnRemove( [&]( const SlidingWindow<Event>::ItemList &items ) {
        removed_total += static_cast<int>( items.size() );
    } );
    window.OnClose( [&]( const SlidingWindow<Event>::ItemList &items ) {
        closed_total += static_cast<int>( items.size() );
    } );

    window.Clear();  // 放弃会话：只通知移除，不算“会话完成”
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( removed_total, 1 );
    EXPECT_EQ( closed_total, 0 );

    // 清空后新会话重新开始；正常结束才触发 OnClose
    window.Add( { 2, 50 } );
    EXPECT_EQ( window.Size(), 1u );
    (void)window.Close();
    EXPECT_EQ( closed_total, 1 );
}

// ============================ PredicateWindow ============================

TEST( PredicateWindowTest, BreakClearsConsecutiveSequence ) {
    PredicateWindow<int> window( []( int x ) { return x > 0; } );

    int         break_count   = 0;
    std::size_t removed_total = 0;
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        ++break_count;
        removed_total += items.size();
    } );

    window.Add( 1 );
    window.Add( 2 );
    window.Add( 3 );
    EXPECT_EQ( window.Size(), 3u );

    window.Add( -1 );  // 打破条件：清空且不入窗
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( break_count, 1 );
    EXPECT_EQ( removed_total, 3u );

    window.Add( 5 );
    EXPECT_EQ( window.Size(), 1u );
    EXPECT_EQ( break_count, 1 );
}

TEST( PredicateWindowTest, BreakOnEmptyDoesNotNotify ) {
    PredicateWindow<int> window( []( int x ) { return x > 0; } );
    int                  remove_batches = 0;
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList & ) { ++remove_batches; } );

    window.Add( -1 );
    window.Add( -2 );
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( remove_batches, 0 );
}

TEST( PredicateWindowTest, GetRecentAndClear ) {
    PredicateWindow<int> window( []( int x ) { return x > 0; } );
    window.Add( 1 );
    window.Add( 2 );
    window.Add( 3 );

    EXPECT_TRUE( window.GetRecent( 0 ).empty() );
    auto recent = window.GetRecent( 2 );
    ASSERT_EQ( recent.size(), 2u );
    EXPECT_EQ( recent[0], 2 );
    EXPECT_EQ( recent[1], 3 );

    window.Clear();
    EXPECT_TRUE( window.Empty() );
}

// ============================ 多态基类 ============================

TEST( SlidingWindowTest, PolymorphicUsage ) {
    CountWindow<int>         count_window( 2 );
    SlidingWindow<int>      &base = count_window;
    std::vector<int>         removed;

    base.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        for ( int v : items ) removed.push_back( v );
    } );

    base.Add( 1 );
    base.Add( 2 );
    base.Add( 3 );
    EXPECT_EQ( base.Size(), 2u );
    EXPECT_FALSE( base.Empty() );

    base.Clear();
    EXPECT_TRUE( base.Empty() );
    EXPECT_EQ( removed, ( std::vector<int>{ 1, 2, 3 } ) );
}

// ============================ 缺席驱动（Poll）与通道 ============================

TEST( TimeWindowTest, PollPrunesWithoutNewData ) {
    TimeWindow<int, SteadyPoint> window( std::chrono::seconds( 10 ),
                                         []( int v ) { return At( v ); } );
    std::vector<int> removed;
    window.OnRemove( [&]( const SlidingWindow<int>::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    window.Add( 10 );
    window.Add( 20 );
    EXPECT_EQ( window.Size(), 2u );

    // 只推进时间、不喂数据：过期数据也应被淘汰
    EXPECT_EQ( window.Poll( At( 25 ) ), 1u );
    EXPECT_EQ( window.Size(), 1u );
    EXPECT_EQ( removed, ( std::vector<int>{ 10 } ) );
    EXPECT_EQ( *window.LatestTime(), At( 20 ) );

    window.Poll( At( 100 ) );
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( removed, ( std::vector<int>{ 10, 20 } ) );

    // 回拨时间不应让已淘汰的数据复活，也不应报错
    window.Poll( At( 5 ) );
    EXPECT_TRUE( window.Empty() );
}

TEST( TimeWindowTest, KeyFnCalledOncePerItem ) {
    int calls = 0;
    TimeWindow<int, SteadyPoint> window( std::chrono::seconds( 100 ),
                                         [&calls]( int v ) {
                                             ++calls;
                                             return At( v );
                                         } );

    for ( int i = 1; i <= 5; ++i ) {
        window.Add( i );
    }
    EXPECT_EQ( calls, 5 );  // 每条只调用一次 key_fn

    (void)window.LatestTime();
    (void)window.Query( At( 2 ), At( 4 ) );
    window.Poll( At( 50 ) );
    EXPECT_EQ( calls, 5 );  // 查询/裁剪全部使用缓存
}

TEST( SessionWindowTest, PollClosesIdleSession ) {
    SessionWindow<Event, long long> window( 1000, []( const Event &e ) { return e.ts; } );

    int                              close_batches = 0;
    std::size_t                      closed_items  = 0;
    std::vector<std::size_t>         close_sizes;
    window.OnClose( [&]( const SlidingWindow<Event>::ItemList &items ) {
        ++close_batches;
        closed_items += items.size();
        close_sizes.push_back( items.size() );
    } );

    window.Add( { 1, 0 } );
    window.Add( { 2, 100 } );

    window.Poll( 500 );  // 未超时：不结束
    EXPECT_EQ( window.Size(), 2u );
    EXPECT_EQ( close_batches, 0 );

    EXPECT_EQ( window.Poll( 2000 ), 1u );  // 距 100 已 1900 > 1000：主动结束会话
    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( close_batches, 1 );
    EXPECT_EQ( closed_items, 2u );

    window.Poll( 5000 );  // 空会话：不重复通知
    EXPECT_EQ( close_batches, 1 );
}

TEST( PredicateWindowTest, RejectNotifiesBreakingItem ) {
    PredicateWindow<int> window( []( int x ) { return x > 0; } );

    std::vector<int> rejected;
    window.OnReject( [&rejected]( const int &value ) { rejected.push_back( value ); } );

    window.Add( 1 );
    window.Add( -7 );

    ASSERT_EQ( rejected.size(), 1u );
    EXPECT_EQ( rejected[0], -7 );
    EXPECT_TRUE( window.Empty() );
}

TEST( CountWindowTest, RegisteringListenerInsideCallbackIsSafe ) {
    CountWindow<int> window( 10 );
    int              outer = 0;
    int              inner = 0;

    window.OnAdd( [&]( const SlidingWindow<int>::ItemList & ) {
        ++outer;
        window.OnAdd( [&inner]( const SlidingWindow<int>::ItemList & ) { ++inner; } );
    } );

    window.Add( 1 );  // 本次事件：新监听尚未生效
    EXPECT_EQ( outer, 1 );
    EXPECT_EQ( inner, 0 );

    window.Add( 2 );  // 下一事件：新监听生效
    EXPECT_EQ( outer, 2 );
    EXPECT_EQ( inner, 1 );
}

TEST( CountWindowTest, ZeroCapacityPassesThrough ) {
    CountWindow<int> window( 0 );  // 容量 0：数据即进即出
    std::vector<int> added;
    std::vector<int> removed;
    window.OnAdd( [&added]( const SlidingWindow<int>::ItemList &items ) {
        added.insert( added.end(), items.begin(), items.end() );
    } );
    window.OnRemove( [&removed]( const SlidingWindow<int>::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    window.Add( 1 );

    EXPECT_TRUE( window.Empty() );
    EXPECT_EQ( added, ( std::vector<int>{ 1 } ) );
    EXPECT_EQ( removed, ( std::vector<int>{ 1 } ) );
}
