# utils API 文档

本目录是 [Cpp Utils](../README.md) 的 API 参考。库以 **C++17 头文件 + 静态库** 的形式提供，
类与函数位于 `namespace utils`（`Macros.h` / `Pimpl.h` 提供的是全局宏与平台常量），
公共头文件平铺在 `src/`，直接按文件名包含即可：

```cpp
#include "DateTime.h"
#include "TaskScheduler.h"
```

## 文档索引

| 文档 | 覆盖模块 | 适用场景 |
| --- | --- | --- |
| [日期与时间](datetime.md) | `DateTime`、`TimeZone`、`DateTimeParts`、时间单位 | 解析/格式化时间、时区换算、日历加减、按粒度比较 |
| [字符串](string.md) | `StringUtil`、`StringFormatter`、`StringParser` | 切分、替换、数字互转、正则提取、模板化解析 |
| [文件与归档](filesystem.md) | `File`、`FileUtil`、`LockedFile`、`TarUtil` | 读写文件、目录遍历、临时文件、文件锁、tar 压缩 |
| [任务调度](task-scheduler.md) | `TaskScheduler`、触发器、`TaskPolicy`、`TaskExecutor` | 定时/周期任务、日历任务、确定性时间推进 |
| [窗口](windows.md) | `WindowPolicy`、`TumblingWindow`、`SlidingWindow` 族 | 滑动窗口聚合、滚动窗口分桶、会话窗口、条件窗口 |
| [并发与计时](concurrency.md) | `ThreadPoolExecutor`、`Timer`、`ElapsedTimer`、`Throttler`、`ConsumableOptional`、`SingleTrigger` | 线程池、超时/计时、节流、一次性交接 |
| [通用工具](utilities.md) | `Result`、`Compare`、`ResourceGuard`、`Macros`、`Pimpl`、`BreakpadHandler` | 错误处理、浮点比较、RAII、pimpl、崩溃转储 |

## 按任务查找

| 我想…… | 去看 |
| --- | --- |
| 把 `"2026-11-06T12:00:00+08:00"` 解析成时间 | [`DateTime::FromIso8601`](datetime.md#fromiso8601) |
| 求“下个月同一天”，并处理月末 | [`DateTime::Add<Months>`](datetime.md#add--addrangeend) |
| 每天 09:30 执行任务 | [`CalendarTrigger::Daily`](task-scheduler.md#calendartrigger) |
| 在单元测试里精确推进时间，不 sleep | [`TaskScheduler::Poll`](task-scheduler.md#poll) |
| 保留最近 5 秒且最多 1000 条数据 | [`RuleWindow`](windows.md#rulewindow) |
| 每 60 秒输出一批聚合结果 | [`TumblingWindow`](windows.md#tumblingwindow) |
| 空闲 30 秒就结束会话 | [`SessionWindow::Poll`](windows.md#sessionwindow) |
| 把字符串安全地转成整数/浮点 | [`StringUtil::ToNumber`](string.md#tonumber--fromnumber) |
| 用 `name={name},age={age}` 解析日志行 | [`StringParser`](string.md#stringparser) |
| 递归删除目录并统计条目数 | [`FileUtil::RemoveAll`](filesystem.md#fileutil) |
| 有界队列 + 背压的线程池 | [`ThreadPoolExecutor`](concurrency.md#threadpoolexecutor) |
| 链式传播错误而不抛异常 | [`Result`](utilities.md#result) |
| 限制函数调用频率 | [`Throttler`](concurrency.md#throttler) |

## 阅读约定

- **签名**以代码块给出，省略了模板约束与 `[[nodiscard]]` 等修饰；完整声明以头文件为准。
- **线程安全**：每个模块单独说明。默认约定是**非线程安全**，需要外部同步；例外会显式标注。
- **失败方式**分三类，文档中会明确写出：
  - 返回 `std::optional` / `bool` / 空容器（可预期的失败）；
  - 返回 `Result<T, E>`（需要携带错误信息）；
  - 抛异常（构造参数非法、前置条件被破坏）。
- **回调**：调度器与窗口的用户回调都在无锁上下文同步执行。调度器会吞掉回调异常以保证派发循环
  继续运行；窗口的回调异常则向调用者传播（需要自行 `try/catch`）。`StringUtil::FormatCString`
  等明确标注会抛异常的函数除外。
- **时间单位**：`DateTime` 的时间单位（`Years`、`Months`、`Days`、`Hours`……）是**普通类型标签**，
  而 `StringUtil` 的字节单位是普通枚举；两者都可与整型自然互转。
- 示例默认 `using namespace utils;`，并假设已包含对应头文件。

## 模块依赖

```
                 ┌──────────────┐
                 │  DateTime.h  │
                 └──────┬───────┘
        ┌───────────────┼────────────────┐
        ▼               ▼                ▼
 TaskScheduler.h   WindowPolicy.h   （独立使用）
        │               │
        ▼               ├── TumblingWindow.h
 TaskExecutor.h         └── SlidingWindow.h
        │
        ▼
 ThreadPoolExecutor.h
```

- `WindowPolicy.h` 是窗口两族**唯一**的共享依赖；`TumblingWindow` 与 `SlidingWindow` 彼此不依赖。
- `TaskScheduler` 与窗口族**零依赖**，可单独使用，也可在同一主循环里各自 `Poll`。
- `ThreadPoolExecutor` 只依赖 `TaskExecutor` 接口，可注入调度器，也可单独当线程池用。

## 错误处理风格

| 场景 | 推荐做法 |
| --- | --- |
| 解析用户输入 | 返回 `std::optional`（如 `DateTime::FromIso8601`、`StringUtil::ToNumber`） |
| 需要错误详情并链式处理 | `Result<T, std::string>` + `Map` / `AndThen` |
| 构造参数非法 | 抛 `std::invalid_argument`（窗口策略、调度触发器） |
| 文件/系统调用失败 | 返回 `bool` / 空字符串 / 空容器，不抛异常 |
| 回调内部异常 | 调度器吞掉以保证驱动循环继续；窗口向调用者传播 |

## 相关链接

- [项目 README](../README.md)：构建、测试、安装与设计约定
- [测试用例](../test/)：每个模块的行为边界都有对应用例，可作为可执行文档
