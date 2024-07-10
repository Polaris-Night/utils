#include "TimeUtil.h"
#include <iomanip>
#include <sstream>

static bool IsInRange( int value, int min, int max ) {
    return value >= min && value <= max;
}

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
    auto t = std::chrono::system_clock::to_time_t( timestamp );
    return ToString( t, format );
}

Timestamp TimeUtil::NowTimestamp() {
    return std::chrono::system_clock::now();
}

TimestampTuple TimeUtil::TimestampToTuple( const Timestamp &timestamp ) {
    auto time     = std::chrono::system_clock::to_time_t( timestamp );
    auto tm_time  = *std::localtime( &time );
    auto duration = timestamp.time_since_epoch();
    auto ms       = std::chrono::duration_cast<Milliseconds>( duration ).count() % 1000;
    auto us       = std::chrono::duration_cast<Microseconds>( duration ).count() % 1000;
    auto ns       = std::chrono::duration_cast<Nanoseconds>( duration ).count() % 1000;

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

std::optional<Timestamp> TimeUtil::TimestampFromTuple( const TimestampTuple &tuple ) {
    auto [year, month, day, hour, minute, second, ms, us, ns] = tuple;
    if ( !IsInRange( year, 1970, 9999 ) || !IsInRange( month, 1, 12 ) || !IsInRange( day, 1, 31 ) ||
         !IsInRange( hour, 0, 23 ) || !IsInRange( minute, 0, 59 ) || !IsInRange( second, 0, 59 ) ||
         !IsInRange( ms, 0, 999 ) || !IsInRange( us, 0, 999 ) || !IsInRange( ns, 0, 999 ) ) {
        return std::nullopt;
    }
    std::tm tm_time{};
    tm_time.tm_yday = year - 1900;
    tm_time.tm_mon  = month - 1;
    tm_time.tm_mday = day;
    tm_time.tm_hour = hour;
    tm_time.tm_min  = minute;
    tm_time.tm_sec  = second;
    auto time       = std::mktime( &tm_time );
    auto timestamp  = std::chrono::system_clock::from_time_t( time );
    timestamp += Milliseconds( ms );
    timestamp += Microseconds( us );
    timestamp += Nanoseconds( ns );
    return timestamp;
}

}  // namespace utils
