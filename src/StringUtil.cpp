#include "StringUtil.h"
#include <algorithm>
#include <random>
#include <regex>

namespace utils {

void StringUtil::ClearError( std::string *error_msg ) noexcept {
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

std::vector<std::string> StringUtil::Split( std::string_view str, std::string_view separator, bool skip_empty,
                                            bool each_char_as_separator ) noexcept {
    auto                     parts = SplitRef( str, separator, skip_empty, each_char_as_separator );
    std::vector<std::string> result;
    result.reserve( parts.size() );
    for ( const auto &sv : parts ) {
        result.emplace_back( sv );
    }
    return result;
}

std::vector<std::string_view> StringUtil::SplitRef( std::string_view str, std::string_view separator, bool skip_empty,
                                                    bool each_char_as_separator ) noexcept {
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

    // 字符分隔符模式，将separator中的每个字符都作为独立的分隔符
    if ( each_char_as_separator ) {
        size_t last_pos = 0;

        for ( size_t i = 0; i <= str.size(); ++i ) {
            if ( i == str.size() || separator.find( str[i] ) != std::string_view::npos ) {
                std::string_view part = str.substr( last_pos, i - last_pos );
                // 根据skip_empty参数决定是否添加空字符串
                if ( !skip_empty || !part.empty() ) {
                    result.emplace_back( part );
                }

                last_pos = i + 1;
            }
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

std::string StringUtil::ReplaceFirst( std::string_view str, std::string_view from, std::string_view to ) noexcept {
    if ( from.empty() ) {
        return std::string{ str };
    }

    size_t pos = str.find( from );
    if ( pos == std::string_view::npos ) {
        return std::string{ str };
    }

    std::string result;
    result.reserve( str.length() - from.length() + to.length() );
    result.append( str.substr( 0, pos ) ).append( to ).append( str.substr( pos + from.length() ) );
    return result;
}

std::string StringUtil::ReplaceAll( std::string_view str, std::string_view from, std::string_view to ) noexcept {
    if ( from.empty() || from == to ) {
        return std::string{ str };
    }

    std::string result;
    size_t      start_pos = 0;
    size_t      found_pos;

    // 预估最小容量
    size_t count = 0;
    size_t pos   = 0;
    while ( ( found_pos = str.find( from, pos ) ) != std::string_view::npos ) {
        ++count;
        pos = found_pos + from.length();
    }
    result.reserve( str.length() + count * ( to.length() - from.length() ) );

    while ( ( found_pos = str.find( from, start_pos ) ) != std::string_view::npos ) {
        result.append( str.substr( start_pos, found_pos - start_pos ) );
        result.append( to );
        start_pos = found_pos + from.length();
    }
    result.append( str.substr( start_pos ) );

    return result;
}

std::string StringUtil::EscapeC( std::string_view str ) noexcept {
    std::string result;
    result.reserve( str.length() * 2 );  // 极端情况下每个字符都转义成多个

    for ( const unsigned char c : str ) {
        switch ( c ) {
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            case '\\':
                result += "\\\\";
                break;
            case '\"':
                result += "\\\"";
                break;
            case '\'':
                result += "\\\'";
                break;
            default:
                if ( c < 32 || c > 126 ) {
                    result += "\\x";
                    const char hex[] = "0123456789ABCDEF";
                    result += hex[( c >> 4 ) & 0xF];
                    result += hex[c & 0xF];
                }
                else {
                    result += static_cast<char>( c );
                }
                break;
        }
    }

    return result;
}

std::string StringUtil::UnescapeC( std::string_view str ) noexcept {
    std::string result;
    result.reserve( str.length() );

    for ( size_t i = 0; i < str.length(); ++i ) {
        char c = str[i];
        if ( c == '\\' && i + 1 < str.length() ) {
            char next = str[++i];
            switch ( next ) {
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '"':
                    result += '"';
                    break;
                case '\'':
                    result += '\'';
                    break;
                case 'x':
                    if ( i + 2 < str.length() ) {
                        char h1 = str[++i], h2 = str[++i];
                        int  val   = 0;
                        bool valid = true;
                        if ( h1 >= '0' && h1 <= '9' )
                            val = ( h1 - '0' ) << 4;
                        else if ( h1 >= 'A' && h1 <= 'F' )
                            val = ( h1 - 'A' + 10 ) << 4;
                        else if ( h1 >= 'a' && h1 <= 'f' )
                            val = ( h1 - 'a' + 10 ) << 4;
                        else
                            valid = false;

                        if ( h2 >= '0' && h2 <= '9' )
                            val |= ( h2 - '0' );
                        else if ( h2 >= 'A' && h2 <= 'F' )
                            val |= ( h2 - 'A' + 10 );
                        else if ( h2 >= 'a' && h2 <= 'f' )
                            val |= ( h2 - 'a' + 10 );
                        else
                            valid = false;

                        if ( valid ) {
                            result += static_cast<char>( val );
                        }
                        else {
                            result += "\\x";
                            result += h1;
                            result += h2;
                        }
                    }
                    else {
                        result += "\\x";
                    }
                    break;
                default:
                    result += '\\';
                    result += next;
                    break;
            }
        }
        else {
            result += c;
        }
    }

    return result;
}

bool StringUtil::WildcardMatch( std::string_view str, std::string_view pattern ) noexcept {
    size_t s = 0, p = 0;
    size_t star_idx = std::string_view::npos, ss_idx = std::string_view::npos;

    while ( s < str.length() ) {
        // 字符匹配或遇到'?'
        if ( p < pattern.length() && ( pattern[p] == '?' || pattern[p] == str[s] ) ) {
            s++;
            p++;
        }
        // 遇到'*'，记录位置
        else if ( p < pattern.length() && pattern[p] == '*' ) {
            star_idx = p;
            ss_idx   = s;
            p++;
        }
        // 不匹配但之前有'*'，回溯
        else if ( star_idx != std::string_view::npos ) {
            p = star_idx + 1;
            ss_idx++;
            s = ss_idx;
        }
        // 不匹配且无'*'回溯
        else {
            return false;
        }
    }

    // 跳过pattern末尾的'*'
    while ( p < pattern.length() && pattern[p] == '*' ) {
        p++;
    }

    // 只有pattern完全匹配才算成功
    return p == pattern.length();
}

double StringUtil::ConvertByteUnit( double value, std::string_view from_unit, std::string_view to_unit ) noexcept {
    if ( value == 0.0 ) {
        return 0.0;
    }

    // 获取单位对应的 1024 指数（即 log2(size / B) / 10）
    auto get_exponent = []( std::string_view unit ) -> int {
        if ( EqualsIgnoreCase( unit, "B" ) ) {
            return 0;
        }
        else if ( EqualsIgnoreCase( unit, "KB" ) ) {
            return 1;
        }
        else if ( EqualsIgnoreCase( unit, "MB" ) ) {
            return 2;
        }
        else if ( EqualsIgnoreCase( unit, "GB" ) ) {
            return 3;
        }
        else if ( EqualsIgnoreCase( unit, "TB" ) ) {
            return 4;
        }
        return -1;  // 无效单位
    };

    int from_exp = get_exponent( from_unit );
    int to_exp   = get_exponent( to_unit );

    if ( from_exp == -1 || to_exp == -1 ) {
        return -1.0;  // 无效单位
    }

    int              exponent_diff = from_exp - to_exp;
    double           factor        = 1.0;
    constexpr double base          = 1024.0;

    // 快速幂（小指数，最多 ±4）
    if ( exponent_diff > 0 ) {
        for ( int i = 0; i < exponent_diff; ++i ) {
            factor *= base;
        }
    }
    else if ( exponent_diff < 0 ) {
        for ( int i = 0; i < -exponent_diff; ++i ) {
            factor /= base;
        }
    }

    double result = value * factor;

    // 防止 NaN 或 Inf
    if ( !std::isfinite( result ) ) {
        return -1.0;
    }

    return result;
}

std::string StringUtil::HumanizeBytes( uint64_t bytes, int precision, std::string_view target_unit ) noexcept {
    constexpr std::array<const char *, 5> unit_array = { "B", "KB", "MB", "GB", "TB" };
    double                                size       = static_cast<double>( bytes );

    // 钳制精度
    precision = ( precision < 0 ) ? 0 : ( precision > 6 ? 6 : precision );

    // 单位有效时使用指定单位
    if ( !target_unit.empty() && std::any_of( unit_array.begin(), unit_array.end(), [target_unit]( const char *u ) {
             return EqualsIgnoreCase( target_unit, u );
         } ) ) {
        double converted = ConvertByteUnit( size, "B", target_unit );
        if ( converted >= 0.0 ) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision( precision ) << converted << " " << target_unit;
            return oss.str();
        }
    }

    // 单位无效，自动选择最合适单位
    if ( bytes == 0 ) {
        return "0 B";
    }
    const double base       = 1024.0;
    size_t       unit_index = 0;
    while ( size >= base && unit_index < unit_array.size() - 1 ) {
        size /= base;
        unit_index += 1;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision( precision ) << size << " " << unit_array.at( unit_index );
    return oss.str();
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

std::string StringUtil::ToUpper( std::string_view str ) {
    std::string s( str );
    std::transform( s.begin(), s.end(), s.begin(), ::toupper );
    return s;
}

std::string StringUtil::ToLower( std::string_view str ) {
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

bool StringUtil::EqualsIgnoreCase( std::string_view str1, std::string_view str2 ) noexcept {
    if ( str1.size() != str2.size() ) {
        return false;
    }
    for ( size_t i = 0; i < str1.size(); ++i ) {
        if ( std::tolower( str1[i] ) != std::tolower( str2[i] ) ) {
            return false;
        }
    }
    return true;
}

std::string StringUtil::IntToBitString( uint64_t value, int count ) noexcept {
    std::string bit_string;
    for ( int i = count - 1; i >= 0; --i ) {
        uint64_t mask = 1ULL << i;
        bit_string += ( value & mask ) ? '1' : '0';
    }
    return bit_string;
}

std::optional<std::string_view> StringUtil::ExtractBetween( std::string_view str, std::string_view start,
                                                            std::string_view end, bool include_start,
                                                            bool include_end ) noexcept {
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

std::string StringUtil::PadLeft( std::string_view str, size_t width, char fill_char ) noexcept {
    if ( str.length() >= width ) {
        return std::string( str );
    }

    std::string result( width - str.length(), fill_char );
    result.append( str.data(), str.length() );
    return result;
}

std::string StringUtil::PadRight( std::string_view str, size_t width, char fill_char ) noexcept {
    if ( str.length() >= width ) {
        return std::string( str );
    }

    std::string result( str );
    result.append( width - str.length(), fill_char );
    return result;
}

std::string StringUtil::RandomString( size_t length, std::string_view charset ) noexcept {
    if ( charset.empty() || length == 0 ) {
        return {};
    }

    thread_local static std::random_device rd;
    thread_local static std::seed_seq      seed{ rd(), rd(), rd(), rd() };
    thread_local static std::mt19937       gen( seed );
    std::uniform_int_distribution<size_t>  dis( 0, charset.length() - 1 );

    std::string result;
    result.reserve( length );

    for ( size_t i = 0; i < length; ++i ) {
        result += charset[dis( gen )];
    }

    return result;
}

bool StringUtil::IsNumeric( std::string_view str ) noexcept {
    if ( str.empty() ) {
        return false;
    }
    size_t start = 0;
    if ( str[0] == '+' || str[0] == '-' || str[0] == '.' ) {
        if ( str.size() == 1 ) {
            return false;  // 只有一个符号的不是数字
        }
        start = 1;
    }
    bool has_decimal_point = false;
    for ( size_t i = start; i < str.size(); ++i ) {
        if ( str[i] == '.' ) {
            if ( has_decimal_point ) {
                return false;  // 多个小数点为非法数字
            }
            has_decimal_point = true;
        }
        else if ( !std::isdigit( str[i] ) ) {
            return false;  // 存在非数字字符
        }
    }
    return true;
}

bool StringUtil::IsUpper( std::string_view str ) noexcept {
    if ( str.empty() ) {
        return false;
    }
    for ( auto c : str ) {
        if ( !std::isupper( c ) ) {
            return false;
        }
    }
    return true;
}

bool StringUtil::IsLower( std::string_view str ) noexcept {
    if ( str.empty() ) {
        return false;
    }
    for ( auto c : str ) {
        if ( !std::islower( c ) ) {
            return false;
        }
    }
    return true;
}

}  // namespace utils
