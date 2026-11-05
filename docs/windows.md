# 窗口

头文件：`WindowPolicy.h`、`SlidingWindow.h`、`TumblingWindow.h` · 命名空间：`utils`

窗口族用同一套**规则词汇**（`WindowPolicy`）描述“按什么维度切分/淘汰数据”，再按解释方式分成两族：

| 族 | 类型 | 规则解释 | 典型用途 |
| --- | --- | --- | --- |
| 滑动 | `RuleWindow` 及其特例 | 条件**越界** ⇒ 从最旧一端淘汰 | 最近 N 条、最近 5 秒、总权重不超过 W |
| 滚动 | `TumblingWindow` | 条件**命中** ⇒ 收尾输出并清空 | 每 60 秒一批、每 100 条一批 |
| 会话 | `SessionWindow` | 空闲超过间隙 ⇒ 结束会话 | 用户行为会话、设备在线段 |
| 条件 | `PredicateWindow` | 条件被打破 ⇒ 清空连续序列 | 连续失败、连续上升 |

两族**互不依赖**：`TumblingWindow` 只包含 `WindowPolicy.h`，`SlidingWindow.h` 也只包含 `WindowPolicy.h`。
它们共享三个驱动动词：

| 入口 | 含义 |
| --- | --- |
| `Add(item)` | 数据驱动：喂一条数据 |
| `Poll(now)` | 缺席驱动：没有新数据时推进时间，返回本次处理的单位数 |
| `Close()` | 显式收尾（滚动/会话窗口） |
| `Clear()` | 丢弃数据（不产生“完成”语义） |

## 快速示例

```cpp
#include "SlidingWindow.h"
#include "TumblingWindow.h"
using namespace utils;

// 最近 100 条
CountWindow<int> recent( 100 );
recent.Add( 42 );

// 最近 5 秒（key 为毫秒时间戳）
struct Event { int id; std::int64_t ts; };
TimeWindow<Event, std::int64_t> last_5s(
    5000, []( const Event &e ) { return e.ts; } );
last_5s.Add( Event{ 1, 1000 } );
last_5s.Poll( 10000 );                  // 空闲时也能淘汰过期数据

// 每 60 秒一批
WindowPolicy<Event, std::int64_t> policy;
policy.span   = 60;
policy.key_fn = []( const Event &e ) { return e.ts; };

TumblingWindow<Event, std::int64_t> batches( policy );
batches.OnClose( []( std::int64_t bucket_start, const std::vector<Event> &items ) {
    // 一个时间桶结束
} );
batches.Add( Event{ 1, 0 } );
batches.Poll( 60 );                     // 收尾 [0,60)
```

## `WindowPolicy`

```cpp
template <typename T, typename TimeT>
struct WindowPolicy {
    using Duration = decltype( std::declval<TimeT>() - std::declval<TimeT>() );
    using KeyFn      = std::function<TimeT( const T & )>;
    using WeightFn   = std::function<double( const T & )>;
    using PredicateFn = std::function<bool( const T & )>;
    using CustomFn   = std::function<bool( const T &, TimeT )>;

    std::optional<Duration> span;          // 时间跨度 / 桶宽
    WindowAlign             align{ WindowAlign::Epoch };
    TimeT                   origin{};      // align == Explicit 时的对齐原点
    std::optional<std::size_t> count;      // 条数（0 = 即进即出）
    std::optional<double>   weight;        // 聚合权重上限 / 收尾阈值
    std::optional<Duration> session_gap;   // 会话间隙
    PredicateFn             predicate;     // 单条谓词
    CustomFn                custom;        // 自定义条件（item, key）
    RuleCombine             combine{ RuleCombine::Any };
    KeyFn                   key_fn;        // 从元素提取时间点
    WeightFn                weight_fn;     // 从元素提取权重（默认 1.0）
    bool                    emit_empty{ false };
    std::size_t             max_catchup{ 1024 };

    TimeT  KeyOf( const T &item ) const;
    double WeightOf( const T &item ) const;
    void   Validate() const;
};
```

### 字段说明

| 字段 | 作用 | 备注 |
| --- | --- | --- |
| `span` | 时间维度 | 滚动窗口=桶宽；滑动窗口=保留跨度 |
| `align` | 桶对齐方式 | 见下表 |
| `origin` | `Explicit` 的对齐原点 | 仅 `align == Explicit` 使用 |
| `count` | 条数维度 | 滚动窗口要求 `> 0`；滑动窗口 `0` 表示即进即出 |
| `weight` | 权重维度 | 无 `weight_fn` 时每条权重为 `1.0` |
| `session_gap` | 会话间隙 | 仅 `TumblingWindow` 使用 |
| `predicate` | 单条谓词 | 滚动：命中即收尾；条件窗口另有专用类型 |
| `custom` | 自定义条件 | 可同时看到元素与时间点 |
| `combine` | 条件组合 | `Any`：任一满足；`All`：全部满足 |
| `key_fn` | 时间提取 | 为空时 `KeyOf` 返回 `TimeT{}` |
| `weight_fn` | 权重提取 | 为空时 `WeightOf` 返回 `1.0` |
| `emit_empty` | 是否输出空桶 | 仅滚动窗口 |
| `max_catchup` | 空桶补发上限 | 防止长时间停机后一次输出海量空桶 |

### 对齐方式

```cpp
enum class WindowAlign {
    Epoch,       // 对齐到 TimeT{}（如 Unix 纪元）
    FirstItem,   // 对齐到窗口首条数据的时间
    Explicit     // 对齐到 policy.origin
};
```

```cpp
TimeT AlignTo( TimeT t, Duration span, TimeT origin );

template <typename TimeT, typename Duration>
TimeT CutoffTime( TimeT latest, Duration window );   // 无符号时间类型饱和到 0

template <typename TimeT, typename Duration>
bool SessionExpired( TimeT last, TimeT now, Duration gap );
```

`SessionExpired` 的判定是 `last < now && now - last > gap`：时间未前进（`now <= last`）时
不会误判为会话断裂，因此可安全用于无符号时间类型。

| 对齐 | 桶边界 | 示例（span=60s，首条 t=100） |
| --- | --- | --- |
| `Epoch` | `origin + k*span` | `[60,120)`、`[120,180)` |
| `FirstItem` | 以首条数据为原点 | `[100,160)`、`[160,220)` |
| `Explicit` | 以 `origin` 为原点 | 取决于 `origin` |

### 校验：`Validate()`

以下情况在构造时抛 `std::invalid_argument`：

| 条件 | 说明 |
| --- | --- |
| `span && !(Duration(0) < *span)` | 跨度必须为正 |
| `weight && *weight < 0` | 权重不能为负 |
| `session_gap && !(Duration(0) < *session_gap)` | 会话间隙必须为正 |
| `max_catchup == 0` | 至少允许补发一个空桶 |
| `align == FirstItem && !span` | `FirstItem` 必须有时间跨度 |

`count == 0` 在 `WindowPolicy` 层是合法的（滑动窗口的“即进即出”），
`TumblingWindow` 会额外拒绝 `count == 0`。

### 时间类型要求

`TimeT` 只需满足：

- 可比较（`<`、`>`、`==`）；
- `TimeT - TimeT` 得到 `Duration`；
- `TimeT - Duration` 得到新的 `TimeT`；
- `TimeT + Duration` 得到新的 `TimeT`（滚动窗口补发空桶时需要）。

因此 `time_t`、`int64_t` 时间戳、`std::chrono::time_point`、游戏 tick 都可以直接用。
无时间维度的窗口使用 `DefaultTimePoint = std::chrono::steady_clock::time_point` 作为占位。

## 滑动窗口基类：`SlidingWindow<T>`

```cpp
template <typename T>
class SlidingWindow {
public:
    using ItemList       = std::vector<T>;
    using AddCallback    = std::function<void( const ItemList & )>;
    using RemoveCallback = std::function<void( const ItemList & )>;

    virtual void Add( T item ) = 0;
    virtual std::size_t Size() const noexcept = 0;
    virtual void Clear() = 0;

    bool Empty() const noexcept;

    void OnAdd( AddCallback callback );
    void OnRemove( RemoveCallback callback );
};
```

### 回调契约

- 通知是**批量**的：一次操作中移除了多条，`OnRemove` 只调用一次，参数是整批数据；
- **状态先更新、后通知**；
- `OnAdd` 参数是本次加入的数据（通常 1 条）；
- `Clear()` 把当前全部数据作为一批移除并通知；空窗口不触发；
- 回调在调用线程**同步**执行；回调中调用 `Add()` 会同步递归触发；
- 回调中注册新监听是安全的，新监听从**下一个事件**起生效（内部对数量做快照）；
- 回调抛出的异常会向调用者传播（窗口不做捕获），需要自行 `try/catch`；
- 窗口**非线程安全**，多线程访问需外部同步。

<a id="rulewindow"></a>
## `RuleWindow`：滑动族统一引擎

```cpp
template <typename T, typename TimeT>
class RuleWindow : public SlidingWindow<T> {
public:
    using Policy   = WindowPolicy<T, TimeT>;
    using Duration = typename Policy::Duration;
    using KeyFn    = typename Policy::KeyFn;
    using ItemList = typename SlidingWindow<T>::ItemList;

    struct Entry {
        T      item;
        TimeT  key{};
        double weight{ 0.0 };
    };
    using Storage = std::vector<Entry>;

    explicit RuleWindow( Policy policy, Duration time_cooldown = Duration{} );

    void Add( T item ) override;
    std::size_t Poll( TimeT now );

    std::size_t Size() const noexcept override;
    void        Clear() override;

    double                  TotalWeight() const noexcept;
    std::optional<TimeT>    LatestTime() const;
    const Policy &          GetPolicy() const noexcept;
    const Storage &         Entries() const noexcept;
    ItemList                Items() const;
    ItemList                GetRecent( std::size_t n ) const;
    ItemList                Query( std::optional<TimeT> start,
                                   std::optional<TimeT> end ) const;

    // 按时间升序的只读迭代器（value_type = T）
    ItemIterator begin() const;
    ItemIterator end() const;
};
```

### 构造约束

`RuleWindow` 是滑动族的通用形态，但**不接受**以下语义（它们属于别的窗口）：

| 字段 | 原因 | 替代 |
| --- | --- | --- |
| `session_gap` | 会话切分是“结束”而非“淘汰” | `SessionWindow` |
| `predicate` | 条件打破会清空全部数据 | `PredicateWindow` |
| `custom` | 收尾条件不是滑动语义 | `TumblingWindow` |

同时要求 `span` / `count` / `weight` **至少配置一项**，否则抛 `std::invalid_argument`。

### 淘汰顺序

一次 `Add` / `Poll` 按以下顺序计算要淘汰的条数，最后**一次性**批量移除并通知：

1. **时间**：`cutoff = latest - span`，移除所有 `key < cutoff` 的最旧数据；
2. **条数**：若剩余条数超过 `count`，从最旧端移除；
3. **权重**：若剩余总权重超过 `weight`，从最旧端继续移除，直到不超过上限。

`latest` 的取值：

| 调用 | `latest` | 是否遵守冷却 |
| --- | --- | --- |
| `Add` | 窗口内最新数据的 key | 是（`time_cooldown`） |
| `Poll(now)` | `max(最新 key, now)` | 否（显式推进总是立即生效） |

`time_cooldown` 用于避免每条数据都做时间裁剪：距离上次裁剪的数据时间不足冷却时跳过时间维度
（条数/权重维度仍会执行）。`Poll` 绕过冷却，适合低频主循环。

### `Poll`

```cpp
std::size_t Poll( TimeT now );
```

- 无 `span` 或窗口为空时返回 `0`；
- 返回**本次淘汰的条数**；
- 即使没有新数据，也能清理过期数据（缺席驱动）。

### 查询

| 方法 | 说明 |
| --- | --- |
| `Items()` | 按时间升序返回全部元素的副本 |
| `GetRecent(n)` | 最近 `n` 条（按时间顺序取尾部）；`n=0` 返回空；超过长度返回全部 |
| `Query(start, end)` | 闭区间 `[start, end]`；任一端 `nullopt` 表示不限制 |
| `Entries()` | 直接访问内部 `Entry`（含缓存的 key / weight），零拷贝 |
| `begin()` / `end()` | 只读前向迭代器，`*it` 得到 `const T&` |

```cpp
for ( const auto &event : window ) {
    std::cout << event.id << '\n';
}

auto recent = window.GetRecent( 10 );
auto range  = window.Query( 1000, 2000 );
```

> `GetRecent` 对时间窗口是“key 最大的 n 条”；对 `CountWindow` 等无时间维度的窗口，
> 所有 key 相同，等价于“最近插入的 n 条”。

### 惰性裁剪与 key 缓存

- 每条数据只在 `Add` 时调用一次 `key_fn` / `weight_fn`，结果缓存在 `Entry` 中；
- `Add` 使用 `upper_bound` 按 key 有序插入：末尾追加是快路径，乱序数据二分定位；
- 有序结构让淘汰、`Query`、`GetRecent` 都是线性/对数复杂度。

### 示例：组合规则

```cpp
struct Packet { std::int64_t ts; std::size_t bytes; };

WindowPolicy<Packet, std::int64_t> policy;
policy.span      = 5000;    // 最近 5 秒
policy.count     = 1000;    // 最多 1000 条
policy.weight    = 65536;   // 总字节数不超过 64 KiB
policy.key_fn    = []( const Packet &p ) { return p.ts; };
policy.weight_fn = []( const Packet &p ) { return static_cast<double>( p.bytes ); };

RuleWindow<Packet, std::int64_t> window( policy );

window.OnRemove( []( const std::vector<Packet> &removed ) {
    // 一次性收到被淘汰的整批数据
} );

window.Add( Packet{ 1000, 512 } );
window.Poll( 7000 );          // 淘汰 ts < 2000 的数据
```

## 单维度特例

三个特例都是 `RuleWindow` 的薄封装，**公开 API 全部保留**，只是提供更顺手的构造函数与访问器。

### `CountWindow<T>`

```cpp
template <typename T>
class CountWindow final : public RuleWindow<T, DefaultTimePoint> {
public:
    explicit CountWindow( std::size_t max_len );
    std::size_t Capacity() const noexcept;
};
```

- 固定容量 FIFO，超出即淘汰最旧；
- `max_len == 0` 表示“即进即出”：`Add` 后立刻触发 `OnRemove`，窗口始终为空；
- `Capacity()` 返回构造时的容量。

```cpp
CountWindow<int> window( 3 );
window.Add( 1 ); window.Add( 2 ); window.Add( 3 ); window.Add( 4 );
// 窗口内：{2,3,4}，OnRemove 收到 {1}
```

### `TimeWindow<T, TimeT>`

```cpp
template <typename T, typename TimeT>
class TimeWindow final : public RuleWindow<T, TimeT> {
public:
    using Duration = ...;
    using KeyFn    = ...;

    TimeWindow( Duration window_size,
                KeyFn key_fn,
                Duration prune_cooldown = Duration{} );
};
```

- 保留范围 `[latest - window_size, latest]`；
- 有序插入（乱序数据按 key 定位）；
- `Add` 时按 `prune_cooldown` 节流裁剪；`Poll(now)` 可随时主动裁剪；
- `TimeT` 的约束见上文“时间类型要求”。

```cpp
struct Log { std::string msg; std::int64_t ts; };
TimeWindow<Log, std::int64_t> window(
    60'000, []( const Log &l ) { return l.ts; }, 1'000 );
```

### `ThresholdWindow<T>`

```cpp
template <typename T>
class ThresholdWindow final : public RuleWindow<T, DefaultTimePoint> {
public:
    using WeightFn = std::function<double( const T & )>;

    ThresholdWindow( double max_capacity, WeightFn weight_fn );

    double CurrentWeight() const noexcept;
    double MaxCapacity() const noexcept;
};
```

- 添加后若总权重超过 `max_capacity`，从最旧端批量淘汰直到不超过上限；
- 单条数据本身超过上限时“即进即出”（加入后立刻被淘汰）；
- 权重为负时行为未定义，构造前请自行保证。

```cpp
ThresholdWindow<std::string> window(
    1024.0, []( const std::string &s ) { return static_cast<double>( s.size() ); } );
```

## `TumblingWindow`

```cpp
template <typename T, typename TimeT>
class TumblingWindow {
public:
    using Policy        = WindowPolicy<T, TimeT>;
    using Duration      = typename Policy::Duration;
    using ItemList      = std::vector<T>;
    using CloseCallback = std::function<void( TimeT bucket_start, const ItemList &items )>;
    using ListCallback  = std::function<void( const ItemList &items )>;

    explicit TumblingWindow( Policy policy );

    void        Add( T item );
    std::size_t Poll( TimeT now );
    void        Close();
    void        Clear();

    std::size_t Size() const noexcept;
    bool        Empty() const noexcept;
    std::size_t LateCount() const noexcept;
    std::optional<TimeT> BucketStart() const;
    double      CurrentWeight() const noexcept;
    const Policy &GetPolicy() const noexcept;

    void OnAdd( ListCallback callback );
    void OnRemove( ListCallback callback );
    void OnClose( CloseCallback callback );
};
```

### 三个驱动入口

| 入口 | 触发什么 |
| --- | --- |
| `Add(item)` | 跨时间桶、会话断裂、以及条数/权重/谓词/自定义收尾 |
| `Poll(now)` | 时间桶推进（可补发空桶）、会话空闲超时 |
| `Close()` | 立即输出当前桶（空则无输出），重置全部分桶状态 |
| `Clear()` | 丢弃当前桶数据，不输出；通过 `OnRemove` 通知；桶边界保持 |

### `Add` 的处理顺序

1. **时间桶**：建立锚点；若 `key < 当前桶起点`，判定为迟到，**丢弃**并 `late_count_++`；
   否则推进到 `key` 所在桶（跨桶会收尾旧桶）；
2. **会话间隙**：若 `key - last_key > session_gap`，先收尾旧会话；
3. **入窗**：`OnAdd` → 追加数据 → 累加权重；无时间桶时以本批首条数据的时间作为桶标签；
4. **数据驱动收尾**：检查条数 / 权重 / 谓词 / 自定义，命中则 `CloseBucket()`。

### 收尾条件

| 条件 | 判定 |
| --- | --- |
| `count` | `items.size() >= count` |
| `weight` | `current_weight >= weight` |
| `predicate` | `predicate(item)` 为 `true` |
| `custom` | `custom(item, key)` 为 `true` |

`combine` 决定组合方式：

- `Any`（默认）：任一条件满足即收尾；
- `All`：所有**已配置**的条件都满足才收尾。

> 注意：条数/权重是“**达到**阈值”即收尾，不是“超过”。因此 `count = 100` 表示每 100 条一批。
> 同一时间桶内可以因条数/权重多次收尾（部分聚合），桶起点保持不变。

### `Poll` 与 `Close`

```cpp
std::size_t closed = window.Poll( now );
```

- 返回本次收尾的桶数（含补发的空桶与会话收尾）；
- 首次调用只建立锚点，不输出（避免把“从现在开始”误当成一个完整桶）；
- 时间回拨时不回退（保持当前桶）；
- `emit_empty = true` 时，跨过的空桶逐一输出，最多 `max_catchup` 个；
- `Close()` 不返回值；空桶不调用 `OnClose`。

### 回调

| 回调 | 时机 | 参数 |
| --- | --- | --- |
| `OnAdd` | 数据入窗 | 本批数据（单条） |
| `OnRemove` | `Clear()` 丢弃 | 被丢弃的数据 |
| `OnClose` | 桶收尾（数据驱动/`Poll`/`Close`） | 桶起点 + 桶内数据 |

- 回调同步执行；回调中注册新监听安全（数量快照，下一事件生效）；
- `OnClose` 的参数是**桶内数据的引用**：回调返回后数据即被清空，需要保留请自行拷贝；
- 非线程安全。

### 示例：每 60 秒一批

```cpp
struct Sample { int value; std::int64_t ts; };

WindowPolicy<Sample, std::int64_t> policy;
policy.span    = 60;
policy.emit_empty = false;
policy.key_fn  = []( const Sample &s ) { return s.ts; };

TumblingWindow<Sample, std::int64_t> window( policy );
window.OnClose( []( std::int64_t bucket_start, const std::vector<Sample> &items ) {
    int sum = 0;
    for ( const auto &s : items ) sum += s.value;
    std::cout << "bucket " << bucket_start << ": " << sum << '\n';
} );

window.Add( Sample{ 1, 0 } );
window.Add( Sample{ 2, 30 } );
window.Poll( 60 );       // 输出 [0,60) 的聚合
```

### 示例：每 100 条一批（无时间维度）

```cpp
WindowPolicy<int, DefaultTimePoint> policy;
policy.count = 100;

TumblingWindow<int, DefaultTimePoint> window( policy );
window.OnClose( []( DefaultTimePoint, const std::vector<int> &items ) {
    // 每 100 条调用一次
} );
```

### 示例：迟到数据

```cpp
WindowPolicy<Sample, std::int64_t> policy;
policy.span   = 60;
policy.key_fn = []( const Sample &s ) { return s.ts; };

TumblingWindow<Sample, std::int64_t> window( policy );
window.Add( Sample{ 1, 100 } );    // 建立桶 [60,120)
window.Add( Sample{ 2, 50 } );     // 迟到：key < 60，被丢弃
assert( window.LateCount() == 1 );
```

## `SessionWindow`

```cpp
template <typename T, typename TimeT>
class SessionWindow final : public SlidingWindow<T> {
public:
    using Duration = decltype( std::declval<TimeT>() - std::declval<TimeT>() );
    using KeyFn    = std::function<TimeT( const T & )>;

    SessionWindow( Duration session_timeout, KeyFn key_fn );

    void        Add( T item ) override;
    std::size_t Poll( TimeT now );
    std::size_t Size() const noexcept override;
    void        Clear() override;

    ItemList Close();

    using CloseCallback = std::function<void( const ItemList & )>;
    void OnClose( CloseCallback callback );

    // 按到达顺序的只读迭代
    std::deque<T>::const_iterator begin() const noexcept;
    std::deque<T>::const_iterator end() const noexcept;
};
```

### 会话边界

- 以**会话内最大时间点**为基准；新数据与它的间隔 `> session_timeout` 时，旧会话结束；
- 会话内的乱序数据按**到达顺序**保存，不影响边界判定；
- 假设数据近似按时间有序到达。

### 三个结束方式

| 调用 | 是否触发 `OnRemove` | 是否触发 `OnClose` | 语义 |
| --- | --- | --- | --- |
| `Add` 切分 | 是 | 是 | 会话自然完成 |
| `Poll(now)` 超时 | 是 | 是 | 空闲超时，会话完成 |
| `Close()` | 是 | 是 | 显式收尾 |
| `Clear()` | 是 | **否** | 放弃会话（数据被丢弃） |

- `Poll(now)` 返回本次结束的会话数（`0` 或 `1`）；
- `Close()` 返回被归档的数据列表（移动语义），空会话返回空列表且不触发回调；
- `OnClose` 与 `OnRemove` **并行通知**：前者是“会话完成”的语义事件，后者是“数据离开窗口”。

```cpp
struct Click { int user; std::int64_t ts; };

SessionWindow<Click, std::int64_t> window(
    30'000, []( const Click &c ) { return c.ts; } );

window.OnClose( []( const std::vector<Click> &session ) {
    std::cout << "session size = " << session.size() << '\n';
} );

window.Add( Click{ 1, 1000 } );
window.Add( Click{ 1, 2000 } );
window.Add( Click{ 1, 100000 } );   // 间隔 98s > 30s：先归档旧会话
window.Poll( 200000 );              // 空闲超时：结束当前会话
```

## `PredicateWindow`

```cpp
template <typename T>
class PredicateWindow final : public SlidingWindow<T> {
public:
    using Predicate      = std::function<bool( const T & )>;
    using RejectCallback = std::function<void( const T & )>;

    explicit PredicateWindow( Predicate predicate );

    void        Add( T item ) override;
    std::size_t Size() const noexcept override;
    void        Clear() override;

    ItemList GetRecent( std::size_t n ) const;
    void     OnReject( RejectCallback callback );

    std::deque<T>::const_iterator begin() const noexcept;
    std::deque<T>::const_iterator end() const noexcept;
};
```

- `predicate(item) == true`：数据入窗；
- `predicate(item) == false`：
  1. 将窗口内现有数据**整批** `OnRemove` 并清空；
  2. 打破条件的数据**不入窗**；
  3. 通过 `OnReject` 通知该数据（此前没有任何通道能拿到它）。

```cpp
PredicateWindow<int> rising( []( int v ) { return v > 0; } );
rising.OnReject( []( int v ) { std::cout << "rejected: " << v << '\n'; } );

rising.Add( 1 );      // 入窗
rising.Add( 2 );      // 入窗
rising.Add( -1 );     // 清空 {1,2}，-1 走 OnReject，窗口为空
```

## 线程安全与生命周期

- 所有窗口**非线程安全**；多线程访问需外部加锁。
- 回调在调用线程同步执行；回调中不得销毁窗口自身。
- 回调中调用 `Add` 会同步递归；窗口的索引遍历与数量快照保证不会迭代器失效。
- `RuleWindow::Entries()` / `Items()` / `GetRecent()` / `Query()` 返回副本或内部引用：
  - `Entries()` 返回内部引用，窗口修改后失效；
  - `Items()` / `GetRecent()` / `Query()` 返回副本，安全但可能拷贝。
- `TumblingWindow::OnClose` / `SessionWindow::OnClose` 的参数是**引用**，回调返回后可能被清空/移动。

## 选型指南

| 需求 | 选择 |
| --- | --- |
| 固定容量，保留最近 N 条 | `CountWindow` |
| 保留最近一段时间 | `TimeWindow` |
| 总大小/权重不超过上限 | `ThresholdWindow` |
| 时间 + 条数 + 权重的任意组合 | `RuleWindow` |
| 周期性输出一批（时间/条数/权重/条件） | `TumblingWindow` |
| 空闲超时切分会话 | `SessionWindow` |
| 连续满足某条件的数据序列 | `PredicateWindow` |
| 自定义“何时结束一批” | `TumblingWindow` + `custom` / `predicate` |
| 自定义“何时淘汰” | `RuleWindow` 的组合维度；条件淘汰请用 `PredicateWindow` |

## 行为边界速查

| 场景 | 行为 |
| --- | --- |
| `CountWindow(0)` | 数据即进即出，`OnAdd` 后立即 `OnRemove` |
| `RuleWindow` 未配置 span/count/weight | 构造抛 `std::invalid_argument` |
| `RuleWindow` 配置 `session_gap`/`predicate`/`custom` | 构造抛 `std::invalid_argument` |
| `TumblingWindow` 的 `count == 0` | 构造抛 `std::invalid_argument` |
| `TumblingWindow` 首次 `Poll` | 只建立锚点，不输出 |
| `TumblingWindow` 收到迟到数据 | 丢弃，`LateCount()` 增加 |
| `TumblingWindow` 同一桶多次收尾 | 允许（条数/权重部分聚合），桶起点不变 |
| `TumblingWindow::Clear()` | 丢弃当前数据，不触发 `OnClose`，桶边界保持 |
| `SessionWindow::Clear()` | 放弃会话，只触发 `OnRemove` |
| `SessionWindow::Close()` | 归档并触发 `OnRemove` + `OnClose` |
| `PredicateWindow` 条件被打破 | 清空现有数据，打破条件的数据走 `OnReject` |
| 回调中注册新监听 | 下一事件生效 |
| 回调抛异常 | 向调用者传播（窗口不捕获） |
| 无时间维度的窗口调用 `Poll` | 返回 `0`（`RuleWindow` / `SessionWindow` 语义各自处理） |
