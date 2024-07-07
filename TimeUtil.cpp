#include "TimeUtil.h"
#include <iomanip>
#include <sstream>

namespace chrono = std::chrono;

namespace utils {

std::time_t TimeUtil::FromString( const std::string &time, const std::string &format ) {
    std::tm            tm{};
    std::istringstream ss( time );
    ss >> std::get_time( &tm, format.c_str() );
    return std::mktime( &tm );
}

std::string TimeUtil::ToString( const std::time_t &time, const std::string &format ) {
    std::stringstream ss;
    ss << std::put_time( std::localtime( &time ), format.c_str() );
    return ss.str();
}

std::string TimeUtil::ToString( const Timestamp &timestamp, const std::string &format ) {
    auto t = chrono::system_clock::to_time_t( timestamp );
    return ToString( t, format );
}

Timestamp TimeUtil::NowTimestamp() {
    return chrono::system_clock::now();
}

std::tuple<int, int, int, int, int, int, int, int, int> TimeUtil::TimestampToTuple( const Timestamp &timestamp ) {
    auto time     = chrono::system_clock::to_time_t( timestamp );
    auto tm_time  = *std::localtime( &time );
    auto duration = timestamp.time_since_epoch();
    auto ms       = chrono::duration_cast<chrono::milliseconds>( duration ).count() % 1000;
    auto us       = chrono::duration_cast<chrono::microseconds>( duration ).count() % 1000;
    auto ns       = chrono::duration_cast<chrono::nanoseconds>( duration ).count() % 1000;

    // Return as a tuple
    return std::make_tuple( tm_time.tm_year + 1900,  // year
                            tm_time.tm_mon + 1,      // month
                            tm_time.tm_mday,         // day
                            tm_time.tm_hour,         // hour
                            tm_time.tm_min,          // minute
                            tm_time.tm_sec,          // second
                            static_cast<int>( ms ),  // millisecond
                            static_cast<int>( us ),  // microsecond
                            static_cast<int>( ns ) );
}

}  // namespace utils
