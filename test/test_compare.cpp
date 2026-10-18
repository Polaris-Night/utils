#include <limits>
#include "Compare.h"
#include "gtest/gtest.h"

TEST( CompareTest, generalCompare ) {
    int a = 9999999;
    int b = 9999998;
    EXPECT_FALSE( utils::IsEqual( a, b ) );
    
    a = 123456;
    b = 123457;
    EXPECT_FALSE( utils::IsEqual( a, b ) );

    a = static_cast<int>( 3.14 );  // 截断为 3，验证整型比较
    b = 3;
    EXPECT_TRUE( utils::IsEqual( a, b ) );
}

TEST( CompareTest, doubleCompare ) {
    double a = 3.1234567890123414;
    double b = 3.1234567890123411;
    EXPECT_TRUE( utils::IsEqual( a, b ) );

    a = 3.1234567890123415;
    b = 3.1234567890123411;
    EXPECT_FALSE( utils::IsEqual( a, b ) );

    a = 3.1234567890123414;
    b = 3.1234567890123415;
    EXPECT_FALSE( utils::IsEqual( a, b ) );
}

TEST( CompareTest, floatCompare ) {
    float a = 3.1234566f;
    float b = 3.1234567f;
    EXPECT_TRUE( utils::IsEqual( a, b ) );

    a = 3.12345699f;
    b = 3.12345701f;
    EXPECT_TRUE( utils::IsEqual( a, b ) );

    a = 3.1234564f;
    b = 3.1234567f;
    EXPECT_FALSE( utils::IsEqual( a, b ) );
}

TEST( CompareTest, isCloseLargeValues ) {
    // 回归测试：大数值场景下相对容差判断（IsEqual 的绝对容差会失效）
    EXPECT_TRUE( utils::IsClose( 1e15, 1e15 + 1.0 ) );
    EXPECT_FALSE( utils::IsClose( 1e15, 1e15 + 100.0 ) );
    EXPECT_TRUE( utils::IsClose( 1e15, 1e15 + 100.0, 1e-9 ) );
    EXPECT_FALSE( utils::IsClose( 1.0, 1.1 ) );
}

TEST( CompareTest, isCloseNearZero ) {
    // 接近 0 的数值使用绝对容差
    EXPECT_FALSE( utils::IsClose( 0.0, 1e-15 ) );
    EXPECT_TRUE( utils::IsClose( 0.0, 1e-15, 1e-9, 1e-14 ) );
    EXPECT_TRUE( utils::IsClose( 0.0, 0.0 ) );

    // 完全相同与特殊值
    EXPECT_TRUE( utils::IsClose( 3.14, 3.14 ) );
    EXPECT_FALSE( utils::IsClose( std::numeric_limits<double>::infinity(), 1.0 ) );
    EXPECT_TRUE( utils::IsClose( std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity() ) );
    EXPECT_FALSE( utils::IsClose( std::numeric_limits<double>::quiet_NaN(), 1.0 ) );
}

TEST( CompareTest, isCloseFloat ) {
    EXPECT_TRUE( utils::IsClose( 1.0f, 1.0f + 1e-7f ) );
    EXPECT_FALSE( utils::IsClose( 1.0f, 1.01f ) );
}