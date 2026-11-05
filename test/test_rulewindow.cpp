#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <vector>
#include "SlidingWindow.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

using SteadyPoint    = std::chrono::steady_clock::time_point;
using SteadyDuration = std::chrono::seconds;

SteadyPoint At( int seconds ) {
    return SteadyPoint{} + std::chrono::seconds( seconds );
}

using IntWindow = RuleWindow<int, SteadyPoint>;
using IntPolicy = IntWindow::Policy;

IntPolicy CountPolicy( std::size_t count ) {
    IntPolicy policy;
    policy.count  = count;
    policy.key_fn = []( const int &value ) { return At( value ); };
    return policy;
}

}  // namespace

TEST( RuleWindowTest, CountLimitKeepsMostRecent ) {
    IntWindow window( CountPolicy( 3 ) );
    std::vector<int> removed;
    window.OnRemove( [&removed]( const IntWindow::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    for ( int i = 1; i <= 5; ++i ) {
        window.Add( i );
    }

    EXPECT_EQ( window.Size(), 3u );
    EXPECT_EQ( window.Items(), ( std::vector<int>{ 3, 4, 5 } ) );
    EXPECT_EQ( removed, ( std::vector<int>{ 1, 2 } ) );
}

TEST( RuleWindowTest, TimeLimitEvictsOnAdd ) {
    IntPolicy policy;
    policy.span   = SteadyDuration( 10 );
    policy.key_fn = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    window.Add( 5 );
    window.Add( 10 );
    window.Add( 20 );  // cutoff = 10 → 5 被淘汰

    EXPECT_EQ( window.Items(), ( std::vector<int>{ 10, 20 } ) );
    EXPECT_EQ( *window.LatestTime(), At( 20 ) );
}

TEST( RuleWindowTest, PollEvictsWithoutNewData ) {
    IntPolicy policy;
    policy.span   = SteadyDuration( 10 );
    policy.key_fn = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    std::vector<int> removed;
    window.OnRemove( [&removed]( const IntWindow::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    window.Add( 5 );
    window.Add( 6 );
    EXPECT_EQ( window.Size(), 2u );

    EXPECT_EQ( window.Poll( At( 100 ) ), 2u );  // 空闲：仅推进时间即可淘汰

    EXPECT_EQ( window.Size(), 0u );
    EXPECT_EQ( removed, ( std::vector<int>{ 5, 6 } ) );
    EXPECT_FALSE( window.LatestTime().has_value() );
}

TEST( RuleWindowTest, TimeAndCountCombined ) {
    IntPolicy policy;
    policy.span   = SteadyDuration( 100 );
    policy.count  = 2;
    policy.key_fn = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    window.Add( 1 );
    window.Add( 2 );
    window.Add( 3 );  // 时间未越界，但条数超限 → 淘汰最旧

    EXPECT_EQ( window.Items(), ( std::vector<int>{ 2, 3 } ) );
}

TEST( RuleWindowTest, WeightAndCountCombined ) {
    IntPolicy policy;
    policy.count     = 10;
    policy.weight    = 5.0;
    policy.weight_fn = []( const int &value ) { return static_cast<double>( value ); };
    policy.key_fn    = []( const int &value ) { return At( value ); };

    IntWindow window( policy );
    window.Add( 3 );
    window.Add( 3 );  // 权重 6 > 5 → 淘汰最旧的 3

    EXPECT_EQ( window.Size(), 1u );
    EXPECT_DOUBLE_EQ( window.TotalWeight(), 3.0 );
    EXPECT_EQ( window.Items(), ( std::vector<int>{ 3 } ) );
}

TEST( RuleWindowTest, OutOfOrderInsertStaysSorted ) {
    IntWindow window( CountPolicy( 10 ) );
    window.Add( 20 );
    window.Add( 10 );
    window.Add( 15 );

    EXPECT_EQ( window.Items(), ( std::vector<int>{ 10, 15, 20 } ) );
    EXPECT_EQ( *window.LatestTime(), At( 20 ) );
}

TEST( RuleWindowTest, ClearNotifiesRemoval ) {
    IntWindow window( CountPolicy( 10 ) );
    std::vector<int> removed;
    window.OnRemove( [&removed]( const IntWindow::ItemList &items ) {
        removed.insert( removed.end(), items.begin(), items.end() );
    } );

    window.Add( 1 );
    window.Add( 2 );
    window.Clear();

    EXPECT_EQ( window.Size(), 0u );
    EXPECT_DOUBLE_EQ( window.TotalWeight(), 0.0 );
    EXPECT_EQ( removed, ( std::vector<int>{ 1, 2 } ) );
}

TEST( RuleWindowTest, PollWithoutTimeDimensionIsNoOp ) {
    IntWindow window( CountPolicy( 10 ) );
    window.Add( 1 );
    window.Add( 2 );

    window.Poll( At( 10000 ) );

    EXPECT_EQ( window.Size(), 2u );
}

TEST( RuleWindowTest, InvalidPolicyThrows ) {
    IntPolicy empty;  // 没有任何维度
    EXPECT_THROW( IntWindow{ empty }, std::invalid_argument );

    IntPolicy with_predicate;
    with_predicate.count     = 1;
    with_predicate.predicate = []( const int & ) { return true; };
    EXPECT_THROW( IntWindow{ with_predicate }, std::invalid_argument );
}
