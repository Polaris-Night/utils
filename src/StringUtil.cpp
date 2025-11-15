#include "StringUtil.h"
#include <regex>

namespace utils {

void StringUtil::ClearError( std::string *error_msg ) {
    if ( error_msg ) {
        error_msg->clear();
    }
}

int StringUtil::DetectBase( std::string_view str ) noexcept {
    if ( str.size() >= 2 && str[0] == '0' ) {
        if ( str[1] == 'x' || str[1] == 'X' ) {
            return 16;
        }
        else if ( ::isdigit( str[1] ) ) {
            return 8;
        }
    }
    return 10;
}

std::vector<std::string> StringUtil::Split( std::string_view str, std::string_view separator,
                                            bool skip_empty ) noexcept {
    auto                     parts = SplitRef( str, separator, skip_empty );
    std::vector<std::string> result;
    result.reserve( parts.size() );
    for ( const auto &sv : parts ) {
        result.emplace_back( sv );
    }
    return result;
}

std::vector<std::string_view> StringUtil::SplitRef( std::string_view str, std::string_view separator,
                                                    bool skip_empty ) noexcept {
    std::vector<std::string_view> result;
    result.reserve( str.size() + 2 );  // 空separator时最大可能N+2个元素

    // 分隔符为空时，返回每个字符的子串
    if ( separator.empty() ) {
        if ( !skip_empty ) {
            result.emplace_back( "" );
        }
        for ( size_t i = 0; i < str.size(); ++i ) {
            result.emplace_back( &str[i], 1 );
        }
        if ( !skip_empty ) {
            result.emplace_back( "" );
        }
        return result;
    }

    // 分隔符非空时，按常规方式分割
    const size_t sep_len = separator.length();
    size_t       pos     = 0;
    size_t       found   = 0;

    while ( ( found = str.find( separator, pos ) ) != std::string_view::npos ) {
        std::string_view part = str.substr( pos, found - pos );
        if ( !skip_empty || !part.empty() ) {
            result.emplace_back( part );
        }
        pos = found + sep_len;
    }

    std::string_view last_part = str.substr( pos );
    if ( !skip_empty || !last_part.empty() ) {
        result.emplace_back( last_part );
    }

    return result;
}

std::string StringUtil::Left( std::string_view str, size_t len ) noexcept {
    auto result = LeftRef( str, len );
    return std::string{ result };
}

std::string_view StringUtil::LeftRef( std::string_view str, size_t len ) noexcept {
    if ( len >= str.size() ) {
        return str;
    }
    return str.substr( 0, len );
}

std::string StringUtil::Mid( std::string_view str, size_t pos, size_t len ) noexcept {
    auto result = MidRef( str, pos, len );
    return std::string{ result };
}

std::string_view StringUtil::MidRef( std::string_view str, size_t pos, size_t len ) noexcept {
    if ( pos >= str.size() ) {
        return std::string_view{};
    }
    return str.substr( pos, len );
}

std::string StringUtil::Right( std::string_view str, size_t len ) noexcept {
    auto result = RightRef( str, len );
    return std::string{ result };
}

std::string_view StringUtil::RightRef( std::string_view str, size_t len ) noexcept {
    if ( len >= str.size() ) {
        return str;
    }
    return str.substr( str.size() - len, len );
}

std::string StringUtil::Trim( std::string_view str ) noexcept {
    std::string blank = " \n\r\t\v\f";
    size_t      begin = str.size(), end = 0;
    for ( size_t i = 0; i < str.size(); ++i ) {
        if ( blank.find( str[i] ) == std::string::npos ) {
            begin = i;
            break;
        }
    }

    for ( size_t i = str.size(); i > 0; --i ) {
        if ( blank.find( str[i - 1] ) == std::string::npos ) {
            end = i - 1;
            break;
        }
    }

    if ( begin >= end ) {
        return "";
    }
    else {
        return std::string( str.substr( begin, end - begin + 1 ) );
    }
}

std::string StringUtil::Repeat( std::string_view str, unsigned int times ) {
    std::stringstream ss;
    for ( unsigned int i = 0; i < times; ++i ) {
        ss << str;
    }
    return ss.str();
}

std::string StringUtil::ToUpper( const std::string &str ) {
    std::string s( str );
    std::transform( s.begin(), s.end(), s.begin(), ::toupper );
    return s;
}

std::string StringUtil::ToLower( const std::string &str ) {
    std::string s( str );
    std::transform( s.begin(), s.end(), s.begin(), ::tolower );
    return s;
}

bool StringUtil::StartWith( std::string_view str, std::string_view prefix ) {
    if ( str.size() < prefix.size() ) {
        return false;
    }
    return std::equal( prefix.begin(), prefix.end(), str.begin() );
}

bool StringUtil::EndWith( std::string_view str, std::string_view suffix ) {
    if ( str.size() < suffix.size() ) {
        return false;
    }
    return std::equal( suffix.rbegin(), suffix.rend(), str.rbegin() );
}

bool StringUtil::Contains( std::string_view str, std::string_view token ) noexcept {
    return str.find( token ) != std::string::npos ? true : false;
}

std::string StringUtil::ConvertToHexStr( std::string_view data, char separator ) {
    std::stringstream ss;
    for ( size_t i = 0; i < data.length(); ++i ) {
        // 每个字节转换为两位十六进制
        ss << std::setw( 2 ) << std::setfill( '0' ) << std::hex << (int)(unsigned char)data[i];
        // 在每两个字符之间添加间隔符
        if ( i < data.length() - 1 ) {
            ss << separator;
        }
    }
    return ss.str();
}

std::string StringUtil::IntToBitString( uint64_t value, int count ) {
    std::string bit_string;
    for ( int i = count - 1; i >= 0; --i ) {
        uint64_t mask = 1ULL << i;
        bit_string += ( value & mask ) ? '1' : '0';
    }
    return bit_string;
}

std::optional<std::string_view> StringUtil::ExtractBetween( std::string_view str, std::string_view start,
                                                            std::string_view end, bool include_start,
                                                            bool include_end ) {
    // 查找起始位置
    size_t start_pos = 0;
    if ( !start.empty() ) {
        start_pos = str.find( start );
        if ( start_pos == std::string_view::npos ) {
            return std::nullopt;
        }
    }

    // 查找结束位置
    size_t end_pos = str.size();
    if ( !end.empty() ) {
        end_pos = str.find( end, start_pos + start.size() );
        if ( end_pos == std::string_view::npos ) {
            return std::nullopt;
        }
    }

    // 计算内容起止位置，然后进行边界检查
    size_t content_start = include_start ? start_pos : start_pos + start.size();
    size_t content_end   = include_end ? ( end_pos + end.size() ) : end_pos;
    if ( content_start > content_end || content_end > str.size() ) {
        return std::nullopt;
    }

    return str.substr( content_start, content_end - content_start );
}

std::optional<std::string> StringUtil::ExtractFirst( std::string_view text, std::string_view pattern,
                                                     std::string *error_msg ) {
    ClearError( error_msg );

    try {
        std::regex  re( pattern.begin(), pattern.end() );
        std::cmatch match;  // 注意：使用 cmatch 而不是 smatch

        if ( std::regex_search( text.data(),                // const char*
                                text.data() + text.size(),  // end pointer
                                match, re ) ) {
            return std::string( match[0].first, match.length( 0 ) );  // 手动构造 string
        }
        return std::nullopt;
    }
    catch ( const std::regex_error &e ) {
        SetError( error_msg, "Regex error in ExtractFirst: ", e.what() );
        return std::nullopt;
    }
    catch ( ... ) {
        SetError( error_msg, "Unknown error in ExtractFirst." );
        return std::nullopt;
    }
}

std::optional<std::string> StringUtil::ExtractGroup( std::string_view text, std::string_view pattern,
                                                     size_t group_index, std::string *error_msg ) {
    ClearError( error_msg );

    try {
        std::regex  re( pattern.begin(), pattern.end() );
        std::cmatch match;

        if ( std::regex_search( text.data(), text.data() + text.size(), match, re ) ) {
            if ( group_index < match.size() ) {
                auto &sub = match[group_index];
                return std::string( sub.first, sub.length() );
            }
            SetError( error_msg, "Capture group index out of range: ", std::to_string( group_index ),
                      " (max available: ", std::to_string( match.size() - 1 ), ")" );
            return std::nullopt;
        }
        return std::nullopt;
    }
    catch ( const std::regex_error &e ) {
        SetError( error_msg, "Regex error in ExtractGroup: ", e.what() );
        return std::nullopt;
    }
    catch ( ... ) {
        SetError( error_msg, "Unknown error in ExtractGroup." );
        return std::nullopt;
    }
}

std::vector<std::string> StringUtil::ExtractAll( std::string_view text, std::string_view pattern,
                                                 std::string *error_msg ) {
    std::vector<std::string> results;
    ClearError( error_msg );

    try {
        std::regex re( pattern.begin(), pattern.end() );

        // 使用 std::cregex_iterator（基于指针）
        auto                 iter = std::cregex_iterator( text.data(), text.data() + text.size(), re );
        std::cregex_iterator end;

        for ( ; iter != end; ++iter ) {
            const std::csub_match &match = ( *iter )[0];
            results.emplace_back( match.first, match.length() );
        }
    }
    catch ( const std::regex_error &e ) {
        SetError( error_msg, "Regex error in ExtractAll: ", e.what() );
    }
    catch ( ... ) {
        SetError( error_msg, "Unknown error in ExtractAll." );
    }

    return results;
}

std::vector<std::string> StringUtil::ExtractAllGroups( std::string_view text, std::string_view pattern,
                                                       size_t group_index, std::string *error_msg ) {
    std::vector<std::string> results;
    ClearError( error_msg );

    try {
        std::regex           re( pattern.begin(), pattern.end() );
        auto                 iter = std::cregex_iterator( text.data(), text.data() + text.size(), re );
        std::cregex_iterator end;

        bool has_invalid_group = false;
        for ( ; iter != end; ++iter ) {
            const std::cmatch &match = *iter;
            if ( group_index < match.size() ) {
                const std::csub_match &sub = match[group_index];
                results.emplace_back( sub.first, sub.length() );
            }
            else {
                has_invalid_group = true;
            }
        }
        if ( has_invalid_group ) {
            SetError( error_msg, "Some capture groups at index ", std::to_string( group_index ), " were missing." );
        }
    }
    catch ( const std::regex_error &e ) {
        SetError( error_msg, "Regex error in ExtractAllGroups: ", e.what() );
    }
    catch ( ... ) {
        SetError( error_msg, "Unknown error in ExtractAllGroups." );
    }

    return results;
}

}  // namespace utils
