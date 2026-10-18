# Cpp Utils

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

C++17 通用工具库（静态库 `utils`）：文件与字符串处理、时区感知时间、任务调度、并发原语与错误处理等基础设施。

## 目录

- [特性](#特性)
- [模块概览](#模块概览)
- [环境要求](#环境要求)
- [构建](#构建)
- [使用](#使用)
- [测试](#测试)
- [安装](#安装)
- [项目结构](#项目结构)
- [设计约定](#设计约定)
- [贡献](#贡献)
- [许可证](#许可证)
- [第三方依赖](#第三方依赖)

## 特性

- **时间基础设施**：时区感知的 `DateTime`（Local / UTC / 固定偏移、ISO 8601、日历运算）；`TaskScheduler` 双时间轴调度
  （steady 间隔 + wall 日历）；`ThreadPoolExecutor` 提供异步执行
- **确定性核心**：调度由 `AdvanceTo( wall_now, steady_now )` 驱动，核心测试无需 sleep；线程适配器与线程池均为可选注入
- **明确的 DST 策略**：春季不存在的时刻跳过、秋季重复时刻只触发较早一次、月度缺日跳过该月
- **统一命名**：解析/转换类函数一律 `ToXXX` / `FromXXX`；时间单位使用普通枚举，便于与整型互转
- **严格构建**：本工程目标统一启用 `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`，零告警；
  可选 `UTILS_WERROR` 与 sanitizer（ASan / UBSan / TSan）
- **现代 CMake**：CTest 集成（单测粒度 + 并行）、ccache 自动探测、`GNUInstallDirs` 安装、
  `target_compile_features` 自动向消费方传递 C++17 需求

## 模块概览

公共头文件平铺在 `src/` 下，按文件名直接包含（例如 `#include "DateTime.h"`）。

| 模块 | 头文件 | 说明 |
| --- | --- | --- |
| `FileUtil` / `File` | `FileUtil.h` | 文件与目录操作：读写、复制/移动、路径拼接、临时文件、目录遍历、权限 |
| `LockedFile` | `LockedFile.h` | 带文件锁（POSIX `fcntl`）的文件封装 |
| `StringUtil` | `StringUtil.h` | 字符串处理：拆分、替换、Trim、Join、数字解析、编码转换 |
| `StringFormatter` | `StringUtil.h` | `%1` 占位符风格的字符串格式化 |
| `StringParser` | `StringUtil.h` | 基于模板规则（如 `name={name},age={age}`）的文本解析与取值 |
| `DateTime` | `DateTime.h` | 时区感知的日期时间值类型，`ToXXX`/`FromXXX` 转换矩阵：chrono / `time_t` / `tm` / Unix 秒-毫秒-微秒-纳秒 / 字符串（strftime 风格 + ISO 8601）；日历加法与周期边界（`Add` / `StartOf` / `EndOf`）、按粒度比较；服务器时间种子（抗系统时间篡改） |
| `Timer` / `ElapsedTimer` | `Timer.h` | 超时判定与累计计时 |
| `TaskScheduler` | `TaskScheduler.h` | 时间驱动任务调度器：双时间轴、`AdvanceTo` 确定性推进、线程适配器（`Start`/`Stop`）、触发器（一次性 / 固定周期 / 每日-每周-每月日历 / 自定义）、迟到策略（CatchUp / Skip + grace）、coalesce、抖动、任务统计 |
| `TaskExecutor` / `ThreadPoolExecutor` | `TaskExecutor.h` / `ThreadPoolExecutor.h` | 执行器接口（默认同步 `InlineExecutor`）与工作线程池（仅入队不阻塞派发、有界队列背压、析构排空后 join） |
| `SlidingWindow` | `SlidingWindow.h` | 泛型滑动窗口族：`CountWindow`、`TimeWindow`、`ThresholdWindow`、`SessionWindow`、`PredicateWindow`，统一 `OnAdd`/`OnRemove` 回调 |
| `Result` | `Result.h` | `Result<T, E>` / `Result<void, E>` 错误处理类型（`Map` / `AndThen` 链式变换） |
| `ResourceGuard` | `ResourceGuard.h` | RAII 资源守卫 |
| `ConsumableOptional` | `ConsumableOptional.h` | 一次性消费的可选值，支持线程安全模式 |
| `SingleTrigger` / `Throttler` | `SingleTrigger.h` / `Throttler.h` | 一次性触发器与频率节流器 |
| `BreakpadHandler` | `BreakpadHandler.h` | Breakpad 崩溃转储管理（单例），支持转储文件回收 |
| `TarUtil` | `TarUtil.h` | 基于系统 `tar` 的压缩/解压封装 |
| `Compare` / `Pimpl` / `Macros` | `Compare.h` / `Pimpl.h` / `Macros.h` | 浮点比较（`IsEqual` / `IsClose`）、pimpl 辅助、平台与编译器宏 |

## 环境要求

| 项 | 要求 |
| --- | --- |
| CMake | ≥ 3.14 |
| 编译器 | 支持 C++17（GCC / Clang / MSVC）；当前主要在 GCC 11 + Linux 验证 |
| 平台 | Linux（已验证）。`LockedFile` 依赖 POSIX `fcntl`、`BreakpadHandler` 依赖 Breakpad 客户端；macOS / Windows 未验证 |
| 可选工具 | `git`（GitHash 版本信息生成）、`tar`（`TarUtil`）、`ccache`（自动探测）、`python3`（`tool/` 下的脚本） |

## 构建

```bash
git clone https://github.com/Polaris-Night/utils.git
cd utils
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

单配置生成器下未指定构建类型时默认 `RelWithDebInfo`。本工程目标启用
`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`；第三方 Breakpad 头文件标记为 `SYSTEM`，不产生告警。

### 构建选项

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `UTILS_WERROR` | `OFF` | 把编译告警当作错误（CI 建议开启） |
| `UTILS_BUILD_BREAKPAD_TOOLS` | `OFF` | 构建 Breakpad 符号化工具（`dump_syms` / `minidump_stackwalk` / `minidump_module_reader`）；仅做崩溃捕获无需开启，关闭可显著缩短构建时间 |
| `UTILS_ENABLE_SANITIZERS` | `OFF` | 启用 sanitizer |
| `UTILS_SANITIZER` | `address,undefined` | sanitizer 列表，例如 `thread` |
| `UTILS_ENABLE_CCACHE` | `ON` | 自动探测并使用 ccache |

## 使用

### 集成到你的工程

```cmake
add_subdirectory(utils)
target_link_libraries(your_target PRIVATE utils)   # 头文件目录与 C++17 需求自动继承
```

### 快速上手

**DateTime：时区感知与日历运算**

```cpp
#include "DateTime.h"
using namespace utils;

DateTime    local = DateTime::Now();                    // 本地时区
DateTime    utc   = DateTime::Now( TimeZone::Utc() );   // UTC
DateTime    later = local.Add<Milliseconds>( 500 );     // 日历加法（保持墙钟语义）
std::string iso   = later.ToIso8601();                  // 2026-10-18T12:00:00.500+08:00

if ( auto parsed = DateTime::FromIso8601( iso ) ) {     // std::optional<DateTime>
    DateTimeParts parts = parsed->ToParts();
    std::cout << parts.year << '-' << parts.month << '-' << parts.day << '\n';
}
```

**Result：错误传播与链式变换**

```cpp
#include "Result.h"
#include "StringUtil.h"
using namespace utils;

Result<int, std::string> ParseInt( std::string_view text ) {
    auto value = StringUtil::ToNumber<int>( text );
    if ( !value ) {
        return MakeErr<int, std::string>( "不是合法整数" );
    }
    return MakeOk<int, std::string>( std::move( *value ) );   // MakeOk 取右值
}

Result<int, std::string> ok      = 42;                                 // 直接构造 Ok
const int                doubled = ok.Map( []( int v ) { return v * 2; } ).ValueOr( 0 );
```

**StringUtil / FileUtil：切分、数字解析与文件读写**

```cpp
#include "FileUtil.h"
#include "StringUtil.h"
using namespace utils;

const std::string path = FileUtil::JoinPaths( "logs", "app.log" );
FileUtil::WriteStr( path, "hello\n", /* append = */ false );

File              file( path );
const std::string content = file.ReadAll();

const std::vector<std::string> fields = StringUtil::Split( "a,b,c", "," );
const std::optional<int>       number = StringUtil::ToNumber<int>( "42" );
```

**SlidingWindow：泛型滑动窗口**

```cpp
#include "SlidingWindow.h"
using namespace utils;

CountWindow<int> window( 3 );  // 最多保留 3 条，超出即淘汰最旧
window.OnRemove( []( const std::vector<int> &removed ) { /* 被淘汰的数据 */ } );

for ( int i = 1; i <= 4; ++i ) {
    window.Add( i );           // 第 4 条加入时，1 被淘汰
}
```

**TaskScheduler：确定性调度 + 可选线程适配器**

```cpp
#include "TaskScheduler.h"
#include "ThreadPoolExecutor.h"
using namespace utils;

ThreadPoolExecutor pool( 2 );   // 回调异步执行；不注入执行器则默认同步执行（便于确定性测试）
TaskScheduler      scheduler( pool );

// 间隔任务：每秒一次（steady 轴，固定网格不漂移）
auto id = scheduler.Add(
    std::make_shared<IntervalTrigger>( std::chrono::seconds( 1 ) ),
    []( const DateTime &scheduled ) { std::cout << "tick " << scheduled.ToIso8601() << '\n'; } );
if ( !id ) {
    return;                     // 触发器/回调为空，或一次性任务已过期
}

// 日历任务：每天 09:30（wall 轴，按指定时区）
scheduler.Add( std::make_shared<CalendarTrigger>( CalendarTrigger::Daily( 9, 30 ) ),
               []( const DateTime & ) { std::cout << "daily\n"; } );

scheduler.Start();
// ... 业务代码 ...
scheduler.Stop();               // 请求停止并等待派发线程退出

if ( auto stats = scheduler.GetTaskStats( *id ) ) {
    std::cout << "触发 " << stats->fire_count << " 次，迟到 " << stats->late_count << " 次\n";
}
```

> 确定性核心：不调用 `Start()` 时，用 `AdvanceTo( wall_now, steady_now )` 手动推进即可在测试中精确控制时间，无需 sleep。

## 测试

```bash
cd build
ctest -j"$(nproc)"                   # 全量（当前 21 个套件 / 211 个用例）
ctest -R ThreadPoolExecutorTest      # 按套件/用例名筛选
./test/main --gtest_brief=1          # 也可直接运行测试二进制
./test/main --gtest_filter='TaskSchedulerTest.*'
```

用例位于 `test/`（新增 `test/test_<模块>.cpp` 会被自动纳入构建）；`test/test_breakpad/` 用于崩溃捕获链路的手工验证。

### Sanitizer

```bash
cmake -S . -B build-asan -DUTILS_ENABLE_SANITIZERS=ON -DUTILS_SANITIZER=address,undefined
cmake --build build-asan -j"$(nproc)"
(cd build-asan && ctest)
```

- `address` / `address,undefined`：全量用例通过，无越界、无泄漏
- `thread`：使用 `-DUTILS_SANITIZER=thread`。若内核 ASLR 较激进而报 `unexpected memory mapping`，
  以 `setarch -R` 运行；此外本机 GCC 11 环境下 TSan 对条件变量交接模式存在伪报
  （用纯标准库最小复现可复现同样的 "double lock"），跨环境复核结果更有参考价值

## 安装

```bash
cmake --install build --prefix <install-prefix>
```

安装 `libutils.a`（含静态依赖 `libbreakpad_client.a`）与全部公共头文件。本工程暂不提供
`find_package(utils)` 导出包，外部复用建议直接 `add_subdirectory`（见 [使用](#使用)）。

## 项目结构

```
utils/
├── CMakeLists.txt            # 顶层：构建选项、告警、sanitizer/ccache、CTest 接入
├── cmake/
│   └── BuildBreakpad.cmake   # 内置 Breakpad 的构建规则
├── src/                      # 库源码与公共头文件（平铺，按文件名 include）
├── test/                     # GoogleTest 用例
│   └── test_breakpad/        # 崩溃注入用例
├── third_party/              # breakpad、GitHash（随仓库内置）
└── tool/
    ├── breakpad/             # 符号化工具与 minidump 处理脚本
    └── debug/                # 远程调试辅助脚本
```

## 设计约定

- **命名**：解析/转换类函数统一 `ToXXX` / `FromXXX`；类型名 `PascalCase`；成员变量以 `_` 结尾
- **时间**：时间单位使用普通枚举而非 `enum class`，便于与整型互转；`DateTime` 默认本地时区
- **调度器**：确定性核心（`AdvanceTo`）与线程适配器（`Start` / `Stop`）解耦，执行器可注入；
  迟到策略为 `CatchUp` / `Skip`
- **回调纪律**：用户回调一律在锁外执行，回调抛出的异常被吞掉，不允许拖垮调度器
- **头文件**：`#pragma once`；公共头文件平铺在 `src/`，互相以 `#include "Xxx.h"` 引用
- **格式**：4 空格缩进、大括号换行（Allman），与现有代码保持一致

## 贡献

1. Fork 仓库并新建分支
2. 提交前保证零告警、全量用例通过：

   ```bash
   cmake -S . -B build -DUTILS_WERROR=ON
   cmake --build build -j"$(nproc)"
   (cd build && ctest -j"$(nproc)")
   ```

3. 新增模块需附 GoogleTest 用例；涉及公共 API 变更时同步更新本 README
4. 提交信息说明动机与行为变化，便于审阅

## 许可证

[MIT](LICENSE) © 2024 Polaris-Night

## 第三方依赖

| 依赖 | 用途 | 许可证 |
| --- | --- | --- |
| [Google Breakpad](https://chromium.googlesource.com/breakpad/breakpad) | 崩溃捕获（`BreakpadHandler`） | BSD 3-Clause |
| [GoogleTest](https://github.com/google/googletest) | 单元测试 | BSD 3-Clause |
| GitHash（Eugenio Bargiacchi） | 编译期版本信息生成 | MIT |
