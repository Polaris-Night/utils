#include "Compare.h"
#include "gtest/gtest.h"

TEST( CompareTest, generalCompare ) {
    int a = 9999999;
    int b = 9999998;
    EXPECT_FALSE( utils::IsEqual( a, b ) );
    
    a = 123456;
    b = 123457;
    EXPECT_FALSE( utils::IsEqual( a, b ) );

    a = 3.14;
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
    float a = 3.1234566;
    float b = 3.1234567;
    EXPECT_TRUE( utils::IsEqual( a, b ) );

    a = 3.12345699;
    b = 3.12345701;
    EXPECT_TRUE( utils::IsEqual( a, b ) );

    a = 3.1234564;
    b = 3.1234567;
    EXPECT_FALSE( utils::IsEqual( a, b ) );
}