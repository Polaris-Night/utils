# 并发与计时

头文件：`ThreadPoolExecutor.h`、`TaskExecutor.h`、`Timer.h`、`Throttler.h`、`ConsumableOptional.h`、`SingleTrigger.h` · 命名空间：`utils`

| 组件 | 用途 | 线程安全 |
| --- | --- | --- |
| `ThreadPoolExecutor` | 有界队列工作线程池 | 是（内部加锁） |
| `Timer` / `ElapsedTimer` | 超时判定 / 累计计时 | 否 |
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

## `Timer`：一次性超时判定

```cpp
class Timer {
public:
    Timer();
    Timer( int msec );
    Timer( Milliseconds msec );

    void Start();
    void Stop();
    void Restart();
    void SetInterval( int msec );
    void SetInterval( Milliseconds msec );

    bool IsRunning() const;
    bool IsTimeout() const;
};
```

| 方法 | 语义 |
| --- | --- |
| `Start()` | 启动；已在运行则无效 |
| `Stop()` | 停止；停止后 `IsTimeout()` 恒为 `false` |
| `Restart()` | 重置起点并启动（未运行也会启动） |
| `SetInterval()` | 修改间隔；不影响当前起点 |
| `IsTimeout()` | 运行中且 `elapsed >= interval` 时返回 `true` |

- 基于 `steady_clock`，不受系统改时影响；
- 超时后保持 `true`，直到 `Stop()` / `Restart()`；
- 默认间隔为 `0ms`，即 `Start()` 后立即超时；
- 非线程安全。

```cpp
Timer timer( 500 );          // 500ms
timer.Start();

while ( !timer.IsTimeout() ) {
    // 忙等仅作示例；实际应配合事件循环
}
timer.Restart();
```

## `ElapsedTimer`：累计计时

```cpp
class ElapsedTimer {
public:
    void Start();
    void Stop( bool reset_time = true );
    void Pause();
    void Resume();
    void Restart();

    template <typename TimeUnit = Seconds, typename ReturnType = std::int64_t>
    ReturnType GetElapsed() const;
};
```

| 方法 | 语义 |
| --- | --- |
| `Start()` | 启动；已启动（含暂停中）再次调用无效；Stop 后再 Start 从累计值继续 |
| `Stop(reset_time = true)` | 停止并冻结累计值；`reset_time = true` 时清零 |
| `Pause()` | 暂停，累计到当前时刻；未启动/已暂停无效 |
| `Resume()` | 从暂停恢复；未启动/运行中无效 |
| `Restart()` | 清零并重新启动 |
| `GetElapsed<Unit, T>()` | 返回累计时长；未启动返回 0 |

`TimeUnit` 支持 `Days`、`Hours`、`Minutes`、`Seconds`、`Milliseconds`、`Microseconds`、`Nanoseconds`；
`ReturnType` 为整型时截断，为浮点型时保留小数。

```cpp
ElapsedTimer timer;
timer.Start();
// ... 工作 ...
timer.Pause();

auto ms = timer.GetElapsed<Milliseconds>();          // int64_t
auto s  = timer.GetElapsed<Seconds, double>();       // 带小数

timer.Resume();
timer.Stop();                                        // 冻结累计值
```

> `Stop()` 默认 `reset_time = true`，会清零；若要保留累计值请传 `false`。

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
| `Timer` / `ElapsedTimer` | 否 |
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
| `Timer` 默认间隔 | `0ms`，启动后立即超时 |
| `Timer::Stop()` 后 | `IsTimeout()` 恒为 `false` |
| `ElapsedTimer::Stop()` 默认 | 清零累计值；传 `false` 保留 |
| `Throttler` 首次调用 | 总是执行 |
| `ConsumableOptional::consume()` 第二次 | `nullopt` |
| `SingleTrigger::ExecuteIfTriggered` 并发 | 只有一个线程执行 |
