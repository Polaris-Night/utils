#pragma once

#include <chrono>
#include <ctime>
#include <optional>
#include <string>
#include <tuple>

namespace utils {

using Seconds      = std::chrono::seconds;
using Hours        = std::chrono::hours;
using Milliseconds = std::chrono::milliseconds;
using Minutes      = std::chrono::minutes;
using Microseconds = std::chrono::microseconds;
using Nanoseconds  = std::chrono::nanoseconds;
using Timestamp    = std::chrono::system_clock::time_point;

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
     * @return std::tuple<int, int, int, int, int, int, int, int, int>
     */
    static std::tuple<int, int, int, int, int, int, int, int, int> TimestampToTuple( const Timestamp &timestamp );
};

}  // namespace utils