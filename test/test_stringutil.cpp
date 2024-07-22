#include <string>
#include <vector>
#include "StringUtil.h"
#include "gtest/gtest.h"

TEST( StringUtilTest, Split ) {
    std::vector<std::string> expect_result{ "1", "2", "3", "-", "/", "&" };
    std::vector<std::string> result;
    result = utils::StringUtil::Split( "1 2 3 - / &", " " );
    EXPECT_EQ( result, expect_result );
}

TEST( StringUtilTest, Join ) {
    std::string result;
    result = utils::StringUtil::Join( "/", "1", std::string( "2" ), std::string_view( "3" ) );
    EXPECT_EQ( result, "1/2/3" );

    std::vector<std::string> s_vect;
    s_vect.emplace_back( "1" );
    s_vect.emplace_back( "2" );
    s_vect.emplace_back( "3" );
    result = utils::StringUtil::Join( ",", s_vect );
    EXPECT_EQ( result, "1,2,3" );

    std::set<std::string> s_set;
    s_set.insert( "1" );
    s_set.insert( "1" );
    s_set.insert( "2" );
    result = utils::StringUtil::Join( "-", s_set );
    EXPECT_EQ( result, "1-2" );
}

TEST( StringUtilTest, Repeat ) {
    std::string result;

    result = utils::StringUtil::Repeat( "1/", 5 );
    EXPECT_EQ( result, "1/1/1/1/1/" );
}

TEST( StringUtilTest, ToLowerAndUpper ) {
    std::string result;

    result = utils::StringUtil::ToLower( "ABC-Def,G/h-123" );
    EXPECT_EQ( result, "abc-def,g/h-123" );
    result = utils::StringUtil::ToUpper( result );
    EXPECT_EQ( result, "ABC-DEF,G/H-123" );
}

TEST( StringUtilTest, StartAndEndWith ) {
    std::string str = "aBcdeF";
    EXPECT_TRUE( utils::StringUtil::StartWith( str, "aBc" ) );
    EXPECT_FALSE( utils::StringUtil::StartWith( str, "abc" ) );

    EXPECT_TRUE( utils::StringUtil::EndWith( str, "deF" ) );
    EXPECT_TRUE( utils::StringUtil::EndWith( str, "BcdeF" ) );
    EXPECT_FALSE( utils::StringUtil::EndWith( str, "aBcdef" ) );
}

TEST( StringUtilTest, Contains ) {
    std::string str = "testContains-googleTest";
    EXPECT_TRUE( utils::StringUtil::Contains( str, "-googleTest" ) );
    EXPECT_TRUE( utils::StringUtil::Contains( str, "Contains" ) );
    EXPECT_FALSE( utils::StringUtil::Contains( str, "contains" ) );
    EXPECT_FALSE( utils::StringUtil::Contains( str, "test-googleTest" ) );
}

TEST( StringUtilTest, ConvertToHex ) {
    EXPECT_EQ( utils::StringUtil::ConvertToHexStr( "1234567890" ), "31 32 33 34 35 36 37 38 39 30" );
    EXPECT_EQ( utils::StringUtil::ConvertToHexStr( "abcABC" ), "61 62 63 41 42 43" );
    EXPECT_EQ( utils::StringUtil::ConvertToHexStr( ".-/&#" ), "2e 2d 2f 26 23" );
    EXPECT_EQ( utils::StringUtil::ToUpper( utils::StringUtil::ConvertToHexStr( ".-/&#" ) ), "2E 2D 2F 26 23" );
}

TEST( StringUtilTest, IntToBitString ) {
    EXPECT_EQ( utils::StringUtil::IntToBitString( 3u, 8 ), "00000011" );
    EXPECT_EQ( utils::StringUtil::IntToBitString( 4u, 8 ), "00000100" );
    EXPECT_EQ( utils::StringUtil::IntToBitString( -1, 8 ), "11111111" );
    EXPECT_EQ( utils::StringUtil::IntToBitString( 1234, 32 ), "00000000000000000000010011010010" );
    EXPECT_EQ( utils::StringUtil::IntToBitString( 0x33, 32 ), "00000000000000000000000000110011" );
}
