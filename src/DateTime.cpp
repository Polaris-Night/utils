#include "DateTime.h"
#include <atomic>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace utils {

namespace {

using Duration = Timestamp::duration;

// ==================== 平台时间函数封装 ====================

bool LocalTime( const std::time_t &time, std::tm &out ) noexcept {
#if defined( PLATFORM_OS_WINDOWS )
    return localtime_s( &out, &time ) == 0;
#else
    return localtime_r( &time, &out ) != nullptr;
#endif
}

bool GmTime( const std::time_t &time, std::tm &out ) noexcept {
#if defined( PLATFORM_OS_WINDOWS )
    return gmtime_s( &out, &time ) == 0;
#else
    return gmtime_r( &time, &out ) != nullptr;
#endif
}

std::time_t TimeGm( std::tm *tm ) noexcept {
#if defined( PLATFORM_OS_WINDOWS )
    return _mkgmtime( tm );
#else
    return timegm( tm );
#endif
}

// ==================== 通用辅助 ====================

// 亚秒部分：timestamp 减去按秒向下取整后的余数，恒在 [0, 1s)
Duration SubSeconds( const Timestamp &timestamp ) {
    return timestamp - std::chrono::floor<Seconds>( timestamp );
}

/**
 * @brief 由字段 + 亚秒构造时间戳
 *
 * @param trust_isdst true  = 信任调用者设置的 tm_isdst（0/1 有效，其他交由系统判定）
 *                    false = 内部历法运算，一律交由系统判定（Local）或忽略（UTC/Offset）
 */
std::optional<Timestamp> MakeTimestamp( std::tm tm, const Duration &sub, const TimeZone &zone, bool trust_isdst ) {
    if ( zone.IsLocal() ) {
        if ( !trust_isdst || ( tm.tm_isdst != 0 && tm.tm_isdst != 1 ) ) {
            tm.tm_isdst = -1;
        }
        const std::time_t t = std::mktime( &tm );
        if ( t == static_cast<std::time_t>( -1 ) ) {
            return std::nullopt;
        }
        return std::chrono::system_clock::from_time_t( t ) + sub;
    }

    tm.tm_isdst         = 0;
    const std::time_t t = TimeGm( &tm );
    if ( t == static_cast<std::time_t>( -1 ) ) {
        return std::nullopt;
    }
    auto timestamp = std::chrono::system_clock::from_time_t( t );
    if ( zone.IsOffset() ) {
        timestamp -= std::chrono::minutes( zone.OffsetMinutes() );
    }
    return timestamp + sub;
}

// ==================== 固定偏移格式化辅助 ====================

std::string FormatZToken( int offset_minutes ) {
    const char sign = offset_minutes < 0 ? '-' : '+';
    const int  abs  = offset_minutes < 0 ? -offset_minutes : offset_minutes;
    char       buffer[8];
    std::snprintf( buffer, sizeof( buffer ), "%c%02d%02d", sign, abs / 60, abs % 60 );
    return buffer;
}

std::string FormatZoneName( int offset_minutes ) {
    const char sign = offset_minutes < 0 ? '-' : '+';
    const int  abs  = offset_minutes < 0 ? -offset_minutes : offset_minutes;
    char       buffer[16];
    std::snprintf( buffer, sizeof( buffer ), "UTC%c%02d:%02d", sign, abs / 60, abs % 60 );
    return buffer;
}

// 替换格式串中的 %z / %Z（固定偏移时区专用；%% 保持原样交给 strftime）
std::string ReplaceOffsetTokens( const std::string &format, int offset_minutes ) {
    const std::string z_token = FormatZToken( offset_minutes );
    const std::string Z_token = FormatZoneName( offset_minutes );

    std::string result;
    result.reserve( format.size() + 8 );
    for ( std::size_t i = 0; i < format.size(); ++i ) {
        if ( format[i] != '%' || i + 1 >= format.size() ) {
            result += format[i];
            continue;
        }
        const char next = format[i + 1];
        if ( next == '%' ) {
            result += "%%";
        }
        else if ( next == 'z' ) {
            result += z_token;
        }
        else if ( next == 'Z' ) {
            result += Z_token;
        }
        else {
            result += '%';
            result += next;
        }
        ++i;  // 跳过已处理的第二个字符
    }
    return result;
}

// ==================== 时间种子状态 ====================

struct TimeSeedState {
    std::atomic<bool>    has_seed{ false };
    std::atomic<int64_t> server_ns{ 0 };  // 种子时间（自 epoch 起的纳秒）
    std::atomic<int64_t> steady_ns{ 0 };  // 设置种子时的单调时钟纳秒
};

TimeSeedState &SeedState() {
    static TimeSeedState state;
    return state;
}

int64_t EpochNanoseconds( const Timestamp &timestamp ) {
    return std::chrono::duration_cast<Nanoseconds>( timestamp.time_since_epoch() ).count();
}

int64_t SteadyNanoseconds() {
    return std::chrono::duration_cast<Nanoseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count();
}

}  // namespace

// ==================== TimeZone ====================

int TimeZone::OffsetMinutesAt( const Timestamp &timestamp ) const {
    if ( IsUtc() ) {
        return 0;
    }
    if ( IsOffset() ) {
        return offset_minutes_;
    }

    // Local：由本地字段反推实际偏移（含夏令时）
    std::tm           tm{};
    const std::time_t t = std::chrono::system_clock::to_time_t( std::chrono::floor<Seconds>( timestamp ) );
    if ( !LocalTime( t, tm ) ) {
        return 0;
    }
    tm.tm_isdst                = 0;
    const std::time_t as_utc   = TimeGm( &tm );
    if ( as_utc == static_cast<std::time_t>( -1 ) ) {
        return 0;
    }
    return static_cast<int>( ( as_utc - t ) / 60 );
}

// ==================== 内部转换 ====================

bool DateTime::ToTmImpl( const Timestamp &timestamp, const TimeZone &zone, std::tm &out ) noexcept {
    const std::time_t t = std::chrono::system_clock::to_time_t( std::chrono::floor<Seconds>( timestamp ) );
    if ( zone.IsLocal() ) {
        return LocalTime( t, out );
    }
    if ( zone.IsUtc() ) {
        return GmTime( t, out );
    }
    // 固定偏移：平移后用 UTC 字段表示（%z/%Z 由 Format 后处理）
    return GmTime( static_cast<std::time_t>( t + zone.OffsetMinutes() * 60 ), out );
}

std::optional<Timestamp> DateTime::FromTmImpl( const std::tm &tm, const TimeZone &zone ) noexcept {
    return MakeTimestamp( tm, Duration::zero(), zone, true );
}

// ==================== 工厂 ====================

DateTime DateTime::Now( const TimeZone &zone ) {
    return DateTime( NowTimestamp(), zone );
}

Timestamp DateTime::NowTimestamp() {
    return std::chrono::system_clock::now();
}

DateTime DateTime::NowSynced( const TimeZone &zone ) {
    return DateTime( NowSyncedTimestamp(), zone );
}

Timestamp DateTime::NowSyncedTimestamp() {
    const TimeSeedState &state = SeedState();
    if ( !state.has_seed.load( std::memory_order_acquire ) ) {
        return NowTimestamp();
    }
    const int64_t server_ns  = state.server_ns.load( std::memory_order_relaxed );
    const int64_t anchor_ns  = state.steady_ns.load( std::memory_order_relaxed );
    const int64_t elapsed_ns = SteadyNanoseconds() - anchor_ns;  // 单调时钟，恒 >= 0

    return Timestamp( std::chrono::duration_cast<Timestamp::duration>( Nanoseconds( server_ns + elapsed_ns ) ) );
}

DateTime DateTime::FromUnixSeconds( int64_t seconds, const TimeZone &zone ) {
    return DateTime( Timestamp( std::chrono::duration_cast<Timestamp::duration>( Seconds( seconds ) ) ), zone );
}

DateTime DateTime::FromUnixMillis( int64_t millis, const TimeZone &zone ) {
    return DateTime( Timestamp( std::chrono::duration_cast<Timestamp::duration>( Milliseconds( millis ) ) ), zone );
}

DateTime DateTime::FromUnixMicros( int64_t micros, const TimeZone &zone ) {
    return DateTime( Timestamp( std::chrono::duration_cast<Timestamp::duration>( Microseconds( micros ) ) ), zone );
}

DateTime DateTime::FromUnixNanos( int64_t nanos, const TimeZone &zone ) {
    return DateTime( Timestamp( std::chrono::duration_cast<Timestamp::duration>( Nanoseconds( nanos ) ) ), zone );
}

DateTime DateTime::FromTimeT( std::time_t time, const TimeZone &zone ) {
    return DateTime( std::chrono::system_clock::from_time_t( time ), zone );
}

std::optional<DateTime> DateTime::FromTm( const std::tm &tm, const TimeZone &zone ) {
    const auto timestamp = FromTmImpl( tm, zone );
    if ( !timestamp.has_value() ) {
        return std::nullopt;
    }
    return DateTime( *timestamp, zone );
}

std::optional<DateTime> DateTime::FromParts( const DateTimeParts &parts, const TimeZone &zone ) {
    if ( parts.year < 1900 || parts.year > 9999 ) {
        return std::nullopt;
    }
    if ( parts.month < 1 || parts.month > 12 ) {
        return std::nullopt;
    }
    if ( parts.day < 1 || parts.day > DaysInMonth( parts.year, parts.month ) ) {
        return std::nullopt;
    }
    if ( parts.hour < 0 || parts.hour > 23 ) {
        return std::nullopt;
    }
    if ( parts.minute < 0 || parts.minute > 59 ) {
        return std::nullopt;
    }
    if ( parts.second < 0 || parts.second > 59 ) {
        return std::nullopt;
    }
    if ( parts.millisecond < 0 || parts.millisecond > 999 ) {
        return std::nullopt;
    }
    if ( parts.microsecond < 0 || parts.microsecond > 999 ) {
        return std::nullopt;
    }
    if ( parts.nanosecond < 0 || parts.nanosecond > 999 ) {
        return std::nullopt;
    }

    std::tm tm{};
    tm.tm_year = parts.year - 1900;
    tm.tm_mon  = parts.month - 1;
    tm.tm_mday = parts.day;
    tm.tm_hour = parts.hour;
    tm.tm_min  = parts.minute;
    tm.tm_sec  = parts.second;

    const auto sub = std::chrono::duration_cast<Timestamp::duration>(
        Milliseconds( parts.millisecond ) + Microseconds( parts.microsecond ) + Nanoseconds( parts.nanosecond ) );
    const auto timestamp = MakeTimestamp( tm, sub, zone, false );
    if ( !timestamp.has_value() ) {
        return std::nullopt;
    }
    return DateTime( *timestamp, zone );
}

std::optional<DateTime> DateTime::FromString( std::string_view text, std::string_view format, const TimeZone &zone ) {
    if ( text.empty() || format.empty() ) {
        return std::nullopt;
    }

    std::tm tm{};
    tm.tm_year = 70;  // 未出现的字段取默认值：1970-01-01 00:00:00
    tm.tm_mon  = 0;
    tm.tm_mday = 1;

    std::istringstream ss{ std::string( text ) };
    ss >> std::get_time( &tm, std::string( format ).c_str() );
    if ( ss.fail() ) {
        return std::nullopt;
    }

    char extra = '\0';
    if ( ss >> extra ) {
        return std::nullopt;  // 严格模式：仅允许尾随空白
    }

    DateTimeParts parts;
    parts.year   = tm.tm_year + 1900;
    parts.month  = tm.tm_mon + 1;
    parts.day    = tm.tm_mday;
    parts.hour   = tm.tm_hour;
    parts.minute = tm.tm_min;
    parts.second = tm.tm_sec;
    return FromParts( parts, zone );
}

std::optional<DateTime> DateTime::FromIso8601( std::string_view text, const TimeZone &default_zone ) {
    // 允许首尾空白
    std::size_t begin = 0;
    std::size_t end   = text.size();
    while ( begin < end && std::isspace( static_cast<unsigned char>( text[begin] ) ) ) {
        ++begin;
    }
    while ( end > begin && std::isspace( static_cast<unsigned char>( text[end - 1] ) ) ) {
        --end;
    }
    text = text.substr( begin, end - begin );
    if ( text.size() < 10 ) {  // 至少 YYYY-MM-DD
        return std::nullopt;
    }

    std::size_t pos = 0;
    auto        read_digits = [&text, &pos]( std::size_t count, int &value ) -> bool {
        if ( pos + count > text.size() ) {
            return false;
        }
        value = 0;
        for ( std::size_t i = 0; i < count; ++i ) {
            const char c = text[pos + i];
            if ( c < '0' || c > '9' ) {
                return false;
            }
            value = value * 10 + ( c - '0' );
        }
        pos += count;
        return true;
    };

    DateTimeParts parts;
    if ( !read_digits( 4, parts.year ) || pos >= text.size() || text[pos] != '-' ) {
        return std::nullopt;
    }
    ++pos;
    if ( !read_digits( 2, parts.month ) || pos >= text.size() || text[pos] != '-' ) {
        return std::nullopt;
    }
    ++pos;
    if ( !read_digits( 2, parts.day ) ) {
        return std::nullopt;
    }

    // 可选时间部分
    if ( pos < text.size() && ( text[pos] == 'T' || text[pos] == 't' || text[pos] == ' ' ) ) {
        ++pos;
        if ( !read_digits( 2, parts.hour ) || pos >= text.size() || text[pos] != ':' ) {
            return std::nullopt;
        }
        ++pos;
        if ( !read_digits( 2, parts.minute ) ) {
            return std::nullopt;
        }
        if ( pos < text.size() && text[pos] == ':' ) {  // 秒可省略
            ++pos;
            if ( !read_digits( 2, parts.second ) ) {
                return std::nullopt;
            }
        }
        // 可选小数秒（1-9 位，不足 9 位右侧补零）
        if ( pos < text.size() && text[pos] == '.' ) {
            ++pos;
            std::size_t digits   = 0;
            int64_t     fraction = 0;
            while ( pos < text.size() && text[pos] >= '0' && text[pos] <= '9' ) {
                if ( digits >= 9 ) {
                    return std::nullopt;  // 超过纳秒精度
                }
                fraction = fraction * 10 + ( text[pos] - '0' );
                ++digits;
                ++pos;
            }
            if ( digits == 0 ) {
                return std::nullopt;  // '.' 后必须有数字
            }
            for ( std::size_t i = digits; i < 9; ++i ) {
                fraction *= 10;
            }
            parts.millisecond = static_cast<int>( fraction / 1000000 );
            parts.microsecond = static_cast<int>( ( fraction / 1000 ) % 1000 );
            parts.nanosecond  = static_cast<int>( fraction % 1000 );
        }
    }

    // 可选时区后缀
    TimeZone zone = default_zone;
    if ( pos < text.size() ) {
        const char c = text[pos];
        if ( c == 'Z' || c == 'z' ) {
            ++pos;
            zone = TimeZone::Utc();
        }
        else if ( c == '+' || c == '-' ) {
            const int sign = ( c == '-' ) ? -1 : 1;
            ++pos;
            int hours   = 0;
            int minutes = 0;
            if ( !read_digits( 2, hours ) ) {
                return std::nullopt;
            }
            if ( pos < text.size() && text[pos] == ':' ) {
                ++pos;
                if ( !read_digits( 2, minutes ) ) {
                    return std::nullopt;
                }
            }
            else if ( pos + 2 <= text.size() && text[pos] >= '0' && text[pos] <= '9' ) {
                if ( !read_digits( 2, minutes ) ) {  // ±HHMM 紧凑形式
                    return std::nullopt;
                }
            }
            if ( hours > 23 || minutes > 59 ) {
                return std::nullopt;
            }
            zone = TimeZone::OffsetMinutes( sign * ( hours * 60 + minutes ) );
        }
    }

    if ( pos != text.size() ) {
        return std::nullopt;  // 严格模式：不得有残余内容
    }
    return FromParts( parts, zone );
}

// ==================== 导出 ====================

std::time_t DateTime::ToTimeT() const {
    return static_cast<std::time_t>( ToUnixSeconds() );
}

int64_t DateTime::ToUnixSeconds() const {
    return std::chrono::floor<Seconds>( timestamp_ ).time_since_epoch().count();
}

int64_t DateTime::ToUnixMillis() const {
    return std::chrono::floor<Milliseconds>( timestamp_ ).time_since_epoch().count();
}

int64_t DateTime::ToUnixMicros() const {
    return std::chrono::floor<Microseconds>( timestamp_ ).time_since_epoch().count();
}

int64_t DateTime::ToUnixNanos() const {
    return std::chrono::floor<Nanoseconds>( timestamp_ ).time_since_epoch().count();
}

std::optional<std::tm> DateTime::ToTm() const {
    std::tm tm{};
    if ( !ToTmImpl( timestamp_, zone_, tm ) ) {
        return std::nullopt;
    }
    return tm;
}

DateTimeParts DateTime::ToParts() const {
    DateTimeParts parts;
    std::tm       tm{};
    if ( !ToTmImpl( timestamp_, zone_, tm ) ) {
        return parts;  // 转换失败：返回默认组件
    }

    parts.year       = tm.tm_year + 1900;
    parts.month      = tm.tm_mon + 1;
    parts.day        = tm.tm_mday;
    parts.hour       = tm.tm_hour;
    parts.minute     = tm.tm_min;
    parts.second     = tm.tm_sec;
    parts.weekday    = static_cast<Weekday>( tm.tm_wday );
    parts.day_of_year = tm.tm_yday + 1;

    const auto sub_ns  = std::chrono::duration_cast<Nanoseconds>( SubSeconds( timestamp_ ) ).count();
    parts.millisecond  = static_cast<int>( sub_ns / 1000000 );
    parts.microsecond  = static_cast<int>( ( sub_ns / 1000 ) % 1000 );
    parts.nanosecond   = static_cast<int>( sub_ns % 1000 );
    return parts;
}

std::string DateTime::ToString( std::string_view format ) const {
    std::tm tm{};
    if ( !ToTmImpl( timestamp_, zone_, tm ) ) {
        return {};
    }

    std::string fmt( format );
    if ( zone_.IsOffset() ) {
        fmt = ReplaceOffsetTokens( fmt, zone_.OffsetMinutes() );
    }

    std::stringstream ss;
    ss << std::put_time( &tm, fmt.c_str() );
    return ss.str();
}

std::string DateTime::ToIso8601() const {
    const DateTimeParts parts = ToParts();

    // 亚秒按有效精度输出：毫秒 3 位 / 微秒 6 位 / 纳秒 9 位，全零省略
    int fraction_digits = 0;
    int fraction        = 0;
    if ( parts.nanosecond != 0 ) {
        fraction_digits = 9;
        fraction        = parts.millisecond * 1000000 + parts.microsecond * 1000 + parts.nanosecond;
    }
    else if ( parts.microsecond != 0 ) {
        fraction_digits = 6;
        fraction        = parts.millisecond * 1000 + parts.microsecond;
    }
    else if ( parts.millisecond != 0 ) {
        fraction_digits = 3;
        fraction        = parts.millisecond;
    }

    char buffer[48];
    if ( fraction_digits == 0 ) {
        std::snprintf( buffer, sizeof( buffer ), "%04d-%02d-%02dT%02d:%02d:%02d", parts.year, parts.month, parts.day,
                       parts.hour, parts.minute, parts.second );
    }
    else {
        std::snprintf( buffer, sizeof( buffer ), "%04d-%02d-%02dT%02d:%02d:%02d.%0*d", parts.year, parts.month,
                       parts.day, parts.hour, parts.minute, parts.second, fraction_digits, fraction );
    }

    if ( zone_.IsUtc() ) {
        return std::string( buffer ) + "Z";
    }

    const int offset_minutes = zone_.OffsetMinutesAt( timestamp_ );
    // 最坏情况：int 范围下两个 %02d 各可达 11 字符（符号 + 10 位数字）
    char      suffix[32]{};
    const char sign = offset_minutes < 0 ? '-' : '+';
    const int  abs  = offset_minutes < 0 ? -offset_minutes : offset_minutes;
    std::snprintf( suffix, sizeof( suffix ), "%c%02d:%02d", sign, abs / 60, abs % 60 );
    return std::string( buffer ) + suffix;
}

// ==================== 日历辅助 ====================

int DateTime::DaysInMonth( int year, int month ) noexcept {
    if ( month < 1 || month > 12 ) {
        return 0;
    }
    static constexpr int kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if ( month == 2 && IsLeapYear( year ) ) {
        return 29;
    }
    return kDays[month - 1];
}

// ==================== 内部历法运算 ====================

std::optional<Timestamp> DateTime::ClearFields( const Timestamp &timestamp, const TimeZone &zone,
                                                FieldLevel level ) noexcept {
    std::tm tm{};
    if ( !ToTmImpl( timestamp, zone, tm ) ) {
        return std::nullopt;
    }

    switch ( level ) {
        case FieldLevel::Year:
            tm.tm_mon = 0;
            [[fallthrough]];
        case FieldLevel::Month:
            tm.tm_mday = 1;
            [[fallthrough]];
        case FieldLevel::Day:
            tm.tm_hour = 0;
            [[fallthrough]];
        case FieldLevel::Hour:
            tm.tm_min = 0;
            [[fallthrough]];
        case FieldLevel::Minute:
            tm.tm_sec = 0;
            break;
    }
    return MakeTimestamp( tm, Duration::zero(), zone, false );
}

std::optional<Timestamp> DateTime::AddYearsImpl( const Timestamp &timestamp, int64_t years, const TimeZone &zone,
                                                 bool &clamped ) noexcept {
    clamped = false;
    if ( years < -1000000 || years > 1000000 ) {
        return std::nullopt;
    }

    std::tm tm{};
    if ( !ToTmImpl( timestamp, zone, tm ) ) {
        return std::nullopt;
    }
    const auto sub = SubSeconds( timestamp );

    tm.tm_year += static_cast<int>( years );
    const int days_in_target = DaysInMonth( tm.tm_year + 1900, tm.tm_mon + 1 );
    if ( tm.tm_mday > days_in_target ) {
        tm.tm_mday = days_in_target;  // 2-29 加一年钳制到 2-28
        clamped    = true;
    }
    return MakeTimestamp( tm, sub, zone, false );
}

std::optional<Timestamp> DateTime::AddMonthsImpl( const Timestamp &timestamp, int64_t months, const TimeZone &zone,
                                                  bool &clamped ) noexcept {
    clamped = false;
    if ( months < -12000000 || months > 12000000 ) {
        return std::nullopt;
    }

    std::tm tm{};
    if ( !ToTmImpl( timestamp, zone, tm ) ) {
        return std::nullopt;
    }
    const auto sub = SubSeconds( timestamp );

    const int64_t total_months = static_cast<int64_t>( tm.tm_year ) * 12 + tm.tm_mon + months;
    int64_t       year         = total_months / 12;
    int64_t       month        = total_months % 12;
    if ( month < 0 ) {
        month += 12;
        --year;
    }

    const int days_in_target = DaysInMonth( static_cast<int>( year ) + 1900, static_cast<int>( month ) + 1 );
    if ( tm.tm_mday > days_in_target ) {
        tm.tm_mday = days_in_target;  // 1-31 加一月钳制到 2-29/2-28
        clamped    = true;
    }
    tm.tm_year = static_cast<int>( year );
    tm.tm_mon  = static_cast<int>( month );
    return MakeTimestamp( tm, sub, zone, false );
}

std::optional<Timestamp> DateTime::AddDaysImpl( const Timestamp &timestamp, int64_t days, const TimeZone &zone ) noexcept {
    if ( days < -100000000 || days > 100000000 ) {
        return std::nullopt;
    }

    std::tm tm{};
    if ( !ToTmImpl( timestamp, zone, tm ) ) {
        return std::nullopt;
    }
    const auto sub = SubSeconds( timestamp );

    tm.tm_mday += static_cast<int>( days );  // mktime/timegm 归一化，跨夏令时保持墙上时间
    return MakeTimestamp( tm, sub, zone, false );
}

// ==================== 时间种子 ====================

void DateTime::SetTimeSeed( const Timestamp &server_time ) {
    TimeSeedState &state = SeedState();
    state.server_ns.store( EpochNanoseconds( server_time ), std::memory_order_relaxed );
    state.steady_ns.store( SteadyNanoseconds(), std::memory_order_relaxed );
    state.has_seed.store( true, std::memory_order_release );
}

void DateTime::ResetTimeSeed() {
    SeedState().has_seed.store( false, std::memory_order_release );
}

bool DateTime::HasTimeSeed() {
    return SeedState().has_seed.load( std::memory_order_acquire );
}

// ==================== 其他 ====================

int DateTime::AgeInYears( const DateTime &birthday, bool add_age_after_birthday ) {
    const DateTime now = NowSynced( birthday.Zone() );
    if ( now.timestamp_ <= birthday.timestamp_ ) {
        return 0;
    }

    std::tm birth{};
    std::tm current{};
    if ( !ToTmImpl( birthday.timestamp_, birthday.zone_, birth ) ||
         !ToTmImpl( now.timestamp_, birthday.zone_, current ) ) {
        return 0;
    }

    const int year_diff = current.tm_year - birth.tm_year;
    if ( year_diff <= 0 ) {
        return 0;
    }
    if ( current.tm_mon < birth.tm_mon ) {
        return year_diff - 1;
    }
    if ( current.tm_mon > birth.tm_mon ) {
        return year_diff;
    }

    const int day_diff = current.tm_mday - birth.tm_mday;
    if ( ( !add_age_after_birthday && day_diff < 0 ) || ( add_age_after_birthday && day_diff <= 0 ) ) {
        return year_diff - 1;
    }
    return year_diff;
}

}  // namespace utils
