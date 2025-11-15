#include <iostream>
#include <string>
#include <vector>
#include "StringUtil.h"
#include "gtest/gtest.h"

TEST( StringUtilTest, Split ) {
    // 测试Split基本功能
    std::vector<std::string> expect_result{ "1", "2", "3", "-", "/", "&" };
    std::vector<std::string> result;
    result = utils::StringUtil::Split( "1 2 3 - / &", " " );
    EXPECT_EQ( result, expect_result );

    // 测试SplitRef基本功能
    std::vector<std::string_view> expect_result_sv{ "1", "2", "3", "-", "/", "&" };
    std::vector<std::string_view> result_sv = utils::StringUtil::SplitRef( "1 2 3 - / &", " " );
    EXPECT_EQ( result_sv, expect_result_sv );

    // 测试空分隔符的情况
    std::vector<std::string> expect_result_empty_sep{ "", "a", "b", "c", "" };
    std::vector<std::string> result_empty_sep = utils::StringUtil::Split( "abc", "" );
    EXPECT_EQ( result_empty_sep, expect_result_empty_sep );

    // 测试SplitRef使用空分隔符
    std::vector<std::string_view> expect_result_empty_sep_sv{ "", "a", "b", "c", "" };
    std::vector<std::string_view> result_empty_sep_sv = utils::StringUtil::SplitRef( "abc", "" );
    EXPECT_EQ( result_empty_sep_sv, expect_result_empty_sep_sv );

    // 测试空分隔符且跳过空项的情况
    std::vector<std::string> expect_result_empty_sep_skip{ "a", "b", "c" };
    std::vector<std::string> result_empty_sep_skip = utils::StringUtil::Split( "abc", "", true );
    EXPECT_EQ( result_empty_sep_skip, expect_result_empty_sep_skip );

    // 测试SplitRef使用空分隔符且跳过空项
    std::vector<std::string_view> expect_result_empty_sep_skip_sv{ "a", "b", "c" };
    std::vector<std::string_view> result_empty_sep_skip_sv = utils::StringUtil::SplitRef( "abc", "", true );
    EXPECT_EQ( result_empty_sep_skip_sv, expect_result_empty_sep_skip_sv );

    // 测试连续分隔符的情况
    std::vector<std::string> expect_result_consecutive{ "a", "", "b", "c" };
    std::vector<std::string> result_consecutive = utils::StringUtil::Split( "a,,b,c", "," );
    EXPECT_EQ( result_consecutive, expect_result_consecutive );

    // 测试SplitRef处理连续分隔符
    std::vector<std::string_view> expect_result_consecutive_sv{ "a", "", "b", "c" };
    std::vector<std::string_view> result_consecutive_sv = utils::StringUtil::SplitRef( "a,,b,c", "," );
    EXPECT_EQ( result_consecutive_sv, expect_result_consecutive_sv );

    // 测试连续分隔符且跳过空项的情况
    std::vector<std::string> expect_result_consecutive_skip{ "a", "b", "c" };
    std::vector<std::string> result_consecutive_skip = utils::StringUtil::Split( "a,,b,c", ",", true );
    EXPECT_EQ( result_consecutive_skip, expect_result_consecutive_skip );

    // 测试SplitRef处理连续分隔符且跳过空项
    std::vector<std::string_view> expect_result_consecutive_skip_sv{ "a", "b", "c" };
    std::vector<std::string_view> result_consecutive_skip_sv = utils::StringUtil::SplitRef( "a,,b,c", ",", true );
    EXPECT_EQ( result_consecutive_skip_sv, expect_result_consecutive_skip_sv );

    // 测试末尾有分隔符的情况
    std::vector<std::string> expect_result_trailing{ "a", "b", "" };
    std::vector<std::string> result_trailing = utils::StringUtil::Split( "a,b,", "," );
    EXPECT_EQ( result_trailing, expect_result_trailing );

    // 测试SplitRef处理末尾有分隔符的情况
    std::vector<std::string_view> expect_result_trailing_sv{ "a", "b", "" };
    std::vector<std::string_view> result_trailing_sv = utils::StringUtil::SplitRef( "a,b,", "," );
    EXPECT_EQ( result_trailing_sv, expect_result_trailing_sv );

    // 测试末尾有分隔符且跳过空项的情况
    std::vector<std::string> expect_result_trailing_skip{ "a", "b" };
    std::vector<std::string> result_trailing_skip = utils::StringUtil::Split( "a,b,", ",", true );
    EXPECT_EQ( result_trailing_skip, expect_result_trailing_skip );

    // 测试SplitRef处理末尾有分隔符且跳过空项
    std::vector<std::string_view> expect_result_trailing_skip_sv{ "a", "b" };
    std::vector<std::string_view> result_trailing_skip_sv = utils::StringUtil::SplitRef( "a,b,", ",", true );
    EXPECT_EQ( result_trailing_skip_sv, expect_result_trailing_skip_sv );
}

TEST( StringUtilTest, Substrings ) {
    // 测试Left和LeftRef函数
    EXPECT_EQ( utils::StringUtil::Left( "hello world", 5 ), "hello" );
    EXPECT_EQ( utils::StringUtil::Left( "hello", 10 ), "hello" );
    EXPECT_EQ( utils::StringUtil::Left( "hello", 0 ), "" );
    EXPECT_EQ( utils::StringUtil::Left( "", 5 ), "" );

    EXPECT_EQ( utils::StringUtil::LeftRef( "hello world", 5 ), "hello" );
    EXPECT_EQ( utils::StringUtil::LeftRef( "hello", 10 ), "hello" );
    EXPECT_EQ( utils::StringUtil::LeftRef( "hello", 0 ), "" );
    EXPECT_EQ( utils::StringUtil::LeftRef( "", 5 ), "" );

    // 测试Mid和MidRef函数
    EXPECT_EQ( utils::StringUtil::Mid( "hello world", 6 ), "world" );
    EXPECT_EQ( utils::StringUtil::Mid( "hello world", 6, 3 ), "wor" );
    EXPECT_EQ( utils::StringUtil::Mid( "hello world", 0, 5 ), "hello" );
    EXPECT_EQ( utils::StringUtil::Mid( "hello world", 20 ), "" );
    EXPECT_EQ( utils::StringUtil::Mid( "hello", 1, 3 ), "ell" );
    EXPECT_EQ( utils::StringUtil::Mid( "", 0, 5 ), "" );

    EXPECT_EQ( utils::StringUtil::MidRef( "hello world", 6 ), "world" );
    EXPECT_EQ( utils::StringUtil::MidRef( "hello world", 6, 3 ), "wor" );
    EXPECT_EQ( utils::StringUtil::MidRef( "hello world", 0, 5 ), "hello" );
    EXPECT_EQ( utils::StringUtil::MidRef( "hello world", 20 ), "" );
    EXPECT_EQ( utils::StringUtil::MidRef( "hello", 1, 3 ), "ell" );
    EXPECT_EQ( utils::StringUtil::MidRef( "", 0, 5 ), "" );

    // 测试Right和RightRef函数
    EXPECT_EQ( utils::StringUtil::Right( "hello world", 5 ), "world" );
    EXPECT_EQ( utils::StringUtil::Right( "hello", 10 ), "hello" );
    EXPECT_EQ( utils::StringUtil::Right( "hello", 0 ), "" );
    EXPECT_EQ( utils::StringUtil::Right( "", 5 ), "" );
    EXPECT_EQ( utils::StringUtil::Right( "hello world", 4 ), "orld" );

    EXPECT_EQ( utils::StringUtil::RightRef( "hello world", 5 ), "world" );
    EXPECT_EQ( utils::StringUtil::RightRef( "hello", 10 ), "hello" );
    EXPECT_EQ( utils::StringUtil::RightRef( "hello", 0 ), "" );
    EXPECT_EQ( utils::StringUtil::RightRef( "", 5 ), "" );
    EXPECT_EQ( utils::StringUtil::RightRef( "hello world", 4 ), "orld" );
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

TEST( StringUtilTest, Trim ) {
    // 测试基本的前后空格去除
    EXPECT_EQ( utils::StringUtil::Trim( "  hello  " ), "hello" );
    EXPECT_EQ( utils::StringUtil::Trim( "  hello" ), "hello" );
    EXPECT_EQ( utils::StringUtil::Trim( "hello  " ), "hello" );

    // 测试不包含空格的字符串
    EXPECT_EQ( utils::StringUtil::Trim( "hello" ), "hello" );

    // 测试只包含空格的字符串
    EXPECT_EQ( utils::StringUtil::Trim( "   " ), "" );
    EXPECT_EQ( utils::StringUtil::Trim( "\t\n\r " ), "" );

    // 测试包含各种空白字符的字符串
    EXPECT_EQ( utils::StringUtil::Trim( "\t\n\r hello \t\n\r" ), "hello" );

    // 测试空字符串
    EXPECT_EQ( utils::StringUtil::Trim( "" ), "" );

    // 测试特殊字符串，如十六进制表示
    EXPECT_EQ( utils::StringUtil::Trim( "0xFF" ), "0xFF" );
    EXPECT_EQ( utils::StringUtil::Trim( " 0xFF " ), "0xFF" );

    // 测试只有前导空格
    EXPECT_EQ( utils::StringUtil::Trim( "  test" ), "test" );

    // 测试只有尾随空格
    EXPECT_EQ( utils::StringUtil::Trim( "test  " ), "test" );
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

TEST( StringUtilTest, Extract ) {
    // 基本测试用例
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abc-def-ghi", "abc", "ghi" ), std::string_view( "-def-" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abc-def-ghi", "-", "-", true, true ), std::string_view( "-def-" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abc-def-ghi", "abc-", "-ghi" ), std::string_view( "def" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abc", "cd", "ef" ), std::nullopt );
    EXPECT_EQ(
        utils::StringUtil::ExtractBetween(
            R"(CP=&&DataTime=20160801084000;w00000-Cou=10.5,w00000-Min=16.4,w00000-Avg=17.5,w00000-Max=20.1,w00000-Flag=N&&)",
            "CP=&&", "&&" ),
        std::string_view(
            R"(DataTime=20160801084000;w00000-Cou=10.5,w00000-Min=16.4,w00000-Avg=17.5,w00000-Max=20.1,w00000-Flag=N)" ) );

    // 测试空的开始和结束字符串
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "", "f" ), std::string_view( "abcde" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "a", "" ), std::string_view( "bcdef" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "", "" ), std::string_view( "abcdef" ) );

    // 测试包含开始和结束标记的情况
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "ab", "ef", true, true ), std::string_view( "abcdef" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "ab", "ef", true, false ), std::string_view( "abcd" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "ab", "ef", false, true ), std::string_view( "cdef" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "ab", "ef", false, false ), std::string_view( "cd" ) );

    // 测试找不到开始或结束字符串的情况
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "xyz", "def" ), std::nullopt );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "abc", "xyz" ), std::nullopt );

    // 测试开始位置在结束位置之后的情况
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcdef", "def", "abc" ), std::nullopt );

    // 测试相同开始和结束字符串的情况
    // 第一个匹配作为开始，第二个匹配作为结束
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcabcabc", "abc", "abc" ), std::string_view( "" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcabcabc", "abc", "abc", true, true ),
               std::string_view( "abcabc" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcabcabc", "abc", "abc", true, false ), std::string_view( "abc" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "abcabcabc", "abc", "abc", false, true ), std::string_view( "abc" ) );

    // 测试空字符串输入
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "", "", "" ), std::string_view( "" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "", "a", "b" ), std::nullopt );

    // 测试只有一个字符的情况
    // 当开始和结束字符串相同时，在同一个位置无法提取内容
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "a", "a", "a" ), std::nullopt );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "a", "a", "a", true, true ), std::nullopt );

    // 测试其他边界情况
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "test", "te", "st" ), std::string_view( "" ) );
    EXPECT_EQ( utils::StringUtil::ExtractBetween( "test", "te", "st", true, true ), std::string_view( "test" ) );
}

TEST( StringUtilTest, RegexExtractFirst ) {
    std::string error_msg;

    // 基本测试用例
    auto result = utils::StringUtil::ExtractFirst( "abc123def456", R"(\d+)", &error_msg );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "123" );
    EXPECT_TRUE( error_msg.empty() );
    result = utils::StringUtil::ExtractFirst( "456abc123", R"(\d+$)" );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "123" );

    // 测试没有匹配的情况
    result = utils::StringUtil::ExtractFirst( "abcdef", R"(\d+)", &error_msg );
    EXPECT_FALSE( result.has_value() );
    EXPECT_TRUE( error_msg.empty() );

    // 测试复杂正则表达式
    result = utils::StringUtil::ExtractFirst( "email: test@example.com",
                                              R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})", &error_msg );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "test@example.com" );
    EXPECT_TRUE( error_msg.empty() );

    // 测试无效正则表达式
    result = utils::StringUtil::ExtractFirst( "test", R"([)", &error_msg );
    EXPECT_FALSE( result.has_value() );
    EXPECT_FALSE( error_msg.empty() );
}

TEST( StringUtilTest, RegexExtractGroup ) {
    std::string error_msg;

    // 基本测试用例 - 提取第一组
    auto result = utils::StringUtil::ExtractGroup( "Version 1.2.3", R"((\d+)\.(\d+)\.(\d+))", 1, &error_msg );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "1" );
    EXPECT_TRUE( error_msg.empty() );

    // 提取第二组
    result = utils::StringUtil::ExtractGroup( "Version 1.2.3", R"((\d+)\.(\d+)\.(\d+))", 2, &error_msg );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "2" );
    EXPECT_TRUE( error_msg.empty() );

    // 提取第三组
    result = utils::StringUtil::ExtractGroup( "Version 1.2.3", R"((\d+)\.(\d+)\.(\d+))", 3, &error_msg );
    EXPECT_TRUE( result.has_value() );
    EXPECT_EQ( *result, "3" );
    EXPECT_TRUE( error_msg.empty() );

    // 测试组索引超出范围
    result = utils::StringUtil::ExtractGroup( "Version 1.2.3", R"((\d+)\.(\d+)\.(\d+))", 5, &error_msg );
    EXPECT_FALSE( result.has_value() );
    EXPECT_FALSE( error_msg.empty() );
    error_msg.clear();

    // 测试没有匹配的情况
    result = utils::StringUtil::ExtractGroup( "Version x.y.z", R"((\d+)\.(\d+)\.(\d+))", 1, &error_msg );
    EXPECT_FALSE( result.has_value() );
    EXPECT_TRUE( error_msg.empty() );

    // 测试无效正则表达式
    result = utils::StringUtil::ExtractGroup( "test", R"([)", 1, &error_msg );
    EXPECT_FALSE( result.has_value() );
    EXPECT_FALSE( error_msg.empty() );
}

TEST( StringUtilTest, RegexExtractAll ) {
    std::string error_msg;

    // 基本测试用例
    auto results = utils::StringUtil::ExtractAll( "abc123def456ghi789", R"(\d+)", &error_msg );
    EXPECT_EQ( results.size(), 3 );
    EXPECT_EQ( results[0], "123" );
    EXPECT_EQ( results[1], "456" );
    EXPECT_EQ( results[2], "789" );
    EXPECT_TRUE( error_msg.empty() );

    // 测试没有匹配的情况
    results = utils::StringUtil::ExtractAll( "abcdefghi", R"(\d+)", &error_msg );
    EXPECT_TRUE( results.empty() );
    EXPECT_TRUE( error_msg.empty() );

    // 测试提取单词
    results =
        utils::StringUtil::ExtractAll( "the quick brown fox jumps over the lazy dog", R"(\b\w{4}\b)", &error_msg );
    EXPECT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], "over" );
    EXPECT_EQ( results[1], "lazy" );
    EXPECT_TRUE( error_msg.empty() );

    // 测试无效正则表达式
    results = utils::StringUtil::ExtractAll( "test", R"([)", &error_msg );
    EXPECT_TRUE( results.empty() );
    EXPECT_FALSE( error_msg.empty() );
}

TEST( StringUtilTest, RegexExtractAllGroups ) {
    std::string error_msg;

    // 基本测试用例 - 提取日期中的年月日
    auto results = utils::StringUtil::ExtractAllGroups( "Date: 2023-12-25, Date: 2024-01-01",
                                                        R"((\d{4})-(\d{2})-(\d{2}))", 1, &error_msg );
    EXPECT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], "2023" );
    EXPECT_EQ( results[1], "2024" );
    EXPECT_TRUE( error_msg.empty() );

    // 提取月份
    results = utils::StringUtil::ExtractAllGroups( "Date: 2023-12-25, Date: 2024-01-01", R"((\d{4})-(\d{2})-(\d{2}))",
                                                   2, &error_msg );
    EXPECT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], "12" );
    EXPECT_EQ( results[1], "01" );
    EXPECT_TRUE( error_msg.empty() );

    // 提取日期
    results = utils::StringUtil::ExtractAllGroups( "Date: 2023-12-25, Date: 2024-01-01", R"((\d{4})-(\d{2})-(\d{2}))",
                                                   3, &error_msg );
    EXPECT_EQ( results.size(), 2 );
    EXPECT_EQ( results[0], "25" );
    EXPECT_EQ( results[1], "01" );
    EXPECT_TRUE( error_msg.empty() );

    // 测试没有匹配的情况
    results = utils::StringUtil::ExtractAllGroups( "Invalid date format", R"((\d{4})-(\d{2})-(\d{2}))", 1, &error_msg );
    EXPECT_TRUE( results.empty() );
    EXPECT_TRUE( error_msg.empty() );

    // 测试组索引超出范围
    results = utils::StringUtil::ExtractAllGroups( "Date: 2023-12-25", R"((\d{4})-(\d{2})-(\d{2}))", 5, &error_msg );
    EXPECT_TRUE( results.empty() );
    EXPECT_FALSE( error_msg.empty() );
    error_msg.clear();

    // 测试无效正则表达式
    results = utils::StringUtil::ExtractAllGroups( "test", R"([)", 1, &error_msg );
    EXPECT_TRUE( results.empty() );
    EXPECT_FALSE( error_msg.empty() );
}

TEST( StringUtilTest, ToNumber ) {
    // 测试整数转换
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "42" ).value_or( 0 ), 42 );
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "  42  " ).value_or( 0 ), 42 );
    EXPECT_EQ( utils::StringUtil::ToNumber<long>( "-123" ).value_or( 0 ), -123 );

    // 测试十六进制转换
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "0xFF" ).value_or( 0 ), 255 );
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "  0xFF " ).value_or( 0 ), 255 );
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "  0xfF " ).value_or( 0 ), 255 );
    EXPECT_FALSE( utils::StringUtil::ToNumber<int>( "fF" ).has_value() );

    // 测试八进制转换
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "077" ).value_or( 0 ), 63 );

    // 测试浮点数转换
    EXPECT_DOUBLE_EQ( utils::StringUtil::ToNumber<double>( "3.14" ).value_or( 0 ), 3.14 );
    EXPECT_FLOAT_EQ( utils::StringUtil::ToNumber<float>( "2.5" ).value_or( 0 ), 2.5f );
    EXPECT_FALSE( utils::StringUtil::ToNumber<float>( "2.5f" ).has_value() );

    // 测试空字符串
    EXPECT_FALSE( utils::StringUtil::ToNumber<int>( "" ).has_value() );

    // 测试无效字符
    EXPECT_FALSE( utils::StringUtil::ToNumber<int>( "abc" ).has_value() );

    // 测试部分匹配（应该失败）
    EXPECT_FALSE( utils::StringUtil::ToNumber<int>( "123abc" ).has_value() );
    EXPECT_FALSE( utils::StringUtil::ToNumber<int>( "12.34.56" ).has_value() );

    // 测试边界值
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "0" ).value_or( 0 ), 0 );
    EXPECT_EQ( utils::StringUtil::ToNumber<int>( "-0" ).value_or( 0 ), 0 );

    // 测试无符号类型
    EXPECT_EQ( utils::StringUtil::ToNumber<unsigned int>( "42" ).value_or( 0 ), 42u );

    // 验证ToNumber函数具有noexcept属性
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<int>( "42" ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<long>( "42" ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<unsigned int>( "42" ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<float>( "3.14" ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<double>( "3.14" ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::ToNumber<bool>( "1" ) ) );
}

TEST( StringUtilTest, FromNumber ) {
    // 测试整型转换
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 42 ), "42" );
    EXPECT_EQ( utils::StringUtil::FromNumber<long>( -123 ), "-123" );
    EXPECT_EQ( utils::StringUtil::FromNumber<unsigned int>( 42u ), "42" );

    // 测试不同进制的整型转换
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 42, 10 ), "42" );
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 42, 16 ), "2a" );
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 42, 8 ), "52" );
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 0, 16 ), "0" );

    // 测试非法进制
    EXPECT_TRUE( utils::StringUtil::FromNumber<int>( 42, 2 ).empty() );
    EXPECT_TRUE( utils::StringUtil::FromNumber<int>( 42, 7 ).empty() );
    EXPECT_TRUE( utils::StringUtil::FromNumber<int>( 42, 15 ).empty() );
    EXPECT_TRUE( utils::StringUtil::FromNumber<int>( 42, 17 ).empty() );

    // 测试浮点型转换
    EXPECT_EQ( utils::StringUtil::FromNumber<float>( 3.14f, 10, 6 ), "3.140000" );
    EXPECT_EQ( utils::StringUtil::FromNumber<double>( 3.14, 10, 6 ), "3.140000" );
    EXPECT_EQ( utils::StringUtil::FromNumber<double>( 3.14, 10, -1 ), "3.14" );
    EXPECT_EQ( utils::StringUtil::FromNumber<double>( 3.14, 10, 2 ), "3.14" );

    // 测试边界值
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( 0 ), "0" );
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( std::numeric_limits<int>::max() ),
               std::to_string( std::numeric_limits<int>::max() ) );
    EXPECT_EQ( utils::StringUtil::FromNumber<int>( std::numeric_limits<int>::min() ),
               std::to_string( std::numeric_limits<int>::min() ) );
    EXPECT_EQ( utils::StringUtil::FromNumber<unsigned>( std::numeric_limits<unsigned>::max() ),
               std::to_string( std::numeric_limits<unsigned>::max() ) );

    // 验证FromNumber函数具有noexcept属性
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<int>( 42 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<long>( 42 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<unsigned int>( 42 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<float>( 3.14f ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<double>( 3.14 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<bool>( true ) ) );

    // 进制参数不影响noexcept属性
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<int>( 42, 10 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<int>( 42, 16 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<int>( 42, 8 ) ) );

    // 精度参数不影响noexcept属性
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<double>( 3.14, 10, 2 ) ) );
    EXPECT_TRUE( noexcept( utils::StringUtil::FromNumber<float>( 3.14f, 10, -1 ) ) );
}

TEST( StringUtilTest, StringFormat ) {
    // 测试基本字符串格式化功能
    utils::StringFormat fmt1( "Hello %1, welcome to %2!" );
    std::string         result1 = fmt1.Args( "World", "C++" ).ToString();
    EXPECT_EQ( result1, "Hello World, welcome to C++!" );

    // 测试整数参数
    utils::StringFormat fmt2( "Value of x is %1 and y is %2" );
    std::string         result2 = fmt2.Args( 10, 20 ).ToString();
    EXPECT_EQ( result2, "Value of x is 10 and y is 20" );

    // 测试浮点数参数
    utils::StringFormat fmt3( "Pi is approximately %1" );
    std::string         result3 = fmt3.Args( 3.14159 ).ToString();
    EXPECT_EQ( result3, "Pi is approximately 3.14159" );

    // 测试布尔值参数
    utils::StringFormat fmt4( "Flag is %1" );
    std::string         result4 = fmt4.Args( true ).ToString();
    EXPECT_EQ( result4, "Flag is true" );

    // 测试ArgsF方法和精度控制
    utils::StringFormat fmt5( "Price: %1" );
    std::string         result5 = fmt5.ArgsF( 2, 12.3456 ).ToString();
    EXPECT_EQ( result5, "Price: 12.35" );

    // 测试多个不同类型的参数混合使用
    utils::StringFormat fmt6( "Name: %1, Age: %2, Height: %3, Student: %4" );
    std::string         result6 = fmt6.Args( "Alice", 25 ).ArgsF( 1, 1.75 ).Args( true ).ToString();
    EXPECT_EQ( result6, "Name: Alice, Age: 25, Height: 1.8, Student: true" );

    // 测试同一占位符多次出现的情况
    utils::StringFormat fmt7( "%1 is %2, %1 has %3 years old" );
    std::string         result7 = fmt7.Args( "Alice", "student", 25 ).ToString();
    EXPECT_EQ( result7, "Alice is student, Alice has 25 years old" );

    // 测试空格式字符串
    utils::StringFormat fmt8( "" );
    std::string         result8 = fmt8.ToString();
    EXPECT_EQ( result8, "" );

    // 测试没有占位符的字符串
    utils::StringFormat fmt9( "This is a plain text without placeholders" );
    std::string         result9 = fmt9.Args( "unused" ).ToString();
    EXPECT_EQ( result9, "This is a plain text without placeholders" );

    // 测试空参数
    utils::StringFormat fmt10( "Start %1 %2 End" );
    std::string         result10 = fmt10.Args( "" ).ToString();
    EXPECT_EQ( result10, "Start  %2 End" );
    EXPECT_EQ( fmt10.Args( "", "" ).ToString(), "Start   End" );

    // 测试多个参数混合使用
    utils::StringFormat fmt12( "%1 %2 %1 %3 %2 %1 %4 %5" );
    std::string         result12 = fmt12.Args( "a", "b", "c" ).ArgsF( 3, 3.14 ).Args( 55 ).ToString();
    EXPECT_EQ( result12, "a b a c b a 3.140 55" );
}

TEST( StringUtilTest, Vasprintf ) {
    // 测试基本的格式化功能
    std::string result = utils::StringUtil::FormatCString( "Hello %s", "World" );
    EXPECT_EQ( result, "Hello World" );

    // 测试整数格式化
    result = utils::StringUtil::FormatCString( "Value: %d", 42 );
    EXPECT_EQ( result, "Value: 42" );

    // 测试十六进制格式化
    result = utils::StringUtil::FormatCString( "Hex: %x", 255 );
    EXPECT_EQ( result, "Hex: ff" );

    // 测试浮点数格式化
    result = utils::StringUtil::FormatCString( "Float: %.2f", 3.14159 );
    EXPECT_EQ( result, "Float: 3.14" );

    // 测试多个参数
    result = utils::StringUtil::FormatCString( "Name: %s, Age: %d, Score: %.1f", "Alice", 25, 95.5 );
    EXPECT_EQ( result, "Name: Alice, Age: 25, Score: 95.5" );

    // 测试不同的数据类型
    result = utils::StringUtil::FormatCString( "Char: %c, Unsigned: %u", 'A', 123u );
    EXPECT_EQ( result, "Char: A, Unsigned: 123" );

    // 测试指针类型（输出格式可能因平台而异）
    result = utils::StringUtil::FormatCString( "Pointer: %p", nullptr );
    // 只验证不为空且不抛出异常
    EXPECT_FALSE( result.empty() );

    // 测试宽度和对齐
    result = utils::StringUtil::FormatCString( "Padded: %10s", "test" );
    EXPECT_EQ( result, "Padded:       test" );

    // 测试负数格式化
    result = utils::StringUtil::FormatCString( "Negative: %d", -42 );
    EXPECT_EQ( result, "Negative: -42" );

    // 测试空字符串格式化
    result = utils::StringUtil::FormatCString( "%s", "" );
    EXPECT_EQ( result, "" );

    // 测试格式化错误情况 - 应该抛出异常
    EXPECT_THROW( utils::StringUtil::FormatCString( "%", 42 ), std::runtime_error );

    // 测试复杂的格式化模式
    result = utils::StringUtil::FormatCString( "%+05d % 6.2f %-10s", 42, 3.14, "right" );
    EXPECT_EQ( result, "+0042   3.14 right     " );

    // 测试十六进制大写格式化
    EXPECT_EQ( utils::StringUtil::FormatCString( "%X %x %x", 0xabcd, 0xabcd, 0x1234 ), "ABCD abcd 1234" );
}