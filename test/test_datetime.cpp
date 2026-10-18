#include <chrono>
#include <cstdint>
#include <ctime>
#include "DateTime.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

DateTime MakeDt( int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int ms = 0, int us = 0,
                 int ns = 0, const TimeZone &zone = TimeZone::Local() ) {
    DateTimeParts parts;
    parts.year        = year;
    parts.month       = month;
    parts.day         = day;
    parts.hour        = hour;
    parts.minute      = minute;
    parts.second      = second;
    parts.millisecond = ms;
    parts.microsecond = us;
    parts.nanosecond  = ns;
    return DateTime::FromParts( parts, zone ).value();
}

}  // namespace

// ============================ 解析 / 格式化 ============================

TEST( DateTimeTest, FromStringAndToString ) {
    const auto dt = DateTime::FromString( "2024-10-16 12:34:56", "%Y-%m-%d %H:%M:%S" );
    ASSERT_TRUE( dt.has_value() );
    EXPECT_EQ( dt->ToString( "%Y-%m-%d %H:%M:%S" ), "2024-10-16 12:34:56" );
    EXPECT_EQ( dt->ToString(), "2024-10-16 12:34:56" );  // 默认格式
}

TEST( DateTimeTest, FromStringInvalid ) {
    // 格式不匹配 / 空输入
    EXPECT_FALSE( DateTime::FromString( "not-a-time", "%Y-%m-%d %H:%M:%S" ).has_value() );
    EXPECT_FALSE( DateTime::FromString( "", "%Y-%m-%d" ).has_value() );
    EXPECT_FALSE( DateTime::FromString( "2024-10-16", "" ).has_value() );

    // 严格模式：尾随垃圾内容判失败，尾随空白可接受
    EXPECT_FALSE( DateTime::FromString( "2024-10-16xyz", "%Y-%m-%d" ).has_value() );
    EXPECT_TRUE( DateTime::FromString( "2024-10-16 ", "%Y-%m-%d" ).has_value() );

    // 非法日期（2 月 30 日）判失败，不做归一化
    EXPECT_FALSE( DateTime::FromString( "2024-02-30", "%Y-%m-%d" ).has_value() );
    EXPECT_TRUE( DateTime::FromString( "2024-02-29", "%Y-%m-%d" ).has_value() );

    // 年超范围
    EXPECT_FALSE( DateTime::FromString( "1899-01-01", "%Y-%m-%d" ).has_value() );
}

TEST( DateTimeTest, FromStringMissingFieldsDefault ) {
    // 未出现的字段取默认值 1970-01-01
    const auto dt = DateTime::FromString( "12:30:45", "%H:%M:%S" );
    ASSERT_TRUE( dt.has_value() );
    EXPECT_EQ( dt->Year(), 1970 );
    EXPECT_EQ( dt->Month(), 1 );
    EXPECT_EQ( dt->Day(), 1 );
    EXPECT_EQ( dt->Hour(), 12 );
    EXPECT_EQ( dt->Minute(), 30 );
    EXPECT_EQ( dt->Second(), 45 );
}

TEST( DateTimeTest, FromStringWithZone ) {
    const auto utc_dt = DateTime::FromString( "2024-10-16 12:00:00", "%Y-%m-%d %H:%M:%S", TimeZone::Utc() );
    ASSERT_TRUE( utc_dt.has_value() );
    EXPECT_EQ( utc_dt->ToUnixSeconds(), MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::Utc() ).ToUnixSeconds() );

    const auto offset_dt =
        DateTime::FromString( "2024-10-16 12:00:00", "%Y-%m-%d %H:%M:%S", TimeZone::OffsetMinutes( 480 ) );
    ASSERT_TRUE( offset_dt.has_value() );
    EXPECT_EQ( offset_dt->Hour(), 12 );                        // 字段与输入一致
    ASSERT_TRUE( utc_dt.has_value() );
    EXPECT_EQ( offset_dt->ToUnixSeconds() + 8 * 3600, utc_dt->ToUnixSeconds() );  // +8 区早 8 小时到达同一时刻
}

TEST( DateTimeTest, FormatOffsetZone ) {
    const auto east = MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::OffsetMinutes( 480 ) );
    EXPECT_EQ( east.ToString( "%Y-%m-%d %H:%M:%S" ), "2024-10-16 12:00:00" );
    EXPECT_EQ( east.ToString( "%z" ), "+0800" );
    EXPECT_EQ( east.ToString( "%Z" ), "UTC+08:00" );

    const auto west = MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::OffsetMinutes( -330 ) );
    EXPECT_EQ( west.ToString( "%z" ), "-0530" );
    EXPECT_EQ( west.ToString( "%Z" ), "UTC-05:30" );
}

// ============================ 组件 ============================

TEST( DateTimeTest, PartsRoundTrip ) {
    const auto dt = MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 789 );
    const auto p  = dt.ToParts();

    EXPECT_EQ( p.year, 2024 );
    EXPECT_EQ( p.month, 10 );
    EXPECT_EQ( p.day, 16 );
    EXPECT_EQ( p.hour, 12 );
    EXPECT_EQ( p.minute, 34 );
    EXPECT_EQ( p.second, 56 );
    EXPECT_EQ( p.millisecond, 123 );
    EXPECT_EQ( p.microsecond, 456 );
    EXPECT_EQ( p.nanosecond, 789 );
    EXPECT_EQ( p.weekday, Wednesday );  // 普通枚举：可直接比较
    EXPECT_EQ( p.weekday, 3 );          // 也可直接与整数互转
    EXPECT_EQ( p.day_of_year, 290 );

    // 组件转回时间戳，亚秒无损
    const auto back = DateTime::FromParts( p );
    ASSERT_TRUE( back.has_value() );
    EXPECT_EQ( back->ToTimestamp(), dt.ToTimestamp() );
}

TEST( DateTimeTest, ComponentAccessors ) {
    const auto dt = MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 789 );
    EXPECT_EQ( dt.Year(), 2024 );
    EXPECT_EQ( dt.Month(), 10 );
    EXPECT_EQ( dt.Day(), 16 );
    EXPECT_EQ( dt.Hour(), 12 );
    EXPECT_EQ( dt.Minute(), 34 );
    EXPECT_EQ( dt.Second(), 56 );
    EXPECT_EQ( dt.Millisecond(), 123 );
    EXPECT_EQ( dt.Microsecond(), 456 );
    EXPECT_EQ( dt.Nanosecond(), 789 );
    EXPECT_EQ( dt.GetWeekday(), Wednesday );
    EXPECT_EQ( dt.GetDayOfYear(), 290 );
}

TEST( DateTimeTest, FromPartsInvalid ) {
    auto make = []( int year, int month, int day ) {
        DateTimeParts parts;
        parts.year  = year;
        parts.month = month;
        parts.day   = day;
        return DateTime::FromParts( parts );
    };

    EXPECT_FALSE( make( 1899, 1, 1 ).has_value() );  // 年超范围
    EXPECT_FALSE( make( 2024, 13, 1 ).has_value() );  // 非法月
    EXPECT_FALSE( make( 2024, 0, 1 ).has_value() );
    EXPECT_FALSE( make( 2024, 2, 30 ).has_value() );  // 日超出当月天数
    EXPECT_FALSE( make( 2023, 2, 29 ).has_value() );
    EXPECT_TRUE( make( 2024, 2, 29 ).has_value() );  // 闰年

    DateTimeParts parts;
    parts.year   = 2024;
    parts.month  = 1;
    parts.day    = 1;
    parts.hour   = 24;
    EXPECT_FALSE( DateTime::FromParts( parts ).has_value() );
    parts.hour   = 23;
    parts.minute = 60;
    EXPECT_FALSE( DateTime::FromParts( parts ).has_value() );
    parts.minute      = 59;
    parts.second      = 60;
    EXPECT_FALSE( DateTime::FromParts( parts ).has_value() );
    parts.second      = 59;
    parts.millisecond = 1000;
    EXPECT_FALSE( DateTime::FromParts( parts ).has_value() );
}

// ============================ 转换矩阵 ============================

TEST( DateTimeTest, UnixConversions ) {
    const auto dt = DateTime::FromUnixMillis( 1730000000000LL );
    EXPECT_EQ( dt.ToUnixMillis(), 1730000000000LL );
    EXPECT_EQ( dt.ToUnixSeconds(), 1730000000LL );
    EXPECT_EQ( dt.ToUnixMicros(), 1730000000000000LL );
    EXPECT_EQ( dt.ToUnixNanos(), 1730000000000000000LL );

    EXPECT_EQ( DateTime::FromUnixSeconds( 1700000000 ).ToUnixSeconds(), 1700000000 );
    EXPECT_EQ( DateTime::FromUnixMicros( 1730000000123456LL ).ToUnixMicros(), 1730000000123456LL );
    EXPECT_EQ( DateTime::FromUnixNanos( 1730000000123456789LL ).ToUnixNanos(), 1730000000123456789LL );

    // 负值（1970 前）向下取整语义
    EXPECT_EQ( DateTime::FromUnixSeconds( -1 ).ToUnixMillis(), -1000 );
    EXPECT_EQ( DateTime::FromUnixMillis( -1 ).ToUnixSeconds(), -1 );    // -1ms 向下取整为 -1s
    EXPECT_EQ( DateTime::FromUnixMillis( -1001 ).ToUnixSeconds(), -2 );

    // 亚秒字段与毫秒戳一致
    const auto ms_dt = DateTime::FromUnixMillis( 1730000000123LL );
    EXPECT_EQ( ms_dt.Millisecond(), 123 );
}

TEST( DateTimeTest, TimeTConversions ) {
    const std::time_t t  = 1730000000;
    const auto        dt = DateTime::FromTimeT( t );
    EXPECT_EQ( dt.ToTimeT(), t );
    EXPECT_EQ( dt.ToUnixSeconds(), static_cast<int64_t>( t ) );
}

TEST( DateTimeTest, TmConversions ) {
    const auto dt = MakeDt( 2024, 10, 16, 12, 34, 56 );

    // 本地时区往返
    const auto tm_local = dt.ToTm();
    ASSERT_TRUE( tm_local.has_value() );
    const auto back_local = DateTime::FromTm( *tm_local );
    ASSERT_TRUE( back_local.has_value() );
    EXPECT_EQ( back_local->ToUnixSeconds(), dt.ToUnixSeconds() );

    // UTC 时区往返
    const auto utc_dt = dt.WithZone( TimeZone::Utc() );
    const auto tm_utc = utc_dt.ToTm();
    ASSERT_TRUE( tm_utc.has_value() );
    const auto back_utc = DateTime::FromTm( *tm_utc, TimeZone::Utc() );
    ASSERT_TRUE( back_utc.has_value() );
    EXPECT_EQ( back_utc->ToUnixNanos(), dt.ToUnixNanos() );
}

TEST( DateTimeTest, TimestampInterop ) {
    // 与 chrono 无缝互操作
    const Timestamp now = DateTime::NowTimestamp();
    const DateTime  dt( now );
    EXPECT_EQ( dt.ToTimestamp(), now );
    EXPECT_EQ( dt.Zone(), TimeZone::Local() );

    // 隐式转换 + 时刻比较
    EXPECT_TRUE( dt == now );
    EXPECT_TRUE( dt <= DateTime( now + std::chrono::seconds( 1 ) ) );
}

// ============================ 时区 ============================

TEST( DateTimeTest, TimeZoneBasics ) {
    EXPECT_TRUE( TimeZone::Local().IsLocal() );
    EXPECT_TRUE( TimeZone::Utc().IsUtc() );
    EXPECT_TRUE( TimeZone::OffsetMinutes( 480 ).IsOffset() );

    EXPECT_EQ( TimeZone::Utc().OffsetMinutes(), 0 );
    EXPECT_EQ( TimeZone::OffsetMinutes( 480 ).OffsetMinutes(), 480 );

    // 相等性：Local 只等于 Local；Utc 与 Offset(0) 等价
    EXPECT_TRUE( TimeZone::Utc() == TimeZone::OffsetMinutes( 0 ) );
    EXPECT_FALSE( TimeZone::Utc() == TimeZone::OffsetMinutes( 60 ) );
    EXPECT_FALSE( TimeZone::Local() == TimeZone::Utc() );
    EXPECT_TRUE( TimeZone::Local() == TimeZone::Local() );

    // 越界钳制
    EXPECT_EQ( TimeZone::OffsetMinutes( 5000 ).OffsetMinutes(), 1440 );
    EXPECT_EQ( TimeZone::OffsetMinutes( -5000 ).OffsetMinutes(), -1440 );
}

TEST( DateTimeTest, WithZoneKeepsInstant ) {
    const auto utc_dt = MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::Utc() );
    const auto east   = utc_dt.WithZone( TimeZone::OffsetMinutes( 480 ) );
    const auto west   = utc_dt.WithZone( TimeZone::OffsetMinutes( -300 ) );

    // 同一时刻
    EXPECT_EQ( east.ToTimestamp(), utc_dt.ToTimestamp() );
    EXPECT_EQ( west.ToTimestamp(), utc_dt.ToTimestamp() );
    EXPECT_EQ( east.ToUnixSeconds(), utc_dt.ToUnixSeconds() );

    // 字段随偏移变化
    EXPECT_EQ( east.Hour(), 20 );  // 12:00 UTC = 20:00 +08:00
    EXPECT_EQ( west.Hour(), 7 );   // 12:00 UTC = 07:00 -05:00
    EXPECT_EQ( east.Year(), 2024 );
}

TEST( DateTimeTest, OffsetMinutesAt ) {
    const auto ts = MakeDt( 2024, 6, 15, 12, 0, 0 ).ToTimestamp();

    EXPECT_EQ( TimeZone::Utc().OffsetMinutesAt( ts ), 0 );
    EXPECT_EQ( TimeZone::OffsetMinutes( 480 ).OffsetMinutesAt( ts ), 480 );

    // Local：解析出的实际偏移应能复现相同字段（与具体时区无关的一致性检查）
    const int local_offset = TimeZone::Local().OffsetMinutesAt( ts );
    const auto shifted     = DateTime( ts, TimeZone::OffsetMinutes( local_offset ) );
    EXPECT_EQ( shifted.Hour(), 12 );
    EXPECT_EQ( shifted.Day(), 15 );
}

// ============================ 周期边界 ============================

TEST( DateTimeTest, StartOfEndOfPeriod ) {
    const auto dt = MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 789 );

    const auto day_start = dt.StartOf<Days>().ToParts();
    EXPECT_EQ( day_start.day, 16 );
    EXPECT_EQ( day_start.hour, 0 );
    EXPECT_EQ( day_start.minute, 0 );
    EXPECT_EQ( day_start.second, 0 );
    EXPECT_EQ( day_start.nanosecond, 0 );

    const auto day_end = dt.EndOf<Days>().ToParts();
    EXPECT_EQ( day_end.day, 16 );
    EXPECT_EQ( day_end.hour, 23 );
    EXPECT_EQ( day_end.minute, 59 );
    EXPECT_EQ( day_end.second, 59 );
    EXPECT_EQ( day_end.millisecond, 999 );
    EXPECT_EQ( day_end.microsecond, 999 );
    EXPECT_EQ( day_end.nanosecond, 999 );

    const auto hour_end = dt.EndOf<Hours>().ToParts();
    EXPECT_EQ( hour_end.hour, 12 );
    EXPECT_EQ( hour_end.minute, 59 );
    EXPECT_EQ( hour_end.second, 59 );

    // 闰年 2 月（月末边界）
    const auto month_end = MakeDt( 2024, 2, 10 ).EndOf<Months>().ToParts();
    EXPECT_EQ( month_end.month, 2 );
    EXPECT_EQ( month_end.day, 29 );
    EXPECT_EQ( month_end.hour, 23 );

    const auto year_end = dt.EndOf<Years>().ToParts();
    EXPECT_EQ( year_end.month, 12 );
    EXPECT_EQ( year_end.day, 31 );
    EXPECT_EQ( year_end.hour, 23 );

    // 亚秒级：按精度截断
    const auto ms_start = dt.StartOf<Milliseconds>().ToParts();
    EXPECT_EQ( ms_start.millisecond, 123 );
    EXPECT_EQ( ms_start.microsecond, 0 );
    EXPECT_EQ( ms_start.nanosecond, 0 );

    const auto sec_start = dt.StartOf<Seconds>().ToParts();
    EXPECT_EQ( sec_start.second, 56 );
    EXPECT_EQ( sec_start.millisecond, 0 );
}

// ============================ 日历辅助 ============================

TEST( DateTimeTest, TimeUnitTrait ) {
    static_assert( IsTimeUnit_v<Years> && IsTimeUnit_v<Months> && IsTimeUnit_v<Nanoseconds> );
    static_assert( !IsTimeUnit_v<int> && !IsTimeUnit_v<Timestamp> );
    SUCCEED();
}

TEST( DateTimeTest, IsLeapYearAndDaysInMonth ) {
    EXPECT_TRUE( DateTime::IsLeapYear( 2000 ) );
    EXPECT_TRUE( DateTime::IsLeapYear( 2024 ) );
    EXPECT_FALSE( DateTime::IsLeapYear( 1900 ) );
    EXPECT_FALSE( DateTime::IsLeapYear( 2023 ) );

    EXPECT_EQ( DateTime::DaysInMonth( 2024, 1 ), 31 );
    EXPECT_EQ( DateTime::DaysInMonth( 2024, 2 ), 29 );
    EXPECT_EQ( DateTime::DaysInMonth( 2023, 2 ), 28 );
    EXPECT_EQ( DateTime::DaysInMonth( 2024, 4 ), 30 );
    EXPECT_EQ( DateTime::DaysInMonth( 2024, 0 ), 0 );
    EXPECT_EQ( DateTime::DaysInMonth( 2024, 13 ), 0 );

    const auto feb = MakeDt( 2024, 2, 10 );
    EXPECT_TRUE( feb.IsLeapYear() );
    EXPECT_EQ( feb.DaysInMonth(), 29 );
}

// ============================ 运算 ============================

TEST( DateTimeTest, AddDurationUnits ) {
    const auto base = MakeDt( 2024, 10, 16, 12, 0, 0 );

    EXPECT_TRUE( base.Add<Hours>( 1 ) == base.ToTimestamp() + std::chrono::hours( 1 ) );
    EXPECT_TRUE( base.Add<Minutes>( -30 ) == base.ToTimestamp() - std::chrono::minutes( 30 ) );
    EXPECT_TRUE( base.Add<Seconds>( 90 ) == base.ToTimestamp() + std::chrono::seconds( 90 ) );
    EXPECT_TRUE( base.Add<Milliseconds>( 1500 ) == base.ToTimestamp() + std::chrono::milliseconds( 1500 ) );
    EXPECT_TRUE( base.Add<Microseconds>( 123456 ) == base.ToTimestamp() + std::chrono::microseconds( 123456 ) );
    EXPECT_TRUE( base.Add<Nanoseconds>( 987654321 ) == base.ToTimestamp() + std::chrono::nanoseconds( 987654321 ) );

    // 保留时区
    const auto utc = MakeDt( 2024, 1, 1, 0, 0, 0, 0, 0, 0, TimeZone::Utc() );
    EXPECT_EQ( utc.Add<Days>( 1 ).Zone(), TimeZone::Utc() );
}

TEST( DateTimeTest, AddDaysPreservesWallTimeAndSubSeconds ) {
    const auto base = MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 789 );

    const auto next = base.Add<Days>( 1 ).ToParts();
    EXPECT_EQ( next.day, 17 );
    EXPECT_EQ( next.hour, 12 );
    EXPECT_EQ( next.minute, 34 );
    EXPECT_EQ( next.second, 56 );
    EXPECT_EQ( next.millisecond, 123 );
    EXPECT_EQ( next.microsecond, 456 );
    EXPECT_EQ( next.nanosecond, 789 );

    const auto prev = base.Add<Days>( -20 ).ToParts();
    EXPECT_EQ( prev.month, 9 );
    EXPECT_EQ( prev.day, 26 );
    EXPECT_EQ( prev.hour, 12 );

    // 跨年归一化
    const auto new_year = MakeDt( 2024, 12, 31 ).Add<Days>( 1 ).ToParts();
    EXPECT_EQ( new_year.year, 2025 );
    EXPECT_EQ( new_year.month, 1 );
    EXPECT_EQ( new_year.day, 1 );
}

TEST( DateTimeTest, AddMonthsClampsToMonthEnd ) {
    // 月末日期溢出到目标月不存在时必须钳制，而不是滚动到下个月
    auto check = []( const DateTime &base, int64_t months, int expect_year, int expect_month, int expect_day ) {
        const auto p = base.Add<Months>( months ).ToParts();
        EXPECT_EQ( p.year, expect_year );
        EXPECT_EQ( p.month, expect_month );
        EXPECT_EQ( p.day, expect_day );
    };

    check( MakeDt( 2024, 1, 31 ), 1, 2024, 2, 29 );
    check( MakeDt( 2023, 1, 31 ), 1, 2023, 2, 28 );
    check( MakeDt( 2024, 3, 31 ), -1, 2024, 2, 29 );
    check( MakeDt( 2024, 1, 15 ), 1, 2024, 2, 15 );
    check( MakeDt( 2024, 12, 15 ), 1, 2025, 1, 15 );
    check( MakeDt( 2024, 1, 15 ), -1, 2023, 12, 15 );

    // 亚秒保留
    const auto sub = MakeDt( 2024, 1, 15, 8, 30, 0, 111, 222, 333 ).Add<Months>( 1 ).ToParts();
    EXPECT_EQ( sub.month, 2 );
    EXPECT_EQ( sub.millisecond, 111 );
    EXPECT_EQ( sub.nanosecond, 333 );
}

TEST( DateTimeTest, AddYearsClampsLeapDay ) {
    auto check = []( const DateTime &base, int64_t years, int expect_year, int expect_month, int expect_day ) {
        const auto p = base.Add<Years>( years ).ToParts();
        EXPECT_EQ( p.year, expect_year );
        EXPECT_EQ( p.month, expect_month );
        EXPECT_EQ( p.day, expect_day );
    };

    check( MakeDt( 2024, 2, 29 ), 1, 2025, 2, 28 );
    check( MakeDt( 2024, 2, 29 ), -1, 2023, 2, 28 );
    check( MakeDt( 2024, 1, 15 ), 1, 2025, 1, 15 );
}

TEST( DateTimeTest, AddRangeEnd ) {
    auto check = []( const DateTime &base, const DateTime &end, int expect_year, int expect_month, int expect_day ) {
        EXPECT_TRUE( end >= base );
        const auto p = end.ToParts();
        EXPECT_EQ( p.year, expect_year );
        EXPECT_EQ( p.month, expect_month );
        EXPECT_EQ( p.day, expect_day );
    };

    // 日：加 30 天后回退一天
    check( MakeDt( 2024, 1, 15 ), MakeDt( 2024, 1, 15 ).AddRangeEnd<Days>( 30 ), 2024, 2, 13 );

    // 月：普通日期回退一天
    check( MakeDt( 2024, 1, 15 ), MakeDt( 2024, 1, 15 ).AddRangeEnd<Months>( 1 ), 2024, 2, 14 );
    // 月：月末钳制时终点即目标月末，不再回退
    check( MakeDt( 2024, 1, 31 ), MakeDt( 2024, 1, 31 ).AddRangeEnd<Months>( 1 ), 2024, 2, 29 );
    check( MakeDt( 2023, 1, 30 ), MakeDt( 2023, 1, 30 ).AddRangeEnd<Months>( 1 ), 2023, 2, 28 );
    // 月：29 日在闰年 2 月存在（无钳制）→ 回退一天
    check( MakeDt( 2024, 1, 29 ), MakeDt( 2024, 1, 29 ).AddRangeEnd<Months>( 1 ), 2024, 2, 28 );

    // 年
    check( MakeDt( 2024, 1, 31 ), MakeDt( 2024, 1, 31 ).AddRangeEnd<Years>( 1 ), 2025, 1, 30 );
    check( MakeDt( 2024, 2, 29 ), MakeDt( 2024, 2, 29 ).AddRangeEnd<Years>( 1 ), 2025, 2, 28 );

    // count <= 0：等价于普通加法
    EXPECT_TRUE( MakeDt( 2024, 1, 15 ).AddRangeEnd<Months>( 0 ) == MakeDt( 2024, 1, 15 ) );
    const auto back = MakeDt( 2024, 1, 15 ).AddRangeEnd<Months>( -1 ).ToParts();
    EXPECT_EQ( back.year, 2023 );
    EXPECT_EQ( back.month, 12 );
    EXPECT_EQ( back.day, 15 );
}

// ============================ 比较 ============================

TEST( DateTimeTest, ComparisonByUnit ) {
    const auto day1      = MakeDt( 2024, 10, 16, 8, 0, 0 );
    const auto day1_late = MakeDt( 2024, 10, 16, 23, 59, 59, 999, 999, 999 );
    const auto day2      = MakeDt( 2024, 10, 17, 0, 0, 0 );

    EXPECT_TRUE( day1.IsSame<Days>( day1_late ) );
    EXPECT_FALSE( day1.IsSame<Days>( day2 ) );
    EXPECT_TRUE( day2.IsAfter<Days>( day1 ) );
    EXPECT_TRUE( day1.IsBefore<Days>( day2 ) );
    EXPECT_TRUE( day1.IsSameOrAfter<Days>( day1_late ) );
    EXPECT_TRUE( day1.IsSameOrBefore<Days>( day1_late ) );
    EXPECT_FALSE( day2.IsSameOrBefore<Days>( day1 ) );

    // 年/月粒度
    EXPECT_TRUE( MakeDt( 2024, 1, 1 ).IsSame<Years>( MakeDt( 2024, 12, 31 ) ) );
    EXPECT_FALSE( MakeDt( 2023, 12, 31 ).IsSame<Years>( MakeDt( 2024, 1, 1 ) ) );
    EXPECT_TRUE( MakeDt( 2024, 10, 1 ).IsSame<Months>( MakeDt( 2024, 10, 31 ) ) );
    EXPECT_TRUE( MakeDt( 2024, 11, 1 ).IsAfter<Months>( MakeDt( 2024, 10, 31 ) ) );

    // 小时/秒粒度：先按粒度截断再比较
    EXPECT_TRUE( MakeDt( 2024, 10, 16, 12, 0, 0 ).IsSame<Hours>( MakeDt( 2024, 10, 16, 12, 59, 59 ) ) );
    EXPECT_TRUE( MakeDt( 2024, 10, 16, 12, 0, 0, 999, 999, 999 )
                     .IsSame<Seconds>( MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 1 ) ) );
    EXPECT_FALSE( MakeDt( 2024, 10, 16, 12, 0, 1 ).IsSame<Seconds>( MakeDt( 2024, 10, 16, 12, 0, 0 ) ) );
}

TEST( DateTimeTest, CrossZoneComparison ) {
    // 同一时刻在不同时区的"同一天"判断：以接收者时区为准
    const auto a = MakeDt( 2024, 10, 16, 23, 30, 0, 0, 0, 0, TimeZone::Utc() );
    const auto b = MakeDt( 2024, 10, 17, 1, 0, 0, 0, 0, 0, TimeZone::Utc() );

    // UTC 视角：16 日 vs 17 日
    EXPECT_FALSE( a.IsSame<Days>( b ) );

    // +08:00 视角：均为 17 日
    const auto a_east = a.WithZone( TimeZone::OffsetMinutes( 480 ) );
    const auto b_east = b.WithZone( TimeZone::OffsetMinutes( 480 ) );
    EXPECT_TRUE( a_east.IsSame<Days>( b_east ) );
    EXPECT_TRUE( a_east.IsSame<Days>( b ) );   // b 投影到 +08:00 后同为 17 日
    EXPECT_FALSE( b.IsSame<Days>( a_east ) );  // 接收者 UTC：b=17 日；a 投影为 16 日
}

TEST( DateTimeTest, IsOutOfSpan ) {
    const auto a = MakeDt( 2024, 10, 16, 12, 0, 0 );
    const auto b = MakeDt( 2024, 10, 16, 12, 10, 0 );

    // 严格大于：恰好等于 span 不算超出
    EXPECT_FALSE( a.IsOutOfSpan( b, std::chrono::minutes( 10 ) ) );
    EXPECT_TRUE( a.IsOutOfSpan( b, std::chrono::minutes( 9 ) ) );
    EXPECT_FALSE( a.IsOutOfSpan( b, std::chrono::seconds( 600 ) ) );
    EXPECT_TRUE( a.IsOutOfSpan( b, std::chrono::seconds( 599 ) ) );

    // 与参数顺序无关
    EXPECT_TRUE( b.IsOutOfSpan( a, std::chrono::minutes( 9 ) ) );
}

// ============================ 时间种子 / 年龄 ============================

TEST( DateTimeTest, ServerTimeSeed ) {
    DateTime::ResetTimeSeed();
    EXPECT_FALSE( DateTime::HasTimeSeed() );

    // 未设置种子：NowSynced 即系统时间
    EXPECT_FALSE( DateTime::NowSynced( TimeZone::Utc() )
                      .IsOutOfSpan( DateTime::Now( TimeZone::Utc() ), std::chrono::seconds( 1 ) ) );

    // 设置未来时间作为种子，便于区分系统时间
    const auto server_time = MakeDt( 2030, 1, 1, 0, 0, 0, 0, 0, 0, TimeZone::Utc() ).ToTimestamp();
    DateTime::SetTimeSeed( server_time );
    EXPECT_TRUE( DateTime::HasTimeSeed() );

    const auto first  = DateTime::NowSyncedTimestamp();
    const auto second = DateTime::NowSyncedTimestamp();
    EXPECT_GE( first, server_time );                              // 基于种子时间
    EXPECT_GE( second, first );                                   // 单调不减
    EXPECT_LT( first, server_time + std::chrono::seconds( 5 ) );  // 刚设置，偏差很小

    // 重置后恢复系统时间
    DateTime::ResetTimeSeed();
    EXPECT_FALSE( DateTime::HasTimeSeed() );
    EXPECT_FALSE( DateTime::NowSynced( TimeZone::Utc() )
                      .IsOutOfSpan( DateTime::Now( TimeZone::Utc() ), std::chrono::seconds( 1 ) ) );
}

TEST( DateTimeTest, AgeInYears ) {
    // 通过时间种子固定"当前时间"，使测试不受运行时刻影响
    const auto reference = MakeDt( 2024, 10, 16, 12, 0, 0 );
    DateTime::SetTimeSeed( reference.ToTimestamp() );

    // 出生时间晚于当前 → 0
    EXPECT_EQ( DateTime::AgeInYears( reference.Add<Days>( 1 ) ), 0 );

    // 25 年前的今天：默认生日当天即增长；flag=true 时次日起才增长
    const auto birthday_today = reference.Add<Years>( -25 );  // 1999-10-16
    EXPECT_EQ( DateTime::AgeInYears( birthday_today ), 25 );
    EXPECT_EQ( DateTime::AgeInYears( birthday_today, true ), 24 );

    // 生日尚未到（明天才到生日）
    const auto birthday_tomorrow = birthday_today.Add<Days>( 1 );  // 1999-10-17
    EXPECT_EQ( DateTime::AgeInYears( birthday_tomorrow ), 24 );
    EXPECT_EQ( DateTime::AgeInYears( birthday_tomorrow, true ), 24 );

    DateTime::ResetTimeSeed();
}

// ============================ ISO 8601 ============================

TEST( DateTimeTest, Iso8601Parse ) {
    // 完整形式（Z）
    const auto z = DateTime::FromIso8601( "2024-10-16T12:34:56Z" );
    ASSERT_TRUE( z.has_value() );
    EXPECT_EQ( z->Zone(), TimeZone::Utc() );
    EXPECT_EQ( z->ToUnixSeconds(), MakeDt( 2024, 10, 16, 12, 34, 56, 0, 0, 0, TimeZone::Utc() ).ToUnixSeconds() );

    // 固定偏移
    const auto east = DateTime::FromIso8601( "2024-10-16T12:34:56+08:00" );
    ASSERT_TRUE( east.has_value() );
    EXPECT_EQ( east->Zone(), TimeZone::OffsetMinutes( 480 ) );
    EXPECT_EQ( east->Hour(), 12 );
    EXPECT_EQ( east->ToUnixSeconds(), MakeDt( 2024, 10, 16, 4, 34, 56, 0, 0, 0, TimeZone::Utc() ).ToUnixSeconds() );

    // 无时区后缀 → 默认时区
    const auto naive = DateTime::FromIso8601( "2024-10-16 12:34:56", TimeZone::Utc() );
    ASSERT_TRUE( naive.has_value() );
    EXPECT_EQ( naive->Zone(), TimeZone::Utc() );
    EXPECT_EQ( naive->ToUnixSeconds(), MakeDt( 2024, 10, 16, 12, 34, 56, 0, 0, 0, TimeZone::Utc() ).ToUnixSeconds() );

    // 日期 / 分钟精度
    const auto date_only = DateTime::FromIso8601( "2024-10-16", TimeZone::Utc() );
    ASSERT_TRUE( date_only.has_value() );
    EXPECT_EQ( date_only->Day(), 16 );
    EXPECT_EQ( date_only->Hour(), 0 );
    const auto minute_precision = DateTime::FromIso8601( "2024-10-16T12:34", TimeZone::Utc() );
    ASSERT_TRUE( minute_precision.has_value() );
    EXPECT_EQ( minute_precision->Minute(), 34 );
    EXPECT_EQ( minute_precision->Second(), 0 );

    // 小数秒（毫秒/微秒/纳秒）
    const auto ms = DateTime::FromIso8601( "2024-10-16T12:34:56.123Z" );
    ASSERT_TRUE( ms.has_value() );
    EXPECT_EQ( ms->Millisecond(), 123 );
    EXPECT_EQ( ms->Microsecond(), 0 );
    const auto us = DateTime::FromIso8601( "2024-10-16T12:34:56.123456Z" );
    ASSERT_TRUE( us.has_value() );
    EXPECT_EQ( us->Millisecond(), 123 );
    EXPECT_EQ( us->Microsecond(), 456 );
    EXPECT_EQ( us->Nanosecond(), 0 );
    const auto ns = DateTime::FromIso8601( "2024-10-16T12:34:56.123456789Z" );
    ASSERT_TRUE( ns.has_value() );
    EXPECT_EQ( ns->Nanosecond(), 789 );

    // ±HHMM / ±HH 紧凑形式
    const auto compact = DateTime::FromIso8601( "2024-10-16T12:34:56+0800" );
    ASSERT_TRUE( compact.has_value() );
    EXPECT_EQ( compact->Zone(), TimeZone::OffsetMinutes( 480 ) );
    const auto hours_only = DateTime::FromIso8601( "2024-10-16T12:34:56-05" );
    ASSERT_TRUE( hours_only.has_value() );
    EXPECT_EQ( hours_only->Zone(), TimeZone::OffsetMinutes( -300 ) );

    // 首尾空白
    EXPECT_TRUE( DateTime::FromIso8601( "  2024-10-16T12:34:56Z  " ).has_value() );
}

TEST( DateTimeTest, Iso8601Invalid ) {
    EXPECT_FALSE( DateTime::FromIso8601( "" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-13-01" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-02-30" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T25:00:00Z" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T12:61:00Z" ).has_value() );
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T12:34:56.Z" ).has_value() );            // 空小数
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T12:34:56.1234567891Z" ).has_value() );   // 超过 9 位
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T12:34:56+24:00" ).has_value() );         // 偏移越界
    EXPECT_FALSE( DateTime::FromIso8601( "2024-10-16T12:34:56Z extra" ).has_value() );        // 残余内容
    EXPECT_FALSE( DateTime::FromIso8601( "20241016T123456Z" ).has_value() );                  // 不支持紧凑日期
}

TEST( DateTimeTest, Iso8601Format ) {
    // UTC → Z
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 34, 56, 0, 0, 0, TimeZone::Utc() ).ToIso8601(), "2024-10-16T12:34:56Z" );

    // 亚秒精度选择（3/6/9 位）
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 34, 56, 123, 0, 0, TimeZone::Utc() ).ToIso8601(),
               "2024-10-16T12:34:56.123Z" );
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 0, TimeZone::Utc() ).ToIso8601(),
               "2024-10-16T12:34:56.123456Z" );
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 34, 56, 123, 456, 789, TimeZone::Utc() ).ToIso8601(),
               "2024-10-16T12:34:56.123456789Z" );

    // 固定偏移
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::OffsetMinutes( 480 ) ).ToIso8601(),
               "2024-10-16T12:00:00+08:00" );
    EXPECT_EQ( MakeDt( 2024, 10, 16, 12, 0, 0, 0, 0, 0, TimeZone::OffsetMinutes( -330 ) ).ToIso8601(),
               "2024-10-16T12:00:00-05:30" );

    // 字符串往返一致
    const std::string text = "2024-10-16T12:34:56.123456+08:00";
    const auto        back = DateTime::FromIso8601( text );
    ASSERT_TRUE( back.has_value() );
    EXPECT_EQ( back->ToIso8601(), text );

    // 时刻往返一致（不同表示的同一时刻）
    const auto utc = DateTime::FromUnixMillis( 1730000000123LL, TimeZone::Utc() );
    const auto again = DateTime::FromIso8601( utc.ToIso8601() );
    ASSERT_TRUE( again.has_value() );
    EXPECT_EQ( again->ToUnixMillis(), utc.ToUnixMillis() );
}
