# 并发与计时

头文件：`ThreadPoolExecutor.h`、`TaskExecutor.h`、`Clock.h`、`ElapsedTimer.h`、`DeadlineTimer.h`、`Throttler.h`、`ConsumableOptional.h`、`SingleTrigger.h` · 命名空间：`utils`

| 组件 | 用途 | 线程安全 |
| --- | --- | --- |
| `ThreadPoolExecutor` | 有界队列工作线程池 | 是（内部加锁） |
| `ElapsedTimer` | 经过多久：一个起点 | 否 |
| `DeadlineTimer` | 还剩多久 / 是否到期：起点 + 预算 | 否 |
| `Throttler` | 限制函数调用频率 | 否 |
| `ConsumableOptional` | 一次性消费的值，交接语义 | 可选（`thread_safe` 参数） |
| `SingleTrigger` | 原子的一次性触发标志 | 是（`atomic_bool`） |

## `ThreadPoolExecutor`

```cpp
class ThreadPoolExecutor final : public TaskExecutor {
public:
    explicit ThreadPoolExecutor( std::size_t threads = 0,
                                 std::size_t queue_capacity = 256 );

    bool Submit( std::function<void()> job ) override;

    std::size_t ThreadCount() const;
    std::size_t PendingCount() const;
    std::size_t ActiveCount() const;
};
```

### 构造参数

| 参数 | 默认 | 说明 |
| --- | --- | --- |
| `threads` | `0` | 工作线程数；`0` 表示 `max(1, hardware_concurrency())` |
| `queue_capacity` | `256` | 队列上限；`0` 表示无界（慎用） |

构造时立即创建并启动全部工作线程，Linux 下线程名为 `utils-pool-<i>`。

### `Submit`

```cpp
bool Submit( std::function<void()> job );
```

| 返回值 | 含义 |
| --- | --- |
| `true` | 已入队（空 `job` 视为空操作并受理） |
| `false` | 队列满或已停止（背压） |

`Submit` **只入队，不等待执行**，因此不会因慢回调阻塞调用者。队列满时立即返回 `false`，
由调用方（如 `TaskScheduler`）决定丢弃或重试。

### 状态查询

```cpp
std::size_t ThreadCount() const;    // 工作线程数
std::size_t PendingCount() const;   // 队列中等待的动作数
std::size_t ActiveCount() const;    // 正在执行的动作数
```

三个查询都会加锁；高频轮询会有竞争成本。

### 析构语义

- 置停止标志，唤醒全部工作线程；
- 工作线程**继续执行队列中剩余任务**（已受理的提交不丢）；
- 等待所有任务完成并 `join`；
- 因此析构可能**阻塞**：不可返回的任务会卡住析构。生产环境应保证回调可返回。

### 异常与线程纪律

- 动作抛出的异常被**吞掉**，不会杀死工作线程；
- 用户代码**绝不持锁执行**（先解锁再调用 `job()`）；
- 与 `TaskScheduler` 组合时：
  - 派发线程只入队，不被慢回调阻塞；
  - 队列满 → `Submit` 返回 `false` → 调度器计入 `reject_count`；
  - 执行器生命周期必须**长于**调度器（调度器不拥有执行器）；
  - `coalesce = false` 时同一任务可能并行执行，回调需线程安全。

### 示例

```cpp
#include "ThreadPoolExecutor.h"
#include <iostream>
using namespace utils;

ThreadPoolExecutor pool( 4, 1024 );
for ( int i = 0; i < 100; ++i ) {
    if ( !pool.Submit( [i]() { std::cout << i << '\n'; } ) ) {
        // 背压：队列已满
    }
}
// 析构时排空并 join
```

### 作为 `TaskExecutor` 注入

```cpp
ThreadPoolExecutor pool( 2 );
TaskScheduler      scheduler( pool );   // 回调异步执行
```

## 计时组件总览

两个组件覆盖全部五类问题，都按 `Clock` 模板化：

| 问题 | `ElapsedTimer<Clock>` | `DeadlineTimer<Clock>` |
| --- | --- | --- |
| 何时发生 | `StartedAt()`、`StartedAtUnixMillis()`、`StartedAtDateTime()` | `StartedAt()`、`ExpiresAt()`、`ExpiresAtUnixMillis()`、`ExpiresAtDateTime()` |
| 经过多久 | `Elapsed()`、`ElapsedAt(now)`、`ElapsedMillis()` … | 同左 |
| 还剩多久 | — | `Remaining()`、`RemainingAt(now)`、`RemainingMillis()` … |
| 是否到期 | — | `IsExpired()`、`IsExpiredAt(now)` |
| 是否超时 | — | `IsExpired()` |

- 默认 `Clock = std::chrono::steady_clock`，适合测耗时、超时、重试；不受系统改时 / NTP 影响。
- 需要墙钟时间戳或日历时刻时使用 `system_clock` 版本（`SystemElapsedTimer` / `SystemDeadlineTimer`）。
- 自定义时钟只需满足 `std::chrono` 的 Clock 形状（`now()` / `time_point`）；若其纪元是 Unix 纪元，特化 `ClockTraits` 后即可使用时间戳接口。
- 两者都是**值类型、可拷贝**，构造即开始，没有 `Start()` / `Stop()`；`Reset()` / `Extend()` 是写操作，跨线程共享需自行同步。

```cpp
#include "ElapsedTimer.h"
#include "DeadlineTimer.h"
using namespace utils;
using namespace std::chrono_literals;

SteadyDeadlineTimer timeout( 500ms );      // 起点 = 现在，500ms 后到期
if ( timeout.IsExpired() ) { /* ... */ }

SteadyElapsedTimer watch;                  // 起点 = 现在
// ... 工作 ...
auto ms = watch.ElapsedMillis();           // int64_t
```

## `ElapsedTimer<Clock>`：经过多久

```cpp
template <typename Clock = std::chrono::steady_clock>
class ElapsedTimer final {
public:
    using ClockType = Clock;
    using TimePoint = std::chrono::time_point<Clock, Nanoseconds>;

    ElapsedTimer() noexcept;                       // 起点 = Now()
    explicit ElapsedTimer( TimePoint start ) noexcept;

    void Reset() noexcept;                         // 起点 = Now()
    void Reset( TimePoint start ) noexcept;        // 起点 = 指定时间点

    TimePoint StartedAt() const noexcept;
    static TimePoint Now() noexcept;

    Nanoseconds Elapsed() const noexcept;
    Nanoseconds ElapsedAt( TimePoint now ) const noexcept;

    template <typename T = std::int64_t> T ElapsedMillis() const noexcept;
    template <typename T = std::int64_t> T ElapsedSeconds() const noexcept;
    template <typename Unit, typename T = std::int64_t> T ElapsedAs() const noexcept;

    // 仅 Unix 纪元时钟（system_clock）可用
    std::int64_t StartedAtUnixMillis() const;
    std::int64_t StartedAtUnixSeconds() const;
    DateTime StartedAtDateTime( const TimeZone &zone = TimeZone::Local() ) const;

    static std::int64_t ToUnixMillis( TimePoint tp );
    static std::int64_t ToUnixSeconds( TimePoint tp );
    static DateTime ToDateTime( TimePoint tp, const TimeZone &zone = TimeZone::Local() );
};
```

| 方法 | 语义 |
| --- | --- |
| 构造 | 以当前时刻为起点，立即开始 |
| `Reset()` | 起点改为当前时刻 |
| `Reset(start)` | 起点改为指定时间点（可回填历史起点） |
| `StartedAt()` | 取得起点时间点 |
| `Now()` | 当前时钟时间点（已对齐到纳秒） |
| `Elapsed()` / `ElapsedAt(now)` | 起点到当前 / 指定时刻的时长 |
| `ElapsedMillis()` / `ElapsedSeconds()` | 以毫秒 / 秒返回，默认 `int64_t` |
| `ElapsedAs<Unit, T>()` | 任意单位；整型截断，浮点保留小数 |
| `StartedAtUnixMillis()` / `StartedAtUnixSeconds()` | 起点的 Unix 时间戳，**向下取整** |
| `StartedAtDateTime()` | 起点对应的 `DateTime`（含时区 / 日历） |
| `ToUnixMillis()` / `ToUnixSeconds()` / `ToDateTime()` | 任意时间点的静态转换 |

```cpp
SteadyElapsedTimer watch;
// ... 工作 ...
auto ns = watch.Elapsed();                       // Nanoseconds
auto ms = watch.ElapsedMillis();                 // int64_t
auto s  = watch.ElapsedSeconds<double>();        // 带小数
auto us = watch.ElapsedAs<Microseconds>();       // int64_t

// 快照式：同一 now 上做多次判断，便于事件循环与测试
auto now = SteadyElapsedTimer::Now();
auto a   = watch.ElapsedAt( now );
```

> `ElapsedAs<Unit, T>` 的 `Unit` 必须是 `std::chrono::duration` 类型（如 `Milliseconds`、`Minutes`）；
> 返回类型为整型时截断，为浮点型时保留小数。

### 系统时钟版本

```cpp
SystemElapsedTimer wall;
// ...
std::int64_t ms = wall.StartedAtUnixMillis();          // 起点时间戳
DateTime     dt = wall.StartedAtDateTime();            // 本地时区的日历时刻
DateTime     utc = wall.StartedAtDateTime( TimeZone::Utc() );
```

- `SystemElapsedTimer` 的 `Elapsed()` 是墙钟差值，**NTP 校时 / 手动改时可能使其为负**；测耗时请用默认的 steady 版本。
- 非 Unix 纪元时钟调用 `StartedAtUnixMillis()` 等会在**编译期** `static_assert` 失败。

## `DeadlineTimer<Clock>`：还剩多久 / 是否到期

```cpp
template <typename Clock = std::chrono::steady_clock>
class DeadlineTimer final {
public:
    using ClockType = Clock;
    using TimePoint = std::chrono::time_point<Clock, Nanoseconds>;

    DeadlineTimer() = delete;
    explicit DeadlineTimer( std::chrono::duration<Rep, Period> budget ) noexcept;
    DeadlineTimer( TimePoint start, std::chrono::duration<Rep, Period> budget ) noexcept;

    static DeadlineTimer After( std::chrono::duration<Rep, Period> budget ) noexcept;
    static DeadlineTimer Until( TimePoint expires_at ) noexcept;

    void Reset() noexcept;                                     // 起点 = Now()，预算不变
    void Reset( std::chrono::duration<Rep, Period> budget ) noexcept;
    void Reset( TimePoint expires_at ) noexcept;
    void Reset( TimePoint start, std::chrono::duration<Rep, Period> budget ) noexcept;

    TimePoint StartedAt() const noexcept;
    TimePoint ExpiresAt() const noexcept;                      // start + budget
    static TimePoint Now() noexcept;
    Nanoseconds Budget() const noexcept;

    Nanoseconds Elapsed() const noexcept;
    Nanoseconds ElapsedAt( TimePoint now ) const noexcept;
    Nanoseconds Remaining() const noexcept;                    // 负数 = 已超期
    Nanoseconds RemainingAt( TimePoint now ) const noexcept;

    bool IsExpired() const noexcept;
    bool IsExpiredAt( TimePoint now ) const noexcept;

    template <typename T = std::int64_t> T ElapsedMillis() const noexcept;
    template <typename T = std::int64_t> T ElapsedSeconds() const noexcept;
    template <typename T = std::int64_t> T RemainingMillis() const noexcept;
    template <typename T = std::int64_t> T RemainingSeconds() const noexcept;
    template <typename Unit, typename T = std::int64_t> T ElapsedAs() const noexcept;
    template <typename Unit, typename T = std::int64_t> T RemainingAs() const noexcept;

    // 仅 Unix 纪元时钟（system_clock）可用
    std::int64_t ExpiresAtUnixMillis() const;
    std::int64_t ExpiresAtUnixSeconds() const;
    DateTime ExpiresAtDateTime( const TimeZone &zone = TimeZone::Local() ) const;

    double Progress() const noexcept;
    double ProgressAt( TimePoint now ) const noexcept;

    void Extend( std::chrono::duration<Rep, Period> delta ) noexcept;
    static DeadlineTimer Earliest( const DeadlineTimer &a, const DeadlineTimer &b ) noexcept;
};
```

### 构造

| 写法 | 语义 |
| --- | --- |
| `DeadlineTimer( budget )` | 起点 = 现在，`budget` 之后到期 |
| `DeadlineTimer( start, budget )` | 显式指定起点与预算（可回填历史） |
| `DeadlineTimer::After( budget )` | 等价于 `DeadlineTimer( budget )` |
| `DeadlineTimer::Until( expires_at )` | 起点 = 现在，在指定时间点到期 |

```cpp
SteadyDeadlineTimer a( 200ms );
SteadyDeadlineTimer b = SteadyDeadlineTimer::After( 200ms );
SteadyDeadlineTimer c = SteadyDeadlineTimer::Until( SteadyDeadlineTimer::Now() + 200ms );
```

> 不存在默认构造：没有预算的截止时间没有意义。

### `Reset` 的四种语义

| 调用 | 起点 | 预算 |
| --- | --- | --- |
| `Reset()` | 现在 | **不变**（完整重开：现在 + 原预算） |
| `Reset( budget )` | 现在 | 新预算 |
| `Reset( expires_at )` | 现在 | `expires_at - 现在`（可为负，即已到期） |
| `Reset( start, budget )` | 指定时间点 | 指定预算 |

```cpp
SteadyDeadlineTimer timer( 1s );
// ... 500ms 后 ...
timer.Reset();                 // 重新获得完整 1s，而不是剩余 500ms
timer.Reset( 2s );             // 换成 2s
timer.Reset( SteadyDeadlineTimer::Now() + 300ms );   // 截止点 = 现在 + 300ms
timer.Reset( SteadyDeadlineTimer::Now() - 1s, 2s );  // 起点在过去，已用 1s
```

### 查询

| 方法 | 语义 |
| --- | --- |
| `StartedAt()` / `ExpiresAt()` | 起点 / 截止点时间点 |
| `Budget()` | 预算（起点到截止点的名义时长） |
| `Elapsed()` / `ElapsedAt(now)` | 起点到当前 / 指定时刻的时长 |
| `Remaining()` / `RemainingAt(now)` | 距截止点还剩多久，**负数表示已超期** |
| `IsExpired()` / `IsExpiredAt(now)` | `now >= ExpiresAt()` |
| `ElapsedMillis()` / `ElapsedSeconds()` | 已用时长，默认 `int64_t` |
| `RemainingMillis()` / `RemainingSeconds()` | 剩余时长，默认 `int64_t`，可为负 |
| `ElapsedAs<Unit, T>()` / `RemainingAs<Unit, T>()` | 任意单位；整型截断，浮点保留小数 |
| `Progress()` / `ProgressAt(now)` | 已用比例，裁剪到 `[0, 1]`；预算非正时返回 `1.0` |
| `Extend(delta)` | 延长（正）或缩短（负）预算，起点不变 |
| `Earliest(a, b)` | 返回更早到期的那个，用于嵌套预算 |

```cpp
SteadyDeadlineTimer timeout( 500ms );
// ... 工作 ...
if ( timeout.IsExpired() ) {
    std::cout << "超期 " << timeout.RemainingMillis() * -1 << " ms\n";
}
else {
    std::cout << "还剩 " << timeout.RemainingMillis() << " ms\n";
}

auto now = SteadyDeadlineTimer::Now();
if ( !timeout.IsExpiredAt( now ) ) {
    auto left = timeout.RemainingAt( now );
    auto pct  = timeout.ProgressAt( now );      // 0.0 ~ 1.0
}
```

- **恰好等于截止点算到期**（`>=`），与 `wait_until` 的边界一致。
- `Remaining()` 不做裁剪：负数即超期量；需要“超期多久”直接取反即可。
- 预算在构造 / `Reset` 时用 `ceil` 向上取整到纳秒，`0.5ns` 会变成 `1ns`，不会“构造即到期”；`0` 预算则立即到期。
- `Extend()` 传入负值可缩短预算，缩短到负数即视为已到期。
- `Progress()` 中 `elapsed <= 0`（墙钟回拨）返回 `0.0`，不会出现负进度。

### 系统时钟版本

```cpp
SystemDeadlineTimer deadline( 5s );
std::int64_t expires_ms = deadline.ExpiresAtUnixMillis();
DateTime     expires_at = deadline.ExpiresAtDateTime();
std::cout << deadline.ExpiresAtDateTime().ToIso8601() << '\n';
```

- `SystemDeadlineTimer` 适合“在某个墙钟时刻到期”的语义；但它同样受 NTP / 手动改时影响。要求“经过固定时长必定到期”时用默认的 steady 版本。
- 非 Unix 纪元时钟调用 `ExpiresAtUnixMillis()` 等会在**编译期** `static_assert` 失败。

## 确定性测试

测试“经过多久 / 还剩多久 / 是否到期”**不需要假时钟，也不需要 `sleep`**：所有查询都有接收显式 `TimePoint` 的快照版本，把“当前时刻”当作参数传入即可。

| 方法 | 含义 |
| --- | --- |
| `ElapsedAt(now)` | 从起点到 `now` 经过多久 |
| `RemainingAt(now)` | 从 `now` 到截止点还剩多久，负值表示已超期 |
| `IsExpiredAt(now)` | `now` 是否已到/越过截止点 |
| `ProgressAt(now)` | `now` 处的进度，裁剪到 `[0, 1]` |

```cpp
using Clock = std::chrono::steady_clock;
using TP    = SteadyElapsedTimer::TimePoint;   // == std::chrono::time_point<Clock, Nanoseconds>

const TP start = SteadyElapsedTimer::Now();
SteadyElapsedTimer watch( start );
SteadyDeadlineTimer timeout( start, 100ms );

assert( watch.ElapsedAt( start + 40ms ) == 40ms );
assert( timeout.RemainingAt( start + 40ms ) == 60ms );
assert( !timeout.IsExpiredAt( start + 40ms ) );

assert( timeout.IsExpiredAt( start + 100ms ) );           // 恰好到期即算到期
assert( timeout.RemainingAt( start + 100ms ) == Nanoseconds::zero() );
assert( timeout.ProgressAt( start + 150ms ) == 1.0 );
```

`SystemElapsedTimer` / `SystemDeadlineTimer` 用法完全相同，只是 `TimePoint` 基于 `system_clock`（即 `utils::Timestamp` 的同构类型）：

```cpp
using SystemTP = SystemElapsedTimer::TimePoint;   // std::chrono::time_point<system_clock, Nanoseconds>
const SystemTP start = SystemElapsedTimer::Now();
SystemDeadlineTimer deadline( start, 2s );
assert( !deadline.IsExpiredAt( start + 1s ) );
assert( deadline.RemainingAt( start + 1s ) == 1s );
```

`Elapsed()` / `Remaining()` / `IsExpired()` / `Progress()` 以及 `ElapsedMillis()` 等便捷接口内部调用 `Now()`，适合运行时使用；测试中若要断言精确值，请用上表的 `...At(now)` 版本。

> 假时钟属于**测试实现细节，不是公共 API**：仓库的 `test/TestClock.h` 仅用于验证那些内部取 `Now()` 的薄封装（默认构造、`Reset()`、`After()`、`Extend()` 等），不会被安装，不应在业务代码中使用。

### 自定义时钟

```cpp
struct MyClock {
    using rep        = std::int64_t;
    using period     = std::nano;
    using duration   = Nanoseconds;
    using time_point = std::chrono::time_point<MyClock, Nanoseconds>;
    static constexpr bool is_steady = true;
    static time_point now() noexcept;
};

using MyElapsed = ElapsedTimer<MyClock>;
```

若 `MyClock` 的纪元就是 Unix 纪元，特化 `ClockTraits` 即可解锁时间戳接口：

```cpp
template <>
struct utils::ClockTraits<MyClock> {
    static constexpr bool unix_epoch = true;
};
```

<a id="throttler"></a>
## `Throttler`：频率限制

```cpp
template <typename Rep, typename Period>
class Throttler final {
public:
    using IntervalType = std::chrono::duration<Rep, Period>;

    explicit Throttler( const IntervalType &interval );

    template <typename Func, typename... Args>
    auto Run( Func &&func, Args &&...args );
};
```

- 基于 `steady_clock`；
- 距上次**实际执行**超过 `interval` 才执行；首次调用总是执行（`last_time_` 初始为纪元）；
- 非线程安全；
- 不可拷贝/移动。

返回值：

| `Func` 返回类型 | `Run` 返回类型 |
| --- | --- |
| `void` | `bool`（本次是否执行） |
| 非 `void` | `std::pair<bool, std::optional<R>>` |

```cpp
Throttler<std::chrono::milliseconds::rep, std::chrono::milliseconds::period>
    throttler( std::chrono::milliseconds( 100 ) );

throttler.Run( []() { std::cout << "tick\n"; } );      // 第一次：true

auto [ran, value] = throttler.Run( []() { return 42; } );
if ( ran ) {
    std::cout << *value << '\n';
}
```

> 更简洁的写法：`Throttler<std::int64_t, std::milli> throttler( 100ms );`。

## `ConsumableOptional`

```cpp
template <typename T>
class ConsumableOptional {
public:
    ConsumableOptional( bool thread_safe = false );
    ConsumableOptional( const T &value, bool thread_safe = false );
    template <typename U>
    ConsumableOptional( U &&value, bool thread_safe = false );

    void set( const T &value );
    template <typename U> void set( U &&value );

    std::optional<T> consume();
    void reset();
    bool has_value() const;
    explicit operator bool() const;
};
```

“交接”语义：值只能被取走一次。

| 方法 | 语义 |
| --- | --- |
| `set(...)` | 覆盖当前值 |
| `consume()` | 有值时**移动**出来并清空；否则返回 `nullopt` |
| `reset()` | 清空 |
| `has_value()` | 当前是否有值 |

- `thread_safe = true` 时所有操作加内部 `std::mutex`；
- `thread_safe = false` 时零开销，但多线程读写需外部同步；
- 不可拷贝/移动（避免交接语义被意外复制）。

```cpp
ConsumableOptional<std::string> mailbox( true );

// 生产者
mailbox.set( "result" );

// 消费者
if ( auto value = mailbox.consume() ) {
    std::cout << *value << '\n';   // "result"
}
// 再次 consume() 返回 nullopt
```

## `SingleTrigger`

```cpp
class SingleTrigger {
public:
    using Ptr  = std::shared_ptr<SingleTrigger>;
    using Weak = std::weak_ptr<SingleTrigger>;

    explicit SingleTrigger( bool trigger );

    void Trigger();
    bool Consume();

    template <typename Func, typename... Args>
    void ExecuteIfTriggered( Func &&func, Args &&...args );

    static Ptr  Create( bool trigger = false );
    static void Trigger( Ptr &ptr );
    static void Trigger( Weak &weak );
};
```

| 方法 | 语义 |
| --- | --- |
| `Trigger()` | 置位（幂等） |
| `Consume()` | 原子地读取并清零；返回之前是否已置位 |
| `ExecuteIfTriggered(f, args...)` | 原子 CAS：只有成功从 `true` 切换到 `false` 的线程执行一次 `f` |
| `Create(bool)` | 工厂 |
| `Trigger(Ptr&)` / `Trigger(Weak&)` | 空指针安全地置位；`Weak` 会先 `lock()` |

```cpp
auto flag = SingleTrigger::Create();

flag->ExecuteIfTriggered( []() { std::cout << "run once\n"; } );  // 不执行
flag->Trigger();
flag->ExecuteIfTriggered( []() { std::cout << "run once\n"; } );  // 执行
flag->ExecuteIfTriggered( []() { std::cout << "run once\n"; } );  // 不再执行
```

> 常见用途：跨线程“只执行一次”的收尾、延迟初始化、事件去重。

## 线程安全总览

| 类型 | 线程安全 |
| --- | --- |
| `ThreadPoolExecutor` | 是（`Submit` 与查询可并发） |
| `ElapsedTimer` | 否 |
| `DeadlineTimer` | 否 |
| `Throttler` | 否 |
| `ConsumableOptional` | 取决于构造参数 `thread_safe` |
| `SingleTrigger` | 是（原子操作） |

## 行为边界速查

| 场景 | 行为 |
| --- | --- |
| `ThreadPoolExecutor(0)` | 线程数取 `max(1, hardware_concurrency())` |
| `ThreadPoolExecutor` 队列满 | `Submit` 返回 `false` |
| `ThreadPoolExecutor` 空 job | 受理并返回 `true` |
| `ThreadPoolExecutor` 析构 | 排空队列后 join，可能阻塞 |
| `ElapsedTimer` / `DeadlineTimer` 默认时钟 | `steady_clock`，不受系统改时影响 |
| `DeadlineTimer::Remaining()` | 有符号；负数表示已超期 |
| `DeadlineTimer` 恰好到期 | `now == expires_at` 即算到期（`>=`） |
| `DeadlineTimer` 预算取整 | `ceil` 到纳秒；`0.5ns` → `1ns`，`0` 立即到期 |
| `SystemElapsedTimer::Elapsed()` | 受 NTP / 改时影响，可能为负 |
| `Throttler` 首次调用 | 总是执行 |
| `ConsumableOptional::consume()` 第二次 | `nullopt` |
| `SingleTrigger::ExecuteIfTriggered` 并发 | 只有一个线程执行 |
