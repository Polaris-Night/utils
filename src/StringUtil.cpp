#include "StringUtil.h"
#include <algorithm>
#include <cstring>
#include <iomanip>

namespace utils {
stringlist StringUtil::Compact( const stringlist &tokens ) {
    stringlist compacted;

    for ( size_t i = 0; i < tokens.size(); ++i ) {
        if ( !tokens[i].empty() ) {
            compacted.push_back( tokens[i] );
        }
    }

    return compacted;
}

stringlist StringUtil::Split( const std::string &str, const std::string &separator ) {
    size_t     pos, last_pos = 0, len;
    stringlist tokens;

    while ( true ) {
        pos = str.find( separator, last_pos );
        if ( pos == std::string::npos ) {
            pos = str.size();
        }

        len = pos - last_pos;
        if ( len != 0 ) {
            tokens.push_back( str.substr( last_pos, len ) );
        }

        if ( pos == str.size() ) {
            break;
        }
        else {
            last_pos = pos + separator.size();
        }
    }

    return tokens;
}

std::string StringUtil::Trim( const std::string &str ) {
    std::string blank = "\r\n\t ";
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
        return str.substr( begin, end - begin + 1 );
    }
}

std::string StringUtil::Repeat( const std::string &str, unsigned int times ) {
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

bool StringUtil::StartWith( const std::string &str, const std::string &prefix ) {
    if ( str.size() < prefix.size() )
        return false;
    return std::equal( prefix.begin(), prefix.end(), str.begin() );
}

bool StringUtil::EndWith( const std::string &str, const std::string &suffix ) {
    if ( str.size() < suffix.size() ) {
        return false;
    }
    return std::equal( suffix.rbegin(), suffix.rend(), str.rbegin() );
}

bool StringUtil::Contains( const std::string &str, const std::string &token ) {
    return str.find( token ) != std::string::npos ? true : false;
}

std::string StringUtil::ConvertToHexStr( const char *data, char separator ) {
    if ( data == nullptr ) {
        return "";
    }
    std::stringstream ss;
    size_t            len = std::strlen( data );
    for ( size_t i = 0; i < len; ++i ) {
        // 每个字节转换为两位十六进制
        ss << std::setw( 2 ) << std::setfill( '0' ) << std::hex << (int)(unsigned char)data[i];
        // 在每两个字符之间添加间隔符
        if ( i < len - 1 ) {
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

}  // namespace utils
