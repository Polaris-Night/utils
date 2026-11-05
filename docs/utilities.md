# 通用工具

头文件：`Result.h`、`Compare.h`、`ResourceGuard.h`、`Macros.h`、`Pimpl.h`、`BreakpadHandler.h`

除 `Macros.h` 的宏/平台常量与 `Pimpl.h` 的宏为全局作用域外，其余组件位于 `namespace utils`。

| 组件 | 用途 |
| --- | --- |
| `Result<T, E>` | 显式错误传播，支持 `Map` / `AndThen` 链式变换 |
| `Compare` | 浮点比较：`IsEqual` / `IsClose` |
| `ResourceGuard<T>` | RAII 资源守卫（自定义删除器） |
| `Macros` | 平台判断、禁用拷贝/移动、空指针判断 |
| `Pimpl` | pimpl 惯用法的声明与构造宏 |
| `BreakpadHandler` | 崩溃转储的单例管理 |

## `Result`

头文件：`Result.h`

`Result<T, E>` 要么持有 `T`（Ok），要么持有 `E`（Err），不会同时持有两者，也不需要异常。

### 类型

```cpp
template <typename T, typename E = std::string> class Result;
template <typename E> class Result<void, E>;
```

约束：

- `T` 不能是 `void`（请用 `Result<void, E>`）；
- `T` / `E` 不能是引用；
- `T` / `E` 必须可构造、可移动/拷贝（取决于实际使用）。

### 构造与判断

```cpp
Result<int, std::string> ok  = 42;                                    // Ok
Result<int, std::string> err = MakeErr<int, std::string>( "bad" );    // Err

bool IsOk() const;
bool IsErr() const;
```

> 直接传入 `"..."` 时 `E` 会被推导为 `const char *`。若目标是 `std::string`，请显式写
> `MakeErr<int, std::string>( "..." )` 或传 `std::string( "..." )`。

### 取值

```cpp
T &Value() &;
const T &Value() const &;
T &&Value() &&;

E &Error() &;
const E &Error() const &;

template <typename U> T ValueOr( U &&default_value ) const &;
template <typename U> E ErrorOr( U &&default_error ) const &;
```

| 方法 | Ok 时 | Err 时 |
| --- | --- | --- |
| `Value()` | 返回 `T` | 抛 `std::bad_variant_access` |
| `Error()` | 抛 `std::bad_variant_access` | 返回 `E` |
| `ValueOr(x)` | 返回 `T` | 返回 `x` |
| `ErrorOr(x)` | 返回 `x` | 返回 `E` |

### `Map`

```cpp
template <typename F>
auto Map( F &&f ) const -> Result<std::decay_t<std::invoke_result_t<F, const T &>>, E>;
```

- 仅对 Ok 值调用 `f`，返回新的 `Result<R, E>`；
- Err 原样传递；
- 若 `f` 返回 `void`，结果为 `Result<void, E>`。

```cpp
auto r = MakeOk<int>( 21 ).Map( []( int v ) { return v * 2; } );   // Result<int, string>
assert( r.Value() == 42 );
```

### `AndThen`

```cpp
template <typename F>
auto AndThen( F &&f ) const -> std::invoke_result_t<F, const T &>;
```

- `f` 必须返回另一个 `Result<U, E>`，且**错误类型必须与当前 `E` 完全一致**（`static_assert`）；
- Ok 时调用 `f`，Err 时直接返回同错误的 `Result`；
- 用于串联可能失败的步骤，避免嵌套 `if`。

```cpp
Result<int, std::string> Parse( std::string_view s );
Result<int, std::string> Double( int v );

auto result = Parse( "21" ).AndThen( Double );
```

### 工厂函数

```cpp
template <typename T, typename E = std::string>
auto MakeOk( T &&value ) -> Result<std::decay_t<T>, std::decay_t<E>>;

template <typename E = std::string>
auto MakeOk() -> Result<void, std::decay_t<E>>;

template <typename E>
auto MakeErr( E &&error ) -> Result<void, std::decay_t<E>>;

template <typename T, typename E>
auto MakeErr( E &&error ) -> Result<T, std::decay_t<E>>;
```

```cpp
auto a = MakeOk( 42 );                                  // Result<int, string>
auto b = MakeOk<std::string>();                         // Result<void, string>
auto c = MakeErr( std::string( "failed" ) );            // Result<void, string>
auto d = MakeErr<int>( std::string( "failed" ) );       // Result<int, string>
```

> `MakeOk` 取**右值**（`T&&`）。传入 `*optional` 或左值对象时需要显式 `std::move`：
>
> ```cpp
> auto opt = std::optional<int>{ 42 };
> auto r   = MakeOk( std::move( *opt ) );   // 正确
> // auto r = MakeOk( *opt );               // 编译失败（左值）
> ```

### `Result<void, E>`

```cpp
Result<void, E> ok;                              // 默认 Ok
auto err = MakeErr( std::string( "failed" ) );   // Result<void, string>

bool IsOk() const;
bool IsErr() const;
E &Error() &;
const E &Error() const &;
template <typename U> E ErrorOr( U && ) const &;
Result<void, E> Map( F && ) const;
auto AndThen( F &&f ) const;                     // f 返回 Result<U, E>
```

`Map` 对 `void` Ok 无实际操作，仅传递；`AndThen` 执行返回 `Result` 的函数。

```cpp
Result<void, std::string> Step();

auto r = Step().AndThen( []() { return Step(); } );
```

### 完整示例

```cpp
#include "Result.h"
#include "StringUtil.h"
#include <iostream>
using namespace utils;

Result<int, std::string> ParseInt( std::string_view text ) {
    auto value = StringUtil::ToNumber<int>( text );
    if ( !value ) {
        return MakeErr<int>( "not an integer: " + std::string( text ) );
    }
    return MakeOk( std::move( *value ) );      // MakeOk 取右值
}

int main() {
    auto result = ParseInt( "0x2A" )
                      .Map( []( int v ) { return v + 1; } )
                      .AndThen( []( int v ) -> Result<int, std::string> {
                          if ( v < 0 ) {
                              return MakeErr<int, std::string>( "negative" );
                          }
                          return MakeOk( v );
                      } );

    if ( result.IsOk() ) {
        std::cout << result.Value() << '\n';          // 43
    }
    else {
        std::cerr << result.Error() << '\n';
    }
}
```

### 与异常的边界

| 操作 | 行为 |
| --- | --- |
| 构造 Ok/Err | 不抛（除非 T/E 的构造抛） |
| `Value()` 在 Err 时 | 抛 `std::bad_variant_access` |
| `Error()` 在 Ok 时 | 抛 `std::bad_variant_access` |
| `Map` 中 `f` 抛异常 | 向调用者传播（`Map` 不捕获） |
| `AndThen` 中 `f` 抛异常 | 向调用者传播 |

## `Compare`

头文件：`Compare.h`

```cpp
template <typename T>
bool IsEqual( T a, T b );

template <typename T>
bool IsClose( T a, T b,
              T rel_tol = std::numeric_limits<T>::epsilon() * 8,
              T abs_tol = static_cast<T>( 0 ) );
```

### `IsEqual`

- 通用版本：`a == b`；
- `float` / `double` 特化：`|a - b| < epsilon`（**绝对误差**）；
- 适合接近 0 的小数值；大数值请用 `IsClose`。

```cpp
IsEqual( 0.1 + 0.2, 0.3 );        // true（二者差值约 5.6e-17，小于 double epsilon）
IsEqual( 0.0, 1e-20 );            // true（绝对误差小于 epsilon）
IsEqual( 1.0, 1.0 + 1e-15 );      // false（差值大于 epsilon）
```

> 因为用的是绝对误差，`IsEqual` 对“数值很大、但相对差异很小”的两个数会判为不等；
> 这类场景请用 `IsClose`。

### `IsClose`

- 仅接受浮点类型（`static_assert`）；
- 判定：`diff <= abs_tol || diff <= rel_tol * max(|a|, |b|)`；
- `a == b` 直接 `true`（含同号无穷）；
- NaN 或异号无穷返回 `false`。

```cpp
IsClose( 1e15, 1e15 + 1.0 );           // true（相对容差）
IsClose( 1e15, 1e15 + 100.0 );         // false（超出默认相对容差）
IsClose( 0.0, 1e-18, 1e-9, 1e-16 );    // true（绝对容差）
IsClose( std::nan( "" ), 0.0 );        // false
```

### 选型

| 场景 | 建议 |
| --- | --- |
| 精确比较整数/字符串 | `==` / `IsEqual` |
| 接近 0 的浮点 | `IsEqual` 或 `IsClose` + `abs_tol` |
| 大数量级浮点 | `IsClose` + 相对容差 |
| 需要严格控制误差 | 显式传 `rel_tol` / `abs_tol` |

## `ResourceGuard`

头文件：`ResourceGuard.h`

```cpp
template <typename T>
class ResourceGuard {
public:
    using Deleter = std::function<void( T * )>;

    explicit ResourceGuard( T *resource, Deleter &&deleter, bool check = true );

    T *Get() const noexcept;
    T *Release() noexcept;
    void Reset() noexcept;
};
```

- 构造 `check = true` 时：`resource == nullptr` 或 `deleter` 为空，抛 `std::runtime_error`；
- 析构时若 `resource_ && deleter_`，调用 `deleter_(resource_)`；
- 移动构造/赋值：源对象置空；移动赋值先 `Reset()` 自身，避免泄漏；
- 拷贝删除。

| 方法 | 语义 |
| --- | --- |
| `Get()` | 返回原始指针（不转移所有权） |
| `Release()` | 放弃所有权，返回指针；之后析构不释放 |
| `Reset()` | 立即释放并清空 |

```cpp
#include "ResourceGuard.h"
#include <cstdio>

ResourceGuard<std::FILE> file(
    std::fopen( "a.txt", "r" ),
    []( std::FILE *f ) { if ( f ) std::fclose( f ); } );

if ( file.Get() ) {
    // 使用文件
}
// 作用域结束自动 fclose
```

> `ResourceGuard` 不检查 `deleter` 是否为空（`check = false` 时）；空删除器被视为“不释放”，
> 适合需要手动 `Release()` 的场景。

## `Macros`

头文件：`Macros.h`

### 平台判断

```cpp
inline constexpr bool is_windows;
inline constexpr bool is_linux;
inline constexpr bool is_osx;
```

同时定义编译宏：`PLATFORM_OS_WINDOWS`、`PLATFORM_OS_LINUX`、`PLATFORM_OS_OSX`。

```cpp
#if defined( PLATFORM_OS_LINUX )
    // Linux 专用
#endif

if constexpr ( is_windows ) {
    // ...
}
```

### 禁用拷贝/移动

```cpp
DISABLE_COPY( Class );
DISABLE_MOVE( Class );
DISABLE_COPY_MOVE( Class );
```

展开为删除的拷贝/移动构造与赋值。放在类体 `public:` 区域即可。

```cpp
class Handle {
public:
    Handle() = default;
    DISABLE_COPY_MOVE( Handle )
};
```

### 空指针判断

```cpp
template <typename... Args> bool AnyNullptr( Args... args );
template <typename... Args> bool AllNullptr( Args... args );
```

- 至少传 1 个参数（`static_assert`）；
- `AnyNullptr`：任一为 `nullptr` → `true`；
- `AllNullptr`：全部为 `nullptr` → `true`（参数数为 0 不会编译）。

```cpp
AnyNullptr( a, b, c );    // 任一为空
AllNullptr( a, b );       // 全部为空
```

## `Pimpl`

头文件：`Pimpl.h`

用于把实现细节藏进 `.cpp`，减少编译依赖。

```cpp
// MyClass.h
class MyClass {
    PIMPL_DECLARE( MyClass )
public:
    explicit MyClass( std::string name );
    ~MyClass();          // 必须在 .cpp 中定义
};
```

```cpp
// MyClass.cpp
class MyClass::Impl {
public:
    explicit Impl( std::string n ) : name( std::move( n ) ) {}
    std::string name;
};

MyClass::MyClass( std::string name )
    : PIMPL_MAKE_IMPL( MyClass, std::move( name ) ) {}

MyClass::~MyClass() = default;
```

宏：

| 宏 | 用途 |
| --- | --- |
| `PIMPL_DECLARE(Class)` | 声明 `Impl`、`d_ptr`，以及 `d_func()`（const / 非 const） |
| `PIMPL_MAKE_IMPL(Class, ...)` | 在构造函数**初始化列表**中创建 `Impl` |
| `PIMPL_CONSTRUCT_IMPL(Class, ...)` | 在构造函数**函数体**中创建 `Impl`（延迟/条件创建） |
| `PIMPL_D(Class)` | 在成员函数中声明 `auto *const d = d_func();` |

```cpp
void MyClass::DoWork() {
    PIMPL_D( MyClass );
    d->name += "!";
}
```

> `PIMPL_DECLARE` 会把 `Impl` 与 `d_ptr` 放在 `private:`，把 `d_func()` 放在 `protected:`；
> 析构函数必须在 `.cpp` 中定义（`Impl` 在头文件中不完整）。

## `BreakpadHandler`

头文件：`BreakpadHandler.h`

崩溃转储管理单例，依赖 Google Breakpad 客户端。

```cpp
class BreakpadHandler {
public:
    static BreakpadHandler &Instance();

    void Init();
    void SetDumpRootPath( const std::string &path, bool git_hash_as_dump_path = true );
    void SetMaxSize( std::int64_t max_size );      // MB
    void SetMaxCount( std::uint32_t max_count );
    void GenerateDump();

    std::string              GetDumpRootPath() const;
    std::vector<std::string> GetDumpFileList() const;
    std::int64_t             GetMaxSize() const;
    std::uint32_t            GetMaxCount() const;
};
```

| 方法 | 说明 |
| --- | --- |
| `Instance()` | 全局唯一实例 |
| `Init()` | 安装异常处理器 |
| `SetDumpRootPath(path, git_hash_as_dump_path)` | 设置转储根目录；`true` 时用 Git Hash 作为子目录 |
| `SetMaxSize(mb)` | 转储目录容量上限（MB） |
| `SetMaxCount(n)` | 转储文件数量上限 |
| `GenerateDump()` | 主动生成一次转储 |
| `GetDumpFileList()` | 列出当前转储文件 |

```cpp
auto &handler = BreakpadHandler::Instance();
handler.SetDumpRootPath( "/var/crash/myapp" );
handler.SetMaxSize( 256 );
handler.SetMaxCount( 10 );
handler.Init();
```

- 单例，不可拷贝/移动；
- 通常只需在 `main()` 开头初始化一次；
- 未构建 Breakpad 时该模块不可用（见构建选项）。
