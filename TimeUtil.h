#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>

namespace utils {

struct Years {};   // 一年秒数不固定，空类型
struct Months {};  // 一月秒数不固定，空类型
using Days         = std::chrono::duration<int64_t, std::ratio<86400>>;
using Hours        = std::chrono::hours;
using Minutes      = std::chrono::minutes;
using Seconds      = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Nanoseconds  = std::chrono::nanoseconds;
using Timestamp    = std::chrono::system_clock::time_point;
// 年，月，日，时，分，秒，毫秒，微秒，纳秒
using TimestampTuple = std::tuple<int, int, int, int, int, int, int, int, int>;

class TimeUtil {
public:
    /**
     * @brief 从时间字符串解析出时间
     *
     * @param time 字符串时间
     * @param format 字符串格式
     * @return std::time_t
     */
    static std::time_t FromString( const std::string &time, const std::string &format );
    /**
     * @brief 时间转字符串
     *
     * @param time 时间
     * @param format 字符串格式
     * @return std::string
     */
    static std::string ToString( const std::time_t &time, const std::string &format = "%Y-%m-%d %H:%M:%S" );
    /**
     * @brief 时间戳转字符串
     *
     * @param timestamp 时间戳
     * @param format 字符串格式
     * @return std::string
     */
    static std::string ToString( const Timestamp &timestamp, const std::string &format = "%Y-%m-%d %H:%M:%S" );
    /**
     * @brief 当前时间
     *
     * @tparam TimeUnit 时间单位类型
     * @return TimeUnit
     */
    template <typename TimeUnit>
    static TimeUnit NowTime() {
        return std::chrono::duration_cast<TimeUnit>( std::chrono::system_clock::now().time_since_epoch() );
    }
    /**
     * @brief 当前时间戳
     *
     * @return Timestamp
     */
    static Timestamp NowTimestamp();
    /**
     * @brief 时间点转换为时间元组
     * 分别为该时间点的年，月，日，时，分，秒，毫秒，微秒，纳秒
     *
     * @param timestamp
     * @return TimestampTuple
     */
    static TimestampTuple TimestampToTuple( const Timestamp &timestamp );
    /**
     * @brief 时间元组转换为时间点
     *
     * @param tuple
     * @return std::optional<Timestamp>
     */
    static std::optional<Timestamp> TimestampFromTuple( const TimestampTuple &tuple );
    /**
     * @brief 时间按类型清零
     *
     * @tparam TimeUnit
     * @param timestamp
     * @return Timestamp
     */
    template <typename TimeUnit>
    static Timestamp Clear( const Timestamp &timestamp ) {
        auto time    = std::chrono::system_clock::to_time_t( timestamp );
        auto tm_time = *std::localtime( &time );

        if constexpr ( std::is_same_v<TimeUnit, Years> ) {
            // 设为xxxx-01-01 00:00:00
            tm_time.tm_mon  = 0;
            tm_time.tm_mday = 1;
            tm_time.tm_hour = 0;
            tm_time.tm_min  = 0;
            tm_time.tm_sec  = 0;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Months> ) {
            // 设为xxxx-xx-01 00:00:00
            tm_time.tm_mday = 1;
            tm_time.tm_hour = 0;
            tm_time.tm_min  = 0;
            tm_time.tm_sec  = 0;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Days> ) {
            // 设为xxxx-xx-xx 00:00:00
            tm_time.tm_hour = 0;
            tm_time.tm_min  = 0;
            tm_time.tm_sec  = 0;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Hours> ) {
            // 设为xxxx-xx-xx xx:00:00
            tm_time.tm_min = 0;
            tm_time.tm_sec = 0;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Minutes> ) {
            // 设为xxxx-xx-xx xx:xx:00
            tm_time.tm_sec = 0;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Seconds> || std::is_same_v<TimeUnit, Milliseconds> ||
                            std::is_same_v<TimeUnit, Microseconds> || std::is_same_v<TimeUnit, Nanoseconds> ) {
            // 秒级无需处理
            // 毫秒级、微秒级、纳秒级无法处理，std::tm未提供相应成员
        }
        else {
            // 未定义类型，不处理
        }
        return std::chrono::system_clock::from_time_t( std::mktime( &tm_time ) );
    }
    /**
     * @brief 获取时间点的指定类型值
     *
     * @tparam TimeUnit
     * @param timestamp
     * @return int
     */
    template <typename TimeUnit>
    static int GetTimeValue( const Timestamp &timestamp ) {
        auto [year, month, day, hour, minute, second, ms, us, ns] = TimestampToTuple( timestamp );
        if constexpr ( std::is_same_v<TimeUnit, Years> ) {
            return year;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Months> ) {
            return month;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Days> ) {
            return day;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Hours> ) {
            return hour;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Minutes> ) {
            return minute;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Seconds> ) {
            return second;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Milliseconds> ) {
            return ms;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Microseconds> ) {
            return us;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Nanoseconds> ) {
            return ns;
        }
        return -1;
    }
};

}  // namespace utils