#include <string>
#include "Result.h"
#include "gtest/gtest.h"

using namespace utils;

// 测试基本的Result<int, std::string>功能
TEST( ResultTest, BasicIntStringResult ) {
    // 测试构造Ok值
    auto result_ok = MakeOk<int, std::string>( 42 );
    EXPECT_TRUE( result_ok.IsOk() );
    EXPECT_FALSE( result_ok.IsErr() );
    EXPECT_EQ( result_ok.Value(), 42 );

    // 测试构造Err值
    auto result_err = MakeErr<int, std::string>( "error message" );
    EXPECT_FALSE( result_err.IsOk() );
    EXPECT_TRUE( result_err.IsErr() );
    EXPECT_EQ( result_err.Error(), "error message" );
}

// 测试Result<void, std::string>功能
TEST( ResultTest, BasicVoidStringResult ) {
    // 测试构造Ok值(void特化)
    auto result_ok = MakeOk<std::string>();
    EXPECT_TRUE( result_ok.IsOk() );
    EXPECT_FALSE( result_ok.IsErr() );

    // 测试构造Err值(void特化)
    auto result_err = MakeErr<void, std::string>( "error message" );
    EXPECT_FALSE( result_err.IsOk() );
    EXPECT_TRUE( result_err.IsErr() );
    EXPECT_EQ( result_err.Error(), "error message" );
}

// 测试ValueOr和ErrorOr方法
TEST( ResultTest, ValueOrAndErrorOr ) {
    // 测试ValueOr在Ok情况下的行为
    auto result_ok = MakeOk<int, std::string>( 42 );
    EXPECT_EQ( result_ok.ValueOr( 0 ), 42 );

    // 测试ValueOr在Err情况下的行为
    auto result_err = MakeErr<int, std::string>( "error" );
    EXPECT_EQ( result_err.ValueOr( 100 ), 100 );

    // 测试ErrorOr在Err情况下的行为
    EXPECT_EQ( result_err.ErrorOr( "default" ), "error" );

    // 测试ErrorOr在Ok情况下的行为
    auto result_ok2 = MakeOk<int, std::string>( 42 );
    EXPECT_EQ( result_ok2.ErrorOr( "default" ), "default" );
}

// 测试Map方法
TEST( ResultTest, MapOperation ) {
    // 测试Map在Ok情况下的行为
    auto result_ok     = MakeOk<int, std::string>( 42 );
    auto mapped_result = result_ok.Map( []( int x ) { return x * 2; } );
    EXPECT_TRUE( mapped_result.IsOk() );
    EXPECT_EQ( mapped_result.Value(), 84 );

    // 测试Map在Err情况下的行为
    auto result_err        = MakeErr<int, std::string>( "error" );
    auto mapped_result_err = result_err.Map( []( int x ) { return x * 2; } );
    EXPECT_TRUE( mapped_result_err.IsErr() );
    EXPECT_EQ( mapped_result_err.Error(), "error" );
}

// 测试Map方法对于void特化的版本
TEST( ResultTest, MapOperationVoid ) {
    // 测试Map在Ok情况下的行为(void特化)
    auto result_ok     = MakeOk<std::string>();
    bool called        = false;
    auto mapped_result = result_ok.Map( [&called]() { called = true; } );
    EXPECT_TRUE( mapped_result.IsOk() );
    EXPECT_FALSE( called );  // 注意：Map不会执行传入的函数，只是返回一个新的Ok Result

    // 测试Map在Err情况下的行为(void特化)
    auto result_err        = MakeErr<void, std::string>( "error" );
    auto mapped_result_err = result_err.Map( []() {} );
    EXPECT_TRUE( mapped_result_err.IsErr() );
    EXPECT_EQ( mapped_result_err.Error(), "error" );
}

// 测试AndThen方法
TEST( ResultTest, AndThenOperation ) {
    // 测试AndThen在Ok情况下的行为
    auto result_ok      = MakeOk<int, std::string>( 21 );
    auto chained_result = result_ok.AndThen( []( int x ) { return MakeOk<int, std::string>( x * 2 ); } );
    EXPECT_TRUE( chained_result.IsOk() );
    EXPECT_EQ( chained_result.Value(), 42 );

    // 测试AndThen在Err情况下的行为
    auto result_err         = MakeErr<int, std::string>( "error" );
    auto chained_result_err = result_err.AndThen( []( int x ) { return MakeOk<int, std::string>( x * 2 ); } );
    EXPECT_TRUE( chained_result_err.IsErr() );
    EXPECT_EQ( chained_result_err.Error(), "error" );
}

// 测试AndThen方法对于void特化的版本
TEST( ResultTest, AndThenOperationVoid ) {
    // 测试AndThen在Ok情况下的行为(void特化)
    auto result_ok      = MakeOk<std::string>();
    auto chained_result = result_ok.AndThen( []() { return MakeOk<std::string>(); } );
    EXPECT_TRUE( chained_result.IsOk() );

    // 测试AndThen在Err情况下的行为(void特化)
    auto result_err         = MakeErr<void, std::string>( "error" );
    auto chained_result_err = result_err.AndThen( []() { return MakeOk<std::string>(); } );
    EXPECT_TRUE( chained_result_err.IsErr() );
    EXPECT_EQ( chained_result_err.Error(), "error" );
}

// 测试异常情况
TEST( ResultTest, ExceptionCases ) {
    // 测试访问Err的Value会抛出异常
    auto result_err = MakeErr<int, std::string>( "error" );
    EXPECT_THROW( result_err.Value(), std::bad_variant_access );

    // 测试访问Ok的Error会抛出异常
    auto result_ok = MakeOk<int, std::string>( 42 );
    EXPECT_THROW( result_ok.Error(), std::bad_variant_access );

    // 测试void特化中访问Ok的Error会抛出异常
    auto void_result_ok = MakeOk<std::string>();
    EXPECT_THROW( void_result_ok.Error(), std::bad_variant_access );

    // 测试void特化中访问Err的Error正常工作
    auto void_result_err = MakeErr<void, std::string>( "error" );
    EXPECT_NO_THROW( void_result_err.Error() );
    EXPECT_EQ( void_result_err.Error(), "error" );
}

// 测试不同类型组合
TEST( ResultTest, DifferentTypeCombinations ) {
    // 测试Result<std::string, int>
    auto string_result = MakeOk<std::string, int>( "success" );
    EXPECT_TRUE( string_result.IsOk() );
    EXPECT_EQ( string_result.Value(), "success" );

    auto string_error = MakeErr<std::string, int>( 404 );
    EXPECT_TRUE( string_error.IsErr() );
    EXPECT_EQ( string_error.Error(), 404 );

    // 测试Result<bool, double>
    auto bool_result = MakeOk<bool, double>( true );
    EXPECT_TRUE( bool_result.IsOk() );
    EXPECT_TRUE( bool_result.Value() );

    auto bool_error = MakeErr<bool, double>( 3.14 );
    EXPECT_TRUE( bool_error.IsErr() );
    EXPECT_DOUBLE_EQ( bool_error.Error(), 3.14 );
}