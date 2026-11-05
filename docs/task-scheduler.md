# 任务调度

头文件：`TaskScheduler.h`、`TaskExecutor.h`、`ThreadPoolExecutor.h` · 命名空间：`utils`

`TaskScheduler` 是一个**时间驱动的任务调度器**，与数据驱动的窗口、物理计时的 `Timer` 互补。
它把“何时触发”（触发器）与“如何执行”（执行器）解耦，核心是纯确定性的 `Poll`：

- **无内部时钟读取、无 sleep**：时间由调用者传入，单元测试可精确控制；
- **双时间轴**：steady（物理时长，免疫改时）与 wall（日历真相，时区感知）；
- **可选线程适配器**：`Start` / `Stop` 只是驱动方式之一，与手动 `Poll` 二选一。

## 快速示例

```cpp
#include "TaskScheduler.h"
#include "ThreadPoolExecutor.h"
#include <iostream>
using namespace utils;

ThreadPoolExecutor pool( 2 );       // 回调异步执行
TaskScheduler      scheduler( pool );

// 每 100ms 触发一次（steady 轴，固定网格不漂移）
auto id = scheduler.Schedule(
    std::make_shared<IntervalTrigger>( std::chrono::milliseconds( 100 ) ),
    []( const DateTime &scheduled ) {
        std::cout << "scheduled at " << scheduled.ToIso8601() << '\n';
    } );

// 每天 09:30 触发（wall 轴，本地时区）
scheduler.Schedule(
    std::make_shared<CalendarTrigger>( CalendarTrigger::Daily( 9, 30 ) ),
    []( const DateTime & ) { std::cout << "daily\n"; } );

scheduler.Start();                  // 启动线程适配器
// ... 业务代码 ...
scheduler.Stop();                   // 请求停止并等待派发线程退出
```

### 确定性测试：手动推进

```cpp
TaskScheduler scheduler;            // 默认 InlineExecutor（同步执行）

const auto t0 = DateTime::FromIso8601( "2026-11-06T00:00:00Z" )->ToTimestamp();
const auto s0 = std::chrono::steady_clock::time_point{};

scheduler.Poll( t0, s0 );           // 先设定时间轴起点
auto id = scheduler.Schedule(
    std::make_shared<IntervalTrigger>( std::chrono::seconds( 10 ), s0 ),
    []( const DateTime & ) { /* ... */ } );

scheduler.Poll( t0 + std::chrono::seconds( 10 ), s0 + std::chrono::seconds( 10 ) );  // 触发 1 次
```

> `Schedule` 基于调度器**当前的时间轴**计算首次触发。构造后时间轴是真实时钟；
> 确定性场景请**先 `Poll` 一次**设定起点，再注册任务。

## 时间语义：双时间轴

| 轴 | 触发器 | 语义 | 适用 |
| --- | --- | --- | --- |
| steady | `IntervalTrigger` | 物理时长，基于 `steady_clock`，不受系统改时影响 | 心跳、采样、超时 |
| wall | `AtTrigger`、`CalendarTrigger`、`FunctionalWallTrigger` | 日历时间，按 `DateTime` + 时区计算 | 每日报表、会议提醒 |

steady 任务的回调参数是**锚点换算出的逻辑墙钟**：注册时记录 `(wall_anchor, steady_anchor)`，
之后每次触发用 `wall_anchor + (due - steady_anchor)` 得到稳定的 `scheduled`，适合作为幂等键。

## 触发器

所有触发器都是纯函数：`NextAfter(t)` 返回**严格晚于 `t`** 的下一次触发时刻，`nullopt` 表示不再触发。

### `WallTrigger` / `SteadyTrigger`

```cpp
class WallTrigger {
public:
    virtual std::optional<DateTime> NextAfter( const DateTime &after ) const = 0;
};

class SteadyTrigger {
public:
    using SteadyTime = std::chrono::steady_clock::time_point;
    virtual std::optional<SteadyTime> NextAfter( const SteadyTime &after ) const = 0;
};
```

自定义触发器实现 `NextAfter` 即可，无需继承调度器。

### `AtTrigger`

```cpp
explicit AtTrigger( DateTime at );
```

在指定绝对时刻触发一次。`at` 不晚于当前时间时，`Schedule` 返回 `nullopt`。

```cpp
scheduler.Schedule(
    std::make_shared<AtTrigger>(
        DateTime::FromIso8601( "2026-12-31T23:59:59+08:00" ).value() ),
    []( const DateTime &at ) { std::cout << "new year\n"; } );
```

> 一次性任务触发后即从堆中移除，**不会**因墙钟回拨而复活。

### `IntervalTrigger`

```cpp
explicit IntervalTrigger( Nanoseconds period,
                          SteadyTime start = std::chrono::steady_clock::now() );
```

固定网格（Fixed-Rate），**不漂移**：第 k 次触发时刻恒为 `start + k * period`，与回调耗时无关。
`period` 必须大于 0，否则抛 `std::invalid_argument`。

```cpp
auto start = std::chrono::steady_clock::now();
auto trigger = std::make_shared<IntervalTrigger>( std::chrono::milliseconds( 16 ), start );
// start, start+16ms, start+32ms, ...
```

### `CalendarTrigger`

```cpp
static CalendarTrigger Daily  ( int hour, int minute, int second = 0,
                                TimeZone zone = TimeZone::Local() );
static CalendarTrigger Weekly ( Weekday weekday, int hour, int minute, int second = 0,
                                TimeZone zone = TimeZone::Local() );
static CalendarTrigger Monthly( int day, int hour, int minute, int second = 0,
                                TimeZone zone = TimeZone::Local() );
```

| 工厂 | 参数 | 示例 |
| --- | --- | --- |
| `Daily` | `hour` 0–23，`minute` 0–59，`second` 0–59 | 每天 09:30 |
| `Weekly` | `Weekday`（`Sunday=0` … `Saturday=6`） | 每周一 08:00 |
| `Monthly` | `day` 1–31 | 每月 1 日 00:00 |

DST 策略（固定行为）：

| 情况 | 行为 |
| --- | --- |
| 春季跳变导致目标时刻不存在（如 02:30 被跳过） | **跳过该次**，等下一周期 |
| 秋季回拨导致目标时刻出现两次 | 只在**较早**一次触发 |
| 每月 `day` 日遇到无该日的月份（如 31 日遇 2 月） | **跳过该月** |

非法参数（`hour` 越界、`day` 为 0 等）在构造时抛 `std::invalid_argument`。

### 自定义触发器

```cpp
class FunctionalWallTrigger final : public WallTrigger {
public:
    using Fn = std::function<std::optional<DateTime>( const DateTime & )>;
    explicit FunctionalWallTrigger( Fn fn );
};

class FunctionalSteadyTrigger final : public SteadyTrigger {
public:
    using Fn = std::function<std::optional<SteadyTime>( const SteadyTime & )>;
    explicit FunctionalSteadyTrigger( Fn fn );
};
```

用于适配 cron 库、业务日历或任意规则。`fn` 为空时视为“不再触发”。

```cpp
auto trigger = std::make_shared<FunctionalWallTrigger>(
    []( const DateTime &after ) -> std::optional<DateTime> {
        // 下一个工作日 09:00
        DateTime next = after.Add<Days>( 1 ).StartOf<Days>().Add<Hours>( 9 );
        while ( next.GetWeekday() == Saturday || next.GetWeekday() == Sunday ) {
            next = next.Add<Days>( 1 );
        }
        return next;
    } );
```

## 任务策略：`TaskPolicy`

```cpp
enum class LatePolicy {
    CatchUp,   // 迟到也补跑一次（默认）
    Skip       // 迟到超过 grace 则跳过
};

struct TaskPolicy {
    LatePolicy  late{ LatePolicy::CatchUp };
    Milliseconds grace{ 5000 };
    bool        coalesce{ true };
    Nanoseconds jitter{ 0 };
};
```

| 字段 | 默认 | 说明 |
| --- | --- | --- |
| `late` | `CatchUp` | 超过 `grace` 时：补跑一次 / 跳过 |
| `grace` | 5000ms | 迟到容忍窗口；不超过该窗口一律正常执行 |
| `coalesce` | `true` | 上次未执行完时跳过本次（max_instances = 1） |
| `jitter` | 0 | **首次触发**的相位抖动上界（均匀随机 0..jitter），用于防惊群 |

### 迟到判定

```text
late_by = now - scheduled
迟到    : late_by > 0           → late_count++
太迟    : late_by > grace       → 若 late == Skip，则 skip_count++ 并跳过
```

- 一次 `Poll` 对每个任务**最多触发一次**；中间错过的网格点整体丢弃（不补多次）；
- `CatchUp` 只是“补跑一次”，不是“补跑全部错过次数”；
- 调度器按**计划时刻**递推下一次（固定网格），因此不会因迟到而累积漂移。

### coalesce

`coalesce = true`（默认）时，如果上一次提交的动作尚未执行完（执行器尚未回调完成），
本次触发计入 `skip_count` 并跳过，保证**同一任务最多一个在途实例**。

若业务允许并行，设 `coalesce = false`，此时回调必须线程安全。

### jitter

仅作用于**首次触发**：`Schedule` 时给第一次触发时刻加一个 `[0, jitter]` 的随机偏移，
后续触发仍按触发器网格。适合大量客户端同时启动时打散。

## `TaskScheduler`

```cpp
class TaskScheduler {
public:
    using Action     = std::function<void( const DateTime &scheduled )>;
    using SteadyTime = SteadyTrigger::SteadyTime;
    using TaskId     = std::uint64_t;

    struct TaskStats {
        std::uint64_t fire_count{ 0 };
        std::uint64_t late_count{ 0 };
        std::uint64_t skip_count{ 0 };
        std::uint64_t reject_count{ 0 };
    };

    TaskScheduler();
    explicit TaskScheduler( TaskExecutor &executor );
    ~TaskScheduler();

    std::optional<TaskId> Schedule( std::shared_ptr<WallTrigger> trigger,
                                    Action action,
                                    TaskPolicy policy = {} );
    std::optional<TaskId> Schedule( std::shared_ptr<SteadyTrigger> trigger,
                                    Action action,
                                    TaskPolicy policy = {} );

    bool Cancel( TaskId id );
    std::optional<DateTime> NextFireTime( TaskId id ) const;
    std::size_t TaskCount() const;
    std::optional<TaskStats> GetTaskStats( TaskId id ) const;

    std::size_t Poll( const Timestamp &wall_now, const SteadyTime &steady_now );

    void Start();
    bool Stop( Milliseconds timeout = std::chrono::seconds( 5 ) );
    bool IsRunning() const;
    void SetSyncedWallClock( bool enabled );
};
```

### 构造与执行器

| 构造 | 执行方式 |
| --- | --- |
| `TaskScheduler()` | 使用内部 `InlineExecutor`，回调在 `Poll` 调用线程**同步**执行 |
| `TaskScheduler(executor)` | 提交到注入的 `TaskExecutor`（如 `ThreadPoolExecutor`） |

执行器必须**长于调度器**生命周期。调度器只持有引用，不拥有执行器。

### `Schedule`

```cpp
std::optional<TaskId> Schedule( std::shared_ptr<WallTrigger> trigger, Action action, TaskPolicy policy = {} );
std::optional<TaskId> Schedule( std::shared_ptr<SteadyTrigger> trigger, Action action, TaskPolicy policy = {} );
```

返回任务 id；以下情况返回 `nullopt`：

- `trigger` 或 `action` 为空；
- 一次性任务的目标时刻已过（wall）；
- 触发器没有下一次触发（steady）。

重复型任务（`IntervalTrigger` / `CalendarTrigger` / `Functional*`）**注册一次即自动重复**，
无需在回调里重新注册。

### <a id="poll"></a>`Poll`

```cpp
std::size_t Poll( const Timestamp &wall_now, const SteadyTime &steady_now );
```

- 处理两条时间轴上所有 `due <= now` 的触发；
- 返回**本次成功提交给执行器的动作数量**（不含被跳过、被拒绝的）；
- 内部不读时钟、不睡眠；
- 每条时间轴独立推进，互不影响。

```cpp
// 游戏主循环
void Frame( const DateTime &now, std::chrono::steady_clock::time_point steady ) {
    scheduler.Poll( now.ToTimestamp(), steady );
}
```

### `Cancel`

```cpp
bool Cancel( TaskId id );
```

- 阻止后续触发；**已在执行中的动作不会中断**；
- 任务记录保留，`GetTaskStats` 仍可查询；
- 返回 `true` 表示成功取消了“有计划待触发”的任务；
  任务不存在或已无计划返回 `false`。

### `NextFireTime`

```cpp
std::optional<DateTime> NextFireTime( TaskId id ) const;
```

返回下次触发的墙钟时刻；steady 任务返回按锚点换算的逻辑墙钟。无计划返回 `nullopt`。

### `TaskCount`

```cpp
std::size_t TaskCount() const;
```

当前**有计划待触发**的任务数；一次性任务触发后、被 `Cancel` 后都会减少。

### `GetTaskStats`

```cpp
std::optional<TaskStats> GetTaskStats( TaskId id ) const;
```

| 字段 | 含义 |
| --- | --- |
| `fire_count` | 已成功提交执行次数 |
| `late_count` | 触发时已晚于计划时刻的次数 |
| `skip_count` | 跳过次数（迟到超窗口 / coalesce 占用 / 执行器拒绝？见下） |
| `reject_count` | 执行器 `Submit` 返回 `false` 的次数 |

> `skip_count` 统计 **迟到超窗口** 与 **coalesce 占用**；执行器拒绝单独计入 `reject_count`，
> 不计入 `skip_count`。任务不存在返回 `nullopt`。

## 线程适配器

```cpp
void Start();
bool Stop( Milliseconds timeout = std::chrono::seconds( 5 ) );
bool IsRunning() const;
void SetSyncedWallClock( bool enabled );
```

### `Start` / `Stop`

- `Start()` 幂等；已在运行则直接返回；
- 内部单 worker 只负责**派发**：按两条时间轴最近的 deadline 等待，`Schedule` / `Cancel` / `Stop`
  会唤醒并重算等待；到点调用 `Poll`；
- `Stop()` 请求停止并等待 worker 退出：
  - 返回 `true`：已完全停止；
  - 返回 `false`：超时，worker 仍在运行（通常是同步长任务未返回）。此时**必须保活调度器对象**，
    稍后重试 `Stop`；
- 析构会调用 `Stop(Milliseconds::max())`，长时间不返回的同步任务会**阻塞析构**；
  生产环境建议注入异步执行器。

### `SetSyncedWallClock`

```cpp
void SetSyncedWallClock( bool enabled );
```

| 值 | 墙钟来源 |
| --- | --- |
| `false`（默认） | `DateTime::NowTimestamp()`（系统时间） |
| `true` | `DateTime::NowSyncedTimestamp()`（单调时钟 + 时间种子，抗客户端改时） |

只影响线程适配器读取的墙钟；手动 `Poll` 的时间完全由调用者决定。

### 驱动源互斥

**同一时段只使用一种驱动方式**：

| 场景 | 驱动方式 |
| --- | --- |
| 生产服务 | `Start()` / `Stop()` |
| 游戏主循环 / 事件循环 | 手动 `Poll(wall, steady)` |
| 单元测试 / 回放 | 手动 `Poll` + 伪时间 |

不要同时 `Start()` 又手动 `Poll`：两条驱动会竞争同一个任务堆，导致重复触发或时序不可预期。

## 执行器契约

```cpp
class TaskExecutor {
public:
    virtual bool Submit( std::function<void()> job ) = 0;
};
```

| 返回值 | 含义 |
| --- | --- |
| `true` | 已受理（可能同步或异步执行） |
| `false` | 拒绝（背压）；调度器计入 `reject_count`，不重试 |

内置实现：

| 实现 | 行为 |
| --- | --- |
| `InlineExecutor` | `Submit` 内同步执行，空 job 安全，总是返回 `true` |
| `ThreadPoolExecutor` | 入队后由工作线程执行；有界队列满时返回 `false` |

```cpp
ThreadPoolExecutor pool( /* threads = */ 4, /* queue_capacity = */ 1024 );
TaskScheduler      scheduler( pool );
```

### 回调纪律

- 签名固定为 `void(const DateTime &scheduled)`；
- 回调在**无锁上下文**执行（锁内只做堆操作与状态更新）；
- 回调抛出的异常被**吞掉**，不会拖垮调度器；
- `scheduled` 是**计划时刻**，不是实际执行时刻；可用作幂等键。

## 语义与边界

| 场景 | 行为 |
| --- | --- |
| 一次 `Poll` 跨过多个周期 | 每个任务最多触发 **1 次**，中间点丢弃 |
| 墙钟回拨 | `AtTrigger` 触发后不复活；`CalendarTrigger` 按新时间继续 |
| steady 时间轴回拨 | 不会发生（`steady_clock` 单调） |
| 回调耗时超过周期 | `coalesce=true` 时跳过后续触发；`false` 时可并行 |
| 执行器拒绝 | `reject_count++`，本次动作丢弃，不重试 |
| `Cancel` 在途任务 | 不中断，仅阻止后续 |
| 任务触发完毕 | 从堆移除，`TaskCount` 减少；统计仍保留 |
| 线程适配器超时停止 | `Stop` 返回 `false`，对象必须保活后重试 |
| 回调抛出异常 | 吞掉，调度器继续运行 |
| 同时 `Start` 与手动 `Poll` | **不支持**，行为不可预期 |

## 与窗口组合

调度器与窗口族**零依赖**，可在同一主循环里各司其职：

```cpp
struct Sample { int value; std::int64_t ts; };

WindowPolicy<Sample, std::int64_t> policy;      // 每 60 秒一个时间桶
policy.span   = 60;
policy.key_fn = []( const Sample &s ) { return s.ts; };

TumblingWindow<Sample, std::int64_t> window( policy );
window.OnClose( []( std::int64_t bucket_start,
                    const std::vector<Sample> &items ) {
    // 一个时间桶收尾：输出聚合结果
} );

CountWindow<int> recent( 100 );                  // 最近 100 条

scheduler.Poll( now.ToTimestamp(), steady );     // 推进定时任务
window.Poll( now.ToUnixSeconds() );              // 推进滚动窗口
recent.Add( value );                             // 数据驱动窗口

window.Add( Sample{ value, now.ToUnixSeconds() } );
```

> 调度器负责“何时做”，窗口负责“对哪些数据做”；两者只共享 `Poll` 这一动词约定，
> 不互相包含头文件。
