#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

namespace utils {

// ==================== 基础类型 ====================

/**
 * @brief 年（日历单位，时长不固定）
 */
struct Years {};
/**
 * @brief 月（日历单位，时长不固定）
 */
struct Months {};
using Days         = std::chrono::duration<int64_t, std::ratio<86400>>;
using Hours        = std::chrono::hours;
using Minutes      = std::chrono::minutes;
using Seconds      = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Nanoseconds  = std::chrono::nanoseconds;

/**
 * @brief 规范时刻：system_clock 时间点（纪元 = Unix 纪元，UTC 语义）
 *
 * 这是所有时间表示之间的转换枢纽：任意表示 A -> 任意表示 B，等价于 A -> Timestamp -> B。
 */
using Timestamp = std::chrono::system_clock::time_point;

/**
 * @brief 判断类型是否为 DateTime 支持的时间单位
 *
 * @tparam TimeUnit 待判断类型
 */
template <typename TimeUnit>
struct IsTimeUnit
    : std::disjunction<std::is_same<TimeUnit, Years>, std::is_same<TimeUnit, Months>, std::is_same<TimeUnit, Days>,
                       std::is_same<TimeUnit, Hours>, std::is_same<TimeUnit, Minutes>, std::is_same<TimeUnit, Seconds>,
                       std::is_same<TimeUnit, Milliseconds>, std::is_same<TimeUnit, Microseconds>,
                       std::is_same<TimeUnit, Nanoseconds>> {};

template <typename TimeUnit>
constexpr bool IsTimeUnit_v = IsTimeUnit<TimeUnit>::value;

/**
 * @brief 星期（0 = 周日，对齐 std::tm::tm_wday，可直接与整数互转）
 */
enum Weekday : int
{
    Sunday    = 0,
    Monday    = 1,
    Tuesday   = 2,
    Wednesday = 3,
    Thursday  = 4,
    Friday    = 5,
    Saturday  = 6,
};

// ==================== 时区 ====================

/**
 * @brief 轻量时区描述：本地时区 / UTC / 固定偏移
 *
 * - Local：运行时按系统时区解析（夏令时正确，依赖系统 TZ）；
 * - Utc：零偏移；
 * - Offset：固定分钟偏移（UTC+N），不含夏令时语义。
 *
 * 任意时刻的"时刻值"（Timestamp）与时区无关；时区只影响历法字段的解析与展示。
 */
class TimeZone {
public:
    /// @brief 本地时区（系统时区，默认值）
    static constexpr TimeZone Local() { return TimeZone{}; }
    /// @brief UTC（零偏移）
    static constexpr TimeZone Utc() { return TimeZone( Kind::Utc, 0 ); }
    /**
     * @brief 固定偏移时区
     *
     * @param minutes 相对 UTC 的分钟偏移（+ 表示东区），超出 [-1440, 1440] 时钳制
     */
    static constexpr TimeZone OffsetMinutes( int minutes ) {
        const int clamped = minutes < -1440 ? -1440 : ( minutes > 1440 ? 1440 : minutes );
        return TimeZone( Kind::Offset, static_cast<int16_t>( clamped ) );
    }

    [[nodiscard]] constexpr bool IsLocal() const noexcept { return kind_ == Kind::Local; }
    [[nodiscard]] constexpr bool IsUtc() const noexcept { return kind_ == Kind::Utc; }
    [[nodiscard]] constexpr bool IsOffset() const noexcept { return kind_ == Kind::Offset; }

    /**
     * @brief 静态偏移值（分钟）
     *
     * Utc 返回 0、Offset 返回固定值；Local 返回 0，实际偏移（随夏令时变化）请用 OffsetMinutesAt()。
     */
    [[nodiscard]] constexpr int OffsetMinutes() const noexcept { return offset_minutes_; }

    /**
     * @brief 指定时刻在该时区的实际偏移（分钟，东区为正）
     *
     * Local 时区按系统时区 + 夏令时实时解析；解析失败返回 0。
     */
    [[nodiscard]] int OffsetMinutesAt( const Timestamp &timestamp ) const;

    /**
     * @brief 时区相等性：Local 只等于 Local；Utc 与 Offset(0) 视为等价
     */
    [[nodiscard]] constexpr bool operator==( const TimeZone &other ) const noexcept {
        if ( kind_ == Kind::Local || other.kind_ == Kind::Local ) {
            return kind_ == other.kind_;
        }
        return offset_minutes_ == other.offset_minutes_;
    }
    [[nodiscard]] constexpr bool operator!=( const TimeZone &other ) const noexcept { return !( *this == other ); }

private:
    enum class Kind : int8_t
    {
        Local,
        Utc,
        Offset,
    };

    constexpr TimeZone() = default;
    constexpr TimeZone( Kind kind, int16_t offset_minutes ) : kind_( kind ), offset_minutes_( offset_minutes ) {}

    Kind    kind_{ Kind::Local };
    int16_t offset_minutes_{ 0 };
};

// ==================== 组件结构 ====================

/**
 * @brief 日期时间组件（命名字段，取代元组式访问）
 *
 * 毫秒/微秒/纳秒分别为亚秒部分按位拆分的三位数字
 * （如 12:00:00.123456789 -> ms=123, us=456, ns=789）。
 */
struct DateTimeParts {
    int     year{ 1970 };        ///< 年（1900-9999）
    int     month{ 1 };          ///< 月（1-12）
    int     day{ 1 };            ///< 日（1-31）
    int     hour{ 0 };           ///< 时（0-23）
    int     minute{ 0 };         ///< 分（0-59）
    int     second{ 0 };         ///< 秒（0-59）
    int     millisecond{ 0 };    ///< 毫秒位（0-999）
    int     microsecond{ 0 };    ///< 微秒位（0-999）
    int     nanosecond{ 0 };     ///< 纳秒位（0-999）
    Weekday weekday{ Thursday }; ///< 星期（由转换填充；构造时忽略）
    int     day_of_year{ 1 };    ///< 年内第几天（1-366，由转换填充；构造时忽略）
};

// ==================== DateTime ====================

/**
 * @brief 时区感知的日期时间值类型
 *
 * 定位：墙钟日期时间（chrono / time_t / tm / Unix 数值 / 字符串）之间的统一表示、
 * 转换与运算入口。单调计时请使用 Timer / ElapsedTimer。
 *
 * 语义约定：
 *   - 内部只保存"时刻 + 时区"两个值，所有字段解析按需进行（Local 时区的夏令时始终正确）；
 *   - 时刻比较（==/<）与时区无关；按粒度比较（IsSame<Unit> 等）将双方投影到"接收者时区"后进行；
 *   - 日历运算（Years/Months/Days）保持当地墙上时间与亚秒部分，年/月溢出钳制到目标月末
 *     （如 1-31 加 1 月 = 2-29）；更小单位（Hours 及以下）按绝对时长计算；
 *   - 可失败操作返回 std::optional（解析、从组件构造）；值运算失败返回原值（不抛异常）。
 */
class DateTime {
public:
    // ---- 构造 ----

    constexpr DateTime() = default;

    /**
     * @brief 由规范时刻 + 时区构造
     *
     * @param timestamp 时刻
     * @param zone 时区（默认本地）
     */
    constexpr DateTime( const Timestamp &timestamp, const TimeZone &zone = TimeZone::Local() ) :
        timestamp_( timestamp ), zone_( zone ) {}

    // ---- 工厂 ----

    /**
     * @brief 当前系统时间
     */
    [[nodiscard]] static DateTime Now( const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 校准后的当前时间（设置了时间种子时基于种子推算，否则等价于 Now）
     */
    [[nodiscard]] static DateTime NowSynced( const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 当前系统时间戳（原始系统时间，不受时间种子影响）
     */
    [[nodiscard]] static Timestamp NowTimestamp();

    /**
     * @brief 校准后的当前时间戳（未设置时间种子时等价于 NowTimestamp）
     */
    [[nodiscard]] static Timestamp NowSyncedTimestamp();

    [[nodiscard]] static DateTime FromUnixSeconds( int64_t seconds, const TimeZone &zone = TimeZone::Local() );
    [[nodiscard]] static DateTime FromUnixMillis( int64_t millis, const TimeZone &zone = TimeZone::Local() );
    [[nodiscard]] static DateTime FromUnixMicros( int64_t micros, const TimeZone &zone = TimeZone::Local() );
    [[nodiscard]] static DateTime FromUnixNanos( int64_t nanos, const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 由 time_t（Unix 秒）构造
     */
    [[nodiscard]] static DateTime FromTimeT( std::time_t time, const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 由 std::tm 构造
     *
     * @note tm_isdst 语义：0/1 会被信任（mktime 语义）；其他值（含 -1）交由系统判定。
     *       手工构造 tm 且不确定夏令时状态时，建议显式设置 tm_isdst = -1。
     * @return std::optional<DateTime> 时区转换失败（时间超出可表示范围）时返回 std::nullopt
     */
    [[nodiscard]] static std::optional<DateTime> FromTm( const std::tm         &tm,
                                                         const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 由组件构造
     *
     * @return std::optional<DateTime> 字段非法（年超范围、月/日/时/分/秒/亚秒越界、
     *         日超出当月天数）时返回 std::nullopt；不做自动归一化
     */
    [[nodiscard]] static std::optional<DateTime> FromParts( const DateTimeParts &parts,
                                                            const TimeZone           &zone = TimeZone::Local() );

    /**
     * @brief 解析时间字符串（strftime 占位符风格）
     *
     * 使用 std::get_time。未在格式中出现的字段取默认值
     * （1970-01-01 00:00:00）；严格模式：仅允许尾随空白，其余多余内容判失败。
     *
     * @param text 输入字符串
     * @param format 格式（如 "%Y-%m-%d %H:%M:%S"）
     * @param zone 时区（默认本地）
     * @return std::optional<DateTime> 解析失败或字段非法时返回 std::nullopt
     */
    [[nodiscard]] static std::optional<DateTime> FromString( std::string_view text, std::string_view format,
                                                             const TimeZone &zone = TimeZone::Local() );

    /**
     * @brief 解析 ISO 8601 / RFC 3339 字符串
     *
     * 支持的形式（允许首尾空白）：
     *   - 日期：YYYY-MM-DD
     *   - 日期时间：YYYY-MM-DDTHH:MM 或 YYYY-MM-DDTHH:MM:SS（T 可写作空格）
     *   - 小数秒：.fraction（1-9 位，对应毫秒/微秒/纳秒）
     *   - 时区后缀：Z、±HH:MM、±HHMM、±HH（缺失时按 default_zone 解释）
     *
     * @param text 输入字符串
     * @param default_zone 无时区后缀时的时区（带后缀时返回值时区标记为 Utc/固定偏移）
     * @return std::optional<DateTime> 解析失败或字段非法时返回 std::nullopt
     */
    [[nodiscard]] static std::optional<DateTime> FromIso8601( std::string_view text,
                                                             const TimeZone &default_zone = TimeZone::Local() );

    /**
     * @brief 输出 ISO 8601 字符串
     *
     * 格式：YYYY-MM-DDTHH:MM:SS[.fff|.ffffff|.fffffffff]Z 或 ...±HH:MM
     *   - 亚秒按有效精度输出（毫秒 3 位 / 微秒 6 位 / 纳秒 9 位，全零时省略）；
     *   - Utc 时区输出 "Z"；Local/Offset 输出该时刻的实际偏移（如 +08:00）。
     */
    [[nodiscard]] std::string ToIso8601() const;

    // ---- 导出 ----

    [[nodiscard]] constexpr const Timestamp &ToTimestamp() const noexcept { return timestamp_; }

    /**
     * @brief 转为 time_t（Unix 秒，向下取整）
     *
     * @note 32 位 time_t 平台超出范围时结果由实现定义
     */
    [[nodiscard]] std::time_t ToTimeT() const;

    [[nodiscard]] int64_t ToUnixSeconds() const;  ///< Unix 秒（向下取整）
    [[nodiscard]] int64_t ToUnixMillis() const;   ///< Unix 毫秒（向下取整）
    [[nodiscard]] int64_t ToUnixMicros() const;   ///< Unix 微秒（向下取整）
    [[nodiscard]] int64_t ToUnixNanos() const;    ///< Unix 纳秒（向下取整）

    /**
     * @brief 转为本地时间结构（按自身时区）
     *
     * @return std::optional<std::tm> 时刻超出 tm 可表示范围时返回 std::nullopt
     */
    [[nodiscard]] std::optional<std::tm> ToTm() const;

    /**
     * @brief 解析为组件（按自身时区，含星期与年内日期）
     *
     * @note 解析失败（时刻超范围）时返回默认组件（1970-01-01）
     */
    [[nodiscard]] DateTimeParts ToParts() const;

    /**
     * @brief 转成字符串（strftime 占位符风格，按自身时区）
     *
     * @note 固定偏移时区下 %z 输出 "+0800"、%Z 输出 "UTC+08:00"；
     *       本地/UTC 时区交由系统 strftime 处理
     */
    [[nodiscard]] std::string ToString( std::string_view format = "%Y-%m-%d %H:%M:%S" ) const;

    // ---- 组件访问（便捷方法，等价于 ToParts() 取字段）----

    [[nodiscard]] int Year() const { return ToParts().year; }
    [[nodiscard]] int Month() const { return ToParts().month; }
    [[nodiscard]] int Day() const { return ToParts().day; }
    [[nodiscard]] int Hour() const { return ToParts().hour; }
    [[nodiscard]] int Minute() const { return ToParts().minute; }
    [[nodiscard]] int Second() const { return ToParts().second; }
    [[nodiscard]] int Millisecond() const { return ToParts().millisecond; }
    [[nodiscard]] int Microsecond() const { return ToParts().microsecond; }
    [[nodiscard]] int Nanosecond() const { return ToParts().nanosecond; }
    [[nodiscard]] Weekday GetWeekday() const { return ToParts().weekday; }
    [[nodiscard]] int GetDayOfYear() const { return ToParts().day_of_year; }

    // ---- 日历辅助 ----

    /**
     * @brief 是否为闰年
     */
    [[nodiscard]] static bool IsLeapYear( int year ) noexcept {
        return ( year % 4 == 0 && year % 100 != 0 ) || year % 400 == 0;
    }

    /**
     * @brief 指定年月的天数
     *
     * @param month 1-12；非法返回 0
     */
    [[nodiscard]] static int DaysInMonth( int year, int month ) noexcept;

    /**
     * @brief 所在年份是否闰年（按自身时区）
     */
    [[nodiscard]] bool IsLeapYear() const { return IsLeapYear( Year() ); }

    /**
     * @brief 所在月份的天数（按自身时区）
     */
    [[nodiscard]] int DaysInMonth() const { return DaysInMonth( Year(), Month() ); }

    // ---- 运算 ----

    /**
     * @brief 时间加法（支持全部时间单位）
     *
     * 年/月/日按日历计算（保持墙上时间与亚秒；年/月溢出钳制到目标月末）；
     * 时/分/秒/毫秒/微秒/纳秒按绝对时长计算。
     *
     * @param count 偏移数量，可为负数
     * @return DateTime 计算结果；日历转换失败时返回原值
     */
    template <typename TimeUnit>
    [[nodiscard]] DateTime Add( int64_t count ) const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        if constexpr ( std::is_same_v<TimeUnit, Years> ) {
            bool       clamped = false;
            const auto result  = AddYearsImpl( timestamp_, count, zone_, clamped );
            return DateTime( result.value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Months> ) {
            bool       clamped = false;
            const auto result  = AddMonthsImpl( timestamp_, count, zone_, clamped );
            return DateTime( result.value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Days> ) {
            const auto result = AddDaysImpl( timestamp_, count, zone_ );
            return DateTime( result.value_or( timestamp_ ), zone_ );
        }
        else {
            const auto delta = std::chrono::duration_cast<Timestamp::duration>(
                TimeUnit( static_cast<typename TimeUnit::rep>( count ) ) );
            return DateTime( timestamp_ + delta, zone_ );
        }
    }

    /**
     * @brief 计算“周期区间终点”（排除末尾一天）
     *
     * 从本时刻起加 count 个周期（count > 0），返回该周期作为闭区间时的最后一天，
     * 等价于“先加一个周期、再回退一天”；发生月末钳制时终点即钳制后的月末，不再回退。
     * count <= 0 时等价于 Add。仅支持 年/月/日。
     */
    template <typename TimeUnit>
    [[nodiscard]] DateTime AddRangeEnd( int64_t count ) const {
        static_assert( std::disjunction_v<std::is_same<TimeUnit, Years>, std::is_same<TimeUnit, Months>,
                                          std::is_same<TimeUnit, Days>>,
                       "AddRangeEnd only supports Years/Months/Days" );
        if ( count <= 0 ) {
            return Add<TimeUnit>( count );
        }

        bool                     clamped = false;
        std::optional<Timestamp> period_end;
        if constexpr ( std::is_same_v<TimeUnit, Years> ) {
            period_end = AddYearsImpl( timestamp_, count, zone_, clamped );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Months> ) {
            period_end = AddMonthsImpl( timestamp_, count, zone_, clamped );
        }
        else {
            period_end = AddDaysImpl( timestamp_, count, zone_ );
        }

        if ( !period_end.has_value() ) {
            return *this;
        }
        if ( clamped ) {
            return DateTime( *period_end, zone_ );
        }
        const auto back = AddDaysImpl( *period_end, -1, zone_ );
        return DateTime( back.value_or( *period_end ), zone_ );
    }

    /**
     * @brief 所在周期（按 TimeUnit 粒度）的起点
     *
     * 例：StartOf<Days>() = 当天 00:00:00；StartOf<Months>() = 当月 1 日 00:00:00
     */
    template <typename TimeUnit>
    [[nodiscard]] DateTime StartOf() const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        if constexpr ( std::is_same_v<TimeUnit, Years> ) {
            return DateTime( ClearFields( timestamp_, zone_, FieldLevel::Year ).value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Months> ) {
            return DateTime( ClearFields( timestamp_, zone_, FieldLevel::Month ).value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Days> ) {
            return DateTime( ClearFields( timestamp_, zone_, FieldLevel::Day ).value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Hours> ) {
            return DateTime( ClearFields( timestamp_, zone_, FieldLevel::Hour ).value_or( timestamp_ ), zone_ );
        }
        else if constexpr ( std::is_same_v<TimeUnit, Minutes> ) {
            return DateTime( ClearFields( timestamp_, zone_, FieldLevel::Minute ).value_or( timestamp_ ), zone_ );
        }
        else {
            // 秒及以下：直接按精度向下截断（与时区无关）
            return DateTime( std::chrono::floor<TimeUnit>( timestamp_ ), zone_ );
        }
    }

    /**
     * @brief 所在周期（按 TimeUnit 粒度）的终点
     *
     * 即下一个周期起点减去 1 个时钟节拍。
     * 例：EndOf<Days>() = 当天 23:59:59.999999999（节拍为纳秒时）
     */
    template <typename TimeUnit>
    [[nodiscard]] DateTime EndOf() const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        const DateTime next = StartOf<TimeUnit>().template Add<TimeUnit>( 1 );
        return DateTime( next.timestamp_ - Timestamp::duration( 1 ), zone_ );
    }

    // ---- 时区 ----

    /**
     * @brief 换时区表示（保持时刻不变）
     */
    [[nodiscard]] constexpr DateTime WithZone( const TimeZone &zone ) const { return DateTime( timestamp_, zone ); }

    [[nodiscard]] constexpr const TimeZone &Zone() const noexcept { return zone_; }

    // ---- 比较 ----

    /// @brief 时刻相等（与时区无关）
    [[nodiscard]] constexpr bool operator==( const DateTime &other ) const noexcept {
        return timestamp_ == other.timestamp_;
    }
    [[nodiscard]] constexpr bool operator!=( const DateTime &other ) const noexcept {
        return timestamp_ != other.timestamp_;
    }
    [[nodiscard]] constexpr bool operator<( const DateTime &other ) const noexcept { return timestamp_ < other.timestamp_; }
    [[nodiscard]] constexpr bool operator<=( const DateTime &other ) const noexcept {
        return timestamp_ <= other.timestamp_;
    }
    [[nodiscard]] constexpr bool operator>( const DateTime &other ) const noexcept { return timestamp_ > other.timestamp_; }
    [[nodiscard]] constexpr bool operator>=( const DateTime &other ) const noexcept {
        return timestamp_ >= other.timestamp_;
    }

    /**
     * @brief 按粒度判断是否相同（双方投影到本对象时区后比较）
     *
     * 例：IsSame<Days>(other) 判断是否同一天
     */
    template <typename TimeUnit>
    [[nodiscard]] bool IsSame( const DateTime &other ) const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        return StartOf<TimeUnit>().timestamp_ == other.WithZone( zone_ ).template StartOf<TimeUnit>().timestamp_;
    }

    /// @brief 本时刻是否晚于 other（按粒度）
    template <typename TimeUnit>
    [[nodiscard]] bool IsAfter( const DateTime &other ) const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        return StartOf<TimeUnit>().timestamp_ > other.WithZone( zone_ ).template StartOf<TimeUnit>().timestamp_;
    }

    /// @brief 本时刻是否早于 other（按粒度）
    template <typename TimeUnit>
    [[nodiscard]] bool IsBefore( const DateTime &other ) const {
        static_assert( IsTimeUnit_v<TimeUnit>, "Unsupported time unit" );
        return StartOf<TimeUnit>().timestamp_ < other.WithZone( zone_ ).template StartOf<TimeUnit>().timestamp_;
    }

    template <typename TimeUnit>
    [[nodiscard]] bool IsSameOrAfter( const DateTime &other ) const {
        return IsSame<TimeUnit>( other ) || IsAfter<TimeUnit>( other );
    }

    template <typename TimeUnit>
    [[nodiscard]] bool IsSameOrBefore( const DateTime &other ) const {
        return IsSame<TimeUnit>( other ) || IsBefore<TimeUnit>( other );
    }

    /**
     * @brief 两个时刻的间隔是否超过给定范围（严格大于）
     */
    template <typename Duration>
    [[nodiscard]] bool IsOutOfSpan( const DateTime &other, Duration span ) const {
        auto diff = timestamp_ - other.timestamp_;
        if ( diff < Timestamp::duration::zero() ) {
            diff = -diff;
        }
        return diff > std::chrono::duration_cast<Timestamp::duration>( span );
    }

    // ---- 时间种子（抗系统时间篡改）----

    /**
     * @brief 设置服务器时间种子
     *
     * 记录服务器当前时间与单调时钟锚点；设置后 NowSynced/NowSyncedTimestamp 基于
     * 单调时钟推算，客户端系统时间被向前/向后篡改均不影响，且返回值单调不减。
     */
    static void SetTimeSeed( const Timestamp &server_time );

    /// @brief 清除时间种子
    static void ResetTimeSeed();

    /// @brief 是否已设置时间种子
    [[nodiscard]] static bool HasTimeSeed();

    // ---- 其他 ----

    /**
     * @brief 计算年龄（周岁）
     *
     * 基准时间为 NowSynced()，并投影到 birthday 自身时区计算。
     *
     * @param birthday 出生时间
     * @param add_age_after_birthday 生日次日起才算增长一岁（false 表示生日当天即增长）
     * @return int 年龄；出生时间晚于当前或转换失败时返回 0
     */
    [[nodiscard]] static int AgeInYears( const DateTime &birthday, bool add_age_after_birthday = false );

private:
    enum class FieldLevel : int8_t
    {
        Year,
        Month,
        Day,
        Hour,
        Minute,
    };

    // 按 zone 解析时间戳为字段（Local 用本地时区、Utc/Offset 用偏移后的 UTC 字段）
    [[nodiscard]] static bool ToTmImpl( const Timestamp &timestamp, const TimeZone &zone, std::tm &out ) noexcept;

    // 按 zone 由字段构造时间戳（内部路径不信任 tm_isdst，由实现按 zone 决定语义）
    [[nodiscard]] static std::optional<Timestamp> FromTmImpl( const std::tm &tm, const TimeZone &zone ) noexcept;

    // 按字段级别清零（年/月/日/时/分）
    [[nodiscard]] static std::optional<Timestamp> ClearFields( const Timestamp &timestamp, const TimeZone &zone,
                                                               FieldLevel level ) noexcept;

    // 日历加法（zone 感知；clamped 表示月末钳制）
    [[nodiscard]] static std::optional<Timestamp> AddYearsImpl( const Timestamp &timestamp, int64_t years,
                                                                const TimeZone &zone, bool &clamped ) noexcept;
    [[nodiscard]] static std::optional<Timestamp> AddMonthsImpl( const Timestamp &timestamp, int64_t months,
                                                                 const TimeZone &zone, bool &clamped ) noexcept;
    [[nodiscard]] static std::optional<Timestamp> AddDaysImpl( const Timestamp &timestamp, int64_t days,
                                                               const TimeZone &zone ) noexcept;

private:
    Timestamp timestamp_{};
    TimeZone  zone_{ TimeZone::Local() };
};

}  // namespace utils
