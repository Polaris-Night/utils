#include <thread>
#include <vector>
#include "ConsumableOptional.h"
#include "gtest/gtest.h"

using namespace utils;

TEST( ConsumableOptionalTest, DefaultConstructor_NoValue ) {
    ConsumableOptional<int> co;
    EXPECT_FALSE( co.has_value() );
}

TEST( ConsumableOptionalTest, ConstRefConstructor_HasValue ) {
    std::string                     value = "hello";
    ConsumableOptional<std::string> co( value );
    EXPECT_TRUE( co.has_value() );
    EXPECT_EQ( co.comsume(), "hello" );
    EXPECT_FALSE( co.has_value() );
}

TEST( ConsumableOptionalTest, ForwardConstructor_HasValue ) {
    ConsumableOptional<int> co( 42 );
    EXPECT_TRUE( co.has_value() );
    EXPECT_EQ( co.comsume(), 42 );
    EXPECT_FALSE( co.has_value() );
}

TEST( ConsumableOptionalTest, MoveConstructor_HasValue ) {
    ConsumableOptional<std::string> co( "hello" );
    EXPECT_TRUE( co.has_value() );
    EXPECT_EQ( co.comsume(), "hello" );
    EXPECT_FALSE( co.has_value() );
}

TEST( ConsumableOptionalTest, SetMethod_ValueSet ) {
    ConsumableOptional<int> co;
    co.set( 100 );
    EXPECT_TRUE( co.has_value() );
    EXPECT_EQ( co.comsume(), 100 );
    EXPECT_FALSE( co.has_value() );
}

TEST( ConsumableOptionalTest, Consume_NoValue_ReturnsNullopt ) {
    ConsumableOptional<int> co;
    EXPECT_EQ( co.comsume(), std::nullopt );
}

TEST( ConsumableOptionalTest, ThreadSafety_ConcurrentAccess ) {
    ConsumableOptional<int>  co( 10, true );
    std::vector<std::thread> threads;
    std::mutex               result_mutex;
    std::vector<int>         results;

    for ( int i = 0; i < 10; ++i ) {
        threads.emplace_back( [&co, &result_mutex, &results]() {
            auto value = co.comsume();
            if ( value ) {
                std::lock_guard<std::mutex> lock( result_mutex );
                results.push_back( *value );
            }
        } );
    }

    for ( auto &thread : threads ) {
        thread.join();
    }

    EXPECT_EQ( results.size(), 1 );
    EXPECT_EQ( results[0], 10 );
}

TEST( ConsumableOptionalTest, BoolOperator_HasValue ) {
    ConsumableOptional<int> co( 42 );
    EXPECT_TRUE( static_cast<bool>( co ) );
}

TEST( ConsumableOptionalTest, BoolOperator_NoValue ) {
    ConsumableOptional<int> co;
    EXPECT_FALSE( static_cast<bool>( co ) );
}

TEST( ConsumableOptionalTest, Reset ) {
    ConsumableOptional<int> co( 30 );
    co.reset();
    EXPECT_FALSE( static_cast<bool>( co ) );
}