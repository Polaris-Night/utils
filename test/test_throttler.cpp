#include <thread>
#include "Throttler.h"
#include "gtest/gtest.h"

using namespace utils;

int  call_count = 0;
void test_function() {
    call_count++;
}

int test_function_with_return( int x ) {
    return x * 2;
}

TEST( ThrottlerTest, RunWithinInterval ) {
    Throttler throttler( std::chrono::milliseconds( 100 ) );
    call_count = 0;

    bool is_run = throttler.Run( test_function );
    EXPECT_TRUE( is_run );
    EXPECT_EQ( call_count, 1 );

    is_run = throttler.Run( test_function );
    EXPECT_FALSE( is_run );
    EXPECT_EQ( call_count, 1 );

    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
    is_run = throttler.Run( test_function );
    EXPECT_TRUE( is_run );
    EXPECT_EQ( call_count, 2 );
}

TEST( ThrottlerTest, RunOutsideInterval ) {
    Throttler throttler( std::chrono::milliseconds( 100 ) );
    call_count = 0;

    bool is_run = throttler.Run( test_function );
    EXPECT_TRUE( is_run );
    EXPECT_EQ( call_count, 1 );

    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
    is_run = throttler.Run( test_function );
    EXPECT_TRUE( is_run );
    EXPECT_EQ( call_count, 2 );
}

TEST( ThrottlerTest, RunWithReturn ) {
    Throttler throttler( std::chrono::milliseconds( 100 ) );

    auto pair = throttler.Run( test_function_with_return, 5 );
    EXPECT_TRUE( pair.first );
    EXPECT_EQ( pair.second.value(), 10 );

    pair = throttler.Run( test_function_with_return, 10 );
    EXPECT_FALSE( pair.first );
    EXPECT_EQ( pair.second, std::nullopt );

    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
    pair = throttler.Run( test_function_with_return, 10 );
    EXPECT_TRUE( pair.first );
    EXPECT_EQ( pair.second.value(), 20 );

    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );
    auto p2 = throttler.Run( []() -> std::optional<int> { return std::nullopt; } );
    EXPECT_TRUE( p2.first );
    EXPECT_EQ( p2.second.value(), std::nullopt );
}