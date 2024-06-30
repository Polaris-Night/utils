#include "StringUtil.h"
#include <algorithm>

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

stringlist StringUtil::Split( const std::string &str, const std::string &delim, const bool trim_empty ) {
    size_t     pos, last_pos = 0, len;
    stringlist tokens;

    while ( true ) {
        pos = str.find( delim, last_pos );
        if ( pos == std::string::npos ) {
            pos = str.size();
        }

        len = pos - last_pos;
        if ( !trim_empty || len != 0 ) {
            tokens.push_back( str.substr( last_pos, len ) );
        }

        if ( pos == str.size() ) {
            break;
        }
        else {
            last_pos = pos + delim.size();
        }
    }

    return tokens;
}

std::string StringUtil::Join( const stringlist &tokens, const std::string &delim, const bool trim_empty ) {
    if ( trim_empty ) {
        return Join( Compact( tokens ), delim, false );
    }
    else {
        std::stringstream ss;
        for ( size_t i = 0; i < tokens.size() - 1; ++i ) {
            ss << tokens[i] << delim;
        }
        ss << tokens[tokens.size() - 1];

        return ss.str();
    }
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

}  // namespace utils
