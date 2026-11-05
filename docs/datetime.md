# 日期与时间

头文件：`DateTime.h` · 命名空间：`utils`

`DateTime` 是一个**时区感知的值类型**：内部只保存 `Timestamp`（Unix 纪元、UTC 语义）与 `TimeZone`，
所有格式化、字段拆解、日历运算都按所属时区解释。它是库内唯一的时间基础设施，
`TaskScheduler`、窗口与日志都通过它交换时间。

## 特性

- 三种时区：Local（系统本地，含 DST）、UTC、固定偏移（分钟）。
- 完整的 `ToXXX` / `FromXXX` 转换矩阵：`Timestamp`、`time_t`、`tm`、Unix 秒/毫秒/微秒/纳秒、字符串。
- ISO 8601 解析与输出，支持 `Z`、`±HH:MM`、`±HHMM`、`±HH` 与 1–9 位小数秒。
- 日历运算：年/月/日保持墙钟时间，月末自动钳制；`StartOf` / `EndOf` 周期边界。
- 按粒度比较：`IsSame<Days>`、`IsAfter<Hours>` 等，自动投影到接收者时区。
- 服务器时间种子：`NowSynced()` 基于单调时钟，抗系统改时、单调不减。

## 快速示例

```cpp
#include "DateTime.h"
#include <iostream>
using namespace utils;

DateTime now = DateTime::Now();                          // 本地时区
DateTime utc = DateTime::Now( TimeZone::Utc() );

// 解析 ISO 8601（可带偏移；失败返回 nullopt）
if ( auto parsed = DateTime::FromIso8601( "2026-11-06T12:00:00.250+08:00" ) ) {
    std::cout << parsed->ToIso8601() << '\n';            // 2026-11-06T12:00:00.250+08:00

    // 日历加法：保持墙钟时间，月末钳制
    DateTime next_month = parsed->Add<Months>( 1 );
    std::cout << next_month.ToString( "%Y-%m-%d %H:%M:%S" ) << '\n';

    // 周期边界
    DateTime day_start = parsed->StartOf<Days>();
    DateTime day_end   = parsed->EndOf<Days>();           // 23:59:59.999999999

    // 按粒度比较
    bool same_day = parsed->IsSame<Days>( DateTime::Now() );
    (void)day_start; (void)day_end; (void)same_day;
}
```

## 类型与时间单位

```cpp
struct Years {};
struct Months {};

using Days         = std::chrono::duration<std::int64_t, std::ratio<86400>>;
using Hours        = std::chrono::hours;
using Minutes      = std::chrono::minutes;
using Seconds      = std::chrono::seconds;
using Milliseconds = std::chrono::milliseconds;
using Microseconds = std::chrono::microseconds;
using Nanoseconds  = std::chrono::nanoseconds;

using Timestamp = std::chrono::system_clock::time_point;   // Unix 纪元（UTC 语义）
```

| 名称 | 说明 |
| --- | --- |
| `Years` / `Months` | 日历单位标签，仅用于 `DateTime::Add` / `StartOf` / `EndOf` 等模板参数，不是时长类型 |
| `Days` | 固定 86400 秒的时长；**日历日**语义请用 `Add<Days>`，它按墙钟日期加减 |
| `Hours` … `Nanoseconds` | 标准 `std::chrono` 时长别名 |
| `Timestamp` | 所有转换的枢纽；与 `DateTime` 互转不丢失纳秒 |

```cpp
template <typename T> struct IsTimeUnit;
template <typename T> inline constexpr bool IsTimeUnit_v = IsTimeUnit<T>::value;
```

`IsTimeUnit_v<T>` 对上述单位（含 `Years` / `Months`）为 `true`，可用于约束自己的模板。

## 时区：`TimeZone`

```cpp
class TimeZone {
public:
    static constexpr TimeZone Local();
    static constexpr TimeZone Utc();
    static TimeZone OffsetMinutes( int minutes );   // 钳制到 [-1440, 1440]

    bool IsLocal() const noexcept;
    bool IsUtc() const noexcept;
    bool IsOffset() const noexcept;

    int OffsetMinutes() const noexcept;                          // 静态偏移；Local 返回 0
    int OffsetMinutesAt( const Timestamp &at ) const;   // 该时刻的实际偏移（含 DST）；失败返回 0
};
```

| 工厂 | 含义 | 示例 |
| --- | --- | --- |
| `Local()` | 系统本地时区，格式化/拆解时按该时刻的 DST 规则计算偏移 | `TimeZone::Local()` |
| `Utc()` | UTC | `TimeZone::Utc()` |
| `OffsetMinutes(n)` | 固定偏移，`n` 为东正西负的分钟数 | `OffsetMinutes( 480 )` → UTC+08:00 |

比较规则：

- `Local() == Local()`，不同 `Local` 对象相等；
- `Utc() == OffsetMinutes( 0 )`；
- 其余按偏移量比较。

> `OffsetMinutes()` 对 `Local()` 返回 **0**（静态偏移未知），要拿真实偏移请用
> `OffsetMinutesAt(timestamp)`。这也是格式化 `%z` / `%Z` 时使用的值。

## 字段结构：`DateTimeParts`

```cpp
struct DateTimeParts {
    int      year        = 1970;
    int      month       = 1;     // 1-12
    int      day         = 1;     // 1-31
    int      hour        = 0;     // 0-23
    int      minute      = 0;     // 0-59
    int      second      = 0;     // 0-59（不处理闰秒）
    int      millisecond = 0;     // 0-999
    int      microsecond = 0;     // 0-999
    int      nanosecond  = 0;     // 0-999
    Weekday  weekday     = Weekday::Thursday;
    int      day_of_year = 1;     // 1-366
};
```

亚秒字段是**按十进制位拆分**的，不是“剩余纳秒”：时间戳小数部分 `123456789` 对应
`millisecond=123`、`microsecond=456`、`nanosecond=789`。

```cpp
enum Weekday : int {
    Sunday = 0, Monday = 1, Tuesday = 2, Wednesday = 3,
    Thursday = 4, Friday = 5, Saturday = 6
};
```

## `DateTime` 构造与工厂

```cpp
class DateTime {
public:
    DateTime();                                                   // 1970-01-01 00:00:00 UTC，Local 时区
    DateTime( const Timestamp &ts, const TimeZone &zone = TimeZone::Local() );

    static DateTime Now( const TimeZone &zone = TimeZone::Local() );
    static DateTime NowSynced( const TimeZone &zone = TimeZone::Local() );
    static Timestamp NowTimestamp();
    static Timestamp NowSyncedTimestamp();
};
```

| 工厂 | 说明 |
| --- | --- |
| `Now(zone)` | 读取系统墙钟，按 `zone` 解释 |
| `NowSynced(zone)` | 基于单调时钟 + 时间种子，**抗系统改时、单调不减**；跨进程不保证一致 |
| `NowTimestamp()` / `NowSyncedTimestamp()` | 只取时间戳，不附加时区 |

> 需要“服务器时间不受客户端改时影响”时用 `NowSynced`；纯本地展示用 `Now`。

### 从数值构造（全部 `zone` 默认 `Local()`）

```cpp
static DateTime FromUnixSeconds( std::int64_t seconds, const TimeZone &zone = TimeZone::Local() );
static DateTime FromUnixMillis ( std::int64_t millis,  const TimeZone &zone = TimeZone::Local() );
static DateTime FromUnixMicros ( std::int64_t micros,  const TimeZone &zone = TimeZone::Local() );
static DateTime FromUnixNanos  ( std::int64_t nanos,   const TimeZone &zone = TimeZone::Local() );
static DateTime FromTimeT      ( std::time_t value,    const TimeZone &zone = TimeZone::Local() );
```

这些函数不做范围校验：超出 `Timestamp` 表示范围的值属于未定义行为，调用方需自行保证合理。

### 从 `tm` / 字段构造

```cpp
static std::optional<DateTime> FromTm( const std::tm &tm,
                                       const TimeZone &zone = TimeZone::Local() );

static std::optional<DateTime> FromParts( const DateTimeParts &parts,
                                          const TimeZone &zone = TimeZone::Local() );
```

- `FromTm`：`tm_isdst` 为 `0` / `1` 时被**信任**（0 表示标准时，1 表示夏令时）；
  为 `-1` 或其他值时由系统判定。`tm` 中的 `tm_wday` / `tm_yday` 被忽略。
- `FromParts`：字段越界（如 2 月 30 日、`hour=24`）返回 `nullopt`，**不会自动归一化**。
  `weekday` / `day_of_year` 被忽略，按 `year/month/day/hour/minute/second` 与亚秒字段计算。

### 从字符串构造

```cpp
static std::optional<DateTime> FromString( std::string_view text,
                                           std::string_view format,
                                           const TimeZone &zone = TimeZone::Local() );
```

使用 `std::get_time` 语义（strftime 风格）。注意：

- 格式中**未出现**的字段默认为 `1970-01-01 00:00:00`（与 `std::tm` 初始值一致），不是“当前时间”；
- 只允许尾部空白，其他多余字符视为解析失败；
- 不支持 `%z` / `%Z`，需要解析偏移请用 `FromIso8601`。

<a id="fromiso8601"></a>
```cpp
static std::optional<DateTime> FromIso8601( std::string_view text,
                                            const TimeZone &default_zone = TimeZone::Local() );
```

接受的形式：

| 输入 | 说明 |
| --- | --- |
| `2026-11-06` | 仅日期，时间补 00:00:00 |
| `2026-11-06T12:34:56` | `T` 或空格分隔 |
| `2026-11-06 12:34:56.123456789` | 1–9 位小数秒，右侧补零 |
| `2026-11-06T12:34:56Z` | UTC |
| `2026-11-06T12:34:56+08:00` | 带冒号偏移 |
| `2026-11-06T12:34:56+0800` | 无冒号偏移 |
| `2026-11-06T12:34:56+08` | 仅小时偏移 |

无偏移时使用 `default_zone`。解析失败返回 `nullopt`，不做宽松猜测。

## 输出与转换

```cpp
std::string ToIso8601() const;
Timestamp   ToTimestamp() const &;
std::time_t ToTimeT() const;                 // 向下取整到秒
std::int64_t ToUnixSeconds() const;
std::int64_t ToUnixMillis() const;
std::int64_t ToUnixMicros() const;
std::int64_t ToUnixNanos() const;

std::optional<std::tm> ToTm() const;
DateTimeParts          ToParts() const;      // 失败时返回默认值（1970-01-01）
std::string ToString( std::string_view format = "%Y-%m-%d %H:%M:%S" ) const;
```

### `ToIso8601`

- 亚秒精度按实际值选择：整秒省略小数；毫秒用 3 位，微秒用 6 位，纳秒用 9 位。
  例如 `.250`、`.250000`、`.250000000` 中只会输出与精度匹配的一种。
- `Utc()` 输出 `Z`；`Local()` / 固定偏移输出**该时刻的实际偏移**（`+08:00`）。
- 示例：`2026-11-06T12:00:00.500+08:00`。

### `ToString(format)`

基于 `std::strftime`（`std::put_time` 风格）。固定偏移时：

| 格式符 | 输出 |
| --- | --- |
| `%z` | `+0800` |
| `%Z` | `UTC+08:00`（固定偏移） / 时区缩写（Local、UTC） |

`ToString` 不会抛异常；格式非法时返回 `strftime` 的结果（可能为空串）。

### 组件访问

```cpp
int     Year() const;
int     Month() const;
int     Day() const;
int     Hour() const;
int     Minute() const;
int     Second() const;
int     Millisecond() const;
int     Microsecond() const;
int     Nanosecond() const;
Weekday GetWeekday() const;
int     GetDayOfYear() const;
```

全部按对象所属时区解释。`GetWeekday()` 返回 `enum Weekday`（`int` 底层，可直接与整数互转）。

## 日历与边界

```cpp
static bool IsLeapYear( int year ) noexcept;
static int  DaysInMonth( int year, int month ) noexcept;   // 非法 month 返回 0

bool IsLeapYear() const;
int  DaysInMonth() const;
```

```cpp
template <typename TimeUnit>
DateTime Add( std::int64_t count ) const;

template <typename TimeUnit>
DateTime AddRangeEnd( std::int64_t count ) const;          // 仅 Years / Months / Days

template <typename TimeUnit>
DateTime StartOf() const;

template <typename TimeUnit>
DateTime EndOf() const;
```

<a id="add--addrangeend"></a>
### `Add<TimeUnit>`

| 单位 | 语义 |
| --- | --- |
| `Years` / `Months` | **日历语义**：保持墙钟的日/时/分/秒与亚秒；目标月没有该日时钳制到月末 |
| `Days` | **日历语义**：按墙钟日期加减，跨 DST 时实际经过的时长可能不是 24 小时 |
| `Hours` 及更小 | **绝对时长**：直接作用于时间戳 |

失败（如结果超出 `Timestamp` 表示范围）时**返回原值**，不抛异常。

```cpp
DateTime jan31 = DateTime::FromParts( { 2026, 1, 31 } ).value();
DateTime feb   = jan31.Add<Months>( 1 );    // 2026-02-28（钳制到月末）
DateTime mar   = feb.Add<Months>( 1 );      // 2026-03-28（不会自动“补回”31 日）
```

### `AddRangeEnd`

用于“周期区间”场景：返回“加一个周期再退一天”的日期，即**该周期的最后一天**。

```cpp
DateTime start = DateTime::FromParts( { 2026, 1, 15 } ).value();
DateTime end   = start.AddRangeEnd<Months>( 1 );   // 2026-02-14
// 对 1 月 31 日：Add<Months>(1) = 2 月 28 日，AddRangeEnd<Months>(1) 也为 2 月 28 日（已钳制）
```

`count <= 0` 时等价于 `Add<TimeUnit>(count)`。只支持 `Years` / `Months` / `Days`。

### `StartOf` / `EndOf`

```cpp
dt.StartOf<Days>();     // 当天 00:00:00.000000000
dt.EndOf<Days>();       // 当天 23:59:59.999999999
dt.StartOf<Months>();   // 当月 1 日 00:00:00
dt.EndOf<Years>();      // 当年 12 月 31 日 23:59:59.999999999
dt.StartOf<Hours>();    // 当前小时整点
```

`EndOf<U>` 的实现是 `StartOf<U>` 的下一周期起点减 1 纳秒，因此不会有“月末 23:59:59 但少了 999ms”的问题。
不支持比 `Nanoseconds` 更细的单位。

## 时区转换

```cpp
DateTime WithZone( const TimeZone &zone ) const;   // 时刻不变，只换解释时区
const TimeZone &Zone() const noexcept;
```

```cpp
DateTime shanghai = DateTime::FromIso8601( "2026-11-06T12:00:00Z" ).value();
DateTime local    = shanghai.WithZone( TimeZone::OffsetMinutes( 480 ) );
// 同一时刻：local.ToUnixSeconds() == shanghai.ToUnixSeconds()
// local.Hour() == 20
```

## 比较

```cpp
bool operator==( const DateTime & ) const;
bool operator!=( const DateTime & ) const;
bool operator< ( const DateTime & ) const;
bool operator<=( const DateTime & ) const;
bool operator> ( const DateTime & ) const;
bool operator>=( const DateTime & ) const;
```

`==` / `<` 等**按绝对时刻**比较，与时区无关：

```cpp
DateTime a = DateTime::FromIso8601( "2026-11-06T12:00:00+08:00" ).value();
DateTime b = DateTime::FromIso8601( "2026-11-06T04:00:00Z" ).value();
assert( a == b );
```

### 按粒度比较

```cpp
template <typename Unit> bool IsSame( const DateTime &other ) const;
template <typename Unit> bool IsAfter( const DateTime &other ) const;
template <typename Unit> bool IsBefore( const DateTime &other ) const;
template <typename Unit> bool IsSameOrAfter( const DateTime &other ) const;
template <typename Unit> bool IsSameOrBefore( const DateTime &other ) const;
```

把两个时刻**投影到接收者的时区**后按粒度截断再比较：

```cpp
DateTime morning = DateTime::FromIso8601( "2026-11-06T06:00:00+08:00" ).value();   // 11-05 22:00Z
DateTime night   = DateTime::FromIso8601( "2026-11-06T23:00:00+08:00" ).value();   // 11-06 15:00Z

assert(  morning.IsSame<Days>( night ) );       // +08:00 视角：同一天
assert( !morning.IsSame<Hours>( night ) );

DateTime utc = night.WithZone( TimeZone::Utc() );   // 11-06 15:00Z
assert( morning.IsSame<Days>( utc ) );              // 以 +08:00 解释，仍是 11-06
assert( !utc.IsSame<Days>( morning ) );             // 以 UTC 解释，morning 是 11-05
```

> 比较方向会影响结果，这是刻意设计：**粒度截断发生在接收者（左操作数）的时区**。
> 业务上应让“基准时区”的一方作为左操作数。

### 跨度判断

```cpp
bool IsOutOfSpan( const DateTime &other, const Nanoseconds &span ) const;
```

两个时刻的**绝对差严格大于** `span` 时返回 `true`；相等不算超出。

## 日历查询

```cpp
static int AgeInYears( const DateTime &birthday, bool add_age_after_birthday = false );
```

以 `NowSynced()` 为基准计算周岁：

- 默认生日当天年龄增长；
- `add_age_after_birthday = true` 时生日次日起才增长。

## 时间种子（抗改时）

```cpp
static void      SetTimeSeed( const Timestamp &seed );
static void      ResetTimeSeed();
static bool      HasTimeSeed() noexcept;
```

`NowSynced()` 的正常工作方式是“单调时钟 + 启动时记录的真实时间”。时间种子用于**跨进程固定时间轴**
（测试回放、离线仿真）：设置后，`NowSynced()` 以种子为起点、按单调时长前进。

```cpp
DateTime::SetTimeSeed( DateTime::FromIso8601( "2026-01-01T00:00:00Z" )->ToTimestamp() );
DateTime t1 = DateTime::NowSynced( TimeZone::Utc() );
// ... 单调时钟前进 ...
DateTime t2 = DateTime::NowSynced( TimeZone::Utc() );   // t2 >= t1
DateTime::ResetTimeSeed();                               // 恢复真实时间
```

> 时间种子只影响 `NowSynced`；`Now` / `NowTimestamp` 始终读系统墙钟。

## 语义与边界

- `DateTime` 是**值类型**，拷贝廉价（时间戳 + 时区）；比较与哈希按时间戳。
- 所有 `FromXXX` 的字符串/字段解析失败都返回 `std::optional`，不抛异常。
- 所有 `ToXXX` 都不会抛异常；`ToParts()` / `ToTm()` 失败时返回默认值 / `nullopt`。
- `Add` / `AddRangeEnd` / `StartOf` / `EndOf` 失败时返回**原值**（`StartOf` / `EndOf` 亦不抛）。
- DST 由系统时区数据库决定：
  - 春季不存在的墙钟时刻（如 02:30）会被系统归一化；
  - 秋季重复的墙钟时刻只会对应其中一个绝对时刻（较早者）。
  需要严格的 DST 策略时，任务调度器的 `CalendarTrigger` 已内置“跳过/只触发一次”规则。
- `DateTime` **不处理闰秒**，`second` 范围固定为 0–59。

## 常见问题

**Q：为什么 `DateTime::FromParts` 不自动把 2 月 30 日归一化成 3 月 2 日？**

A：解析与构造应当“要么精确，要么失败”。自动归一化会掩盖输入错误；需要日历加法时请用 `Add<Days>`。

**Q：`Add<Days>(1)` 和 `Add<Hours>(24)` 有什么区别？**

A：前者保持墙钟日期（跨 DST 时实际可能是 23 或 25 小时），后者是绝对 24 小时。日程/报表用前者，
超时/间隔用后者。

**Q：`NowSynced` 能跨机器比较吗？**

A：不能。它是单调时钟推导出的逻辑时间，只保证单进程内单调不减；跨机器请用 `Now()` 或外部时间源。

**Q：如何把 `DateTime` 用作 `std::map` 的键？**

A：直接可用（`operator<` 按绝对时刻）。若需要哈希，请对 `ToTimestamp().time_since_epoch().count()` 取哈希。
