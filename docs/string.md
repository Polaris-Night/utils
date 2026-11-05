# 字符串处理

头文件：`StringUtil.h` · 命名空间：`utils`

该头文件提供三个组件：

| 组件 | 用途 |
| --- | --- |
| `StringUtil` | 无状态的静态字符串工具：切分、替换、裁剪、数字互转、正则提取、命名风格转换 |
| `StringFormatter` | `%1`、`%2` 占位符风格的链式格式化 |
| `StringParser` | 基于 `name={name},age={age}` 模板规则的文本解析 |

## 类型萃取

```cpp
template <typename T> struct CanConvertToString;
template <typename... Args> struct IsStringType;      // 所有参数都能转成字符串
template <typename... Args> inline constexpr bool IsStringType_v = IsStringType<Args...>::value;
```

```cpp
static_assert( IsStringType_v<std::string, const char *, std::string_view> );
static_assert( CanConvertToString<std::string_view>::value );
```

`IsStringType_v` 要求每个参数都能转换为 `std::string` **或** `std::string_view`；算术类型不满足
（数字请用 `std::to_string` 或 `StringFormatter`）。

## 快速示例

```cpp
#include "StringUtil.h"
#include <iostream>
using namespace utils;

auto fields = StringUtil::Split( "a,b,,c", "," );            // {"a","b","","c"}
auto clean  = StringUtil::Split( "a,b,,c", ",", true );      // {"a","b","c"}

std::string text = "  Hello, World  ";
std::cout << '[' << StringUtil::Trim( text ) << "]\n";        // [Hello, World]

auto n = StringUtil::ToNumber<int>( "0x2A" );                 // 42（自动识别 0x）
auto f = StringUtil::ToNumber<double>( "3.14" );              // 3.14

std::string s = StringUtil::Join( ", ", std::string( "a" ), std::string( "b" ) );
// "a, b"（可变参数版本只接受可转成 string / string_view 的参数）

StringFormatter fmt( "name=%1, age=%2" );
std::cout << fmt.Args( "Tom", 18 ).ToString() << '\n';        // name=Tom, age=18
```

## 切分

```cpp
static std::vector<std::string> Split( std::string_view str,
                                       std::string_view separator,
                                       bool skip_empty = false,
                                       bool each_char_as_separator = false ) noexcept;

static std::vector<std::string_view> SplitRef( std::string_view str,
                                               std::string_view separator,
                                               bool skip_empty = false,
                                               bool each_char_as_separator = false ) noexcept;
```

| 参数 | 说明 |
| --- | --- |
| `separator` | 为空时**逐字符**切分，并在首尾各加一个空串（`skip_empty=true` 时省略） |
| `skip_empty` | 丢弃空片段 |
| `each_char_as_separator` | `true` 时把 `separator` 中**每个字符**都当作独立分隔符，如 `",;"` |

```cpp
StringUtil::Split( "a,b,c", "," );          // {"a","b","c"}
StringUtil::Split( "a;b,c", ",;", false, true );  // {"a","b","c"}
StringUtil::Split( "abc", "" );             // {"","a","b","c",""}
StringUtil::SplitRef( "a,b", "," );         // vector<string_view>，零拷贝
```

> `SplitRef` 返回的 `string_view` 指向输入缓冲区，调用方需保证原字符串生命周期。

## 替换与裁剪

```cpp
static std::string ReplaceFirst( std::string_view str, std::string_view from, std::string_view to ) noexcept;
static std::string ReplaceAll  ( std::string_view str, std::string_view from, std::string_view to ) noexcept;
static std::string Trim( std::string_view str ) noexcept;
```

- `ReplaceFirst` / `ReplaceAll`：`from` 为空时返回原串；`ReplaceAll` 不会因替换文本包含 `from` 而循环。
- `Trim`：去除两端的 ` `、`\n`、`\r`、`\t`、`\v`、`\f`。

```cpp
StringUtil::ReplaceAll( "a.b.c", ".", "/" );   // "a/b/c"
StringUtil::Trim( "\t x \n" );                 // "x"
```

## 前后缀与大小写

```cpp
static std::string ToUpper( std::string_view str );
static std::string ToLower( std::string_view str );
static bool StartWith( std::string_view str, std::string_view prefix ) noexcept;
static bool EndWith  ( std::string_view str, std::string_view suffix ) noexcept;
static bool Contains ( std::string_view str, std::string_view needle ) noexcept;
static bool EqualsIgnoreCase( std::string_view a, std::string_view b ) noexcept;
static bool IsNumeric( std::string_view str ) noexcept;
static bool IsUpper( std::string_view str ) noexcept;
static bool IsLower( std::string_view str ) noexcept;
```

```cpp
StringUtil::StartWith( "hello.log", "hello" );   // true
StringUtil::EqualsIgnoreCase( "ABC", "abc" );    // true
StringUtil::IsNumeric( "12.5" );                 // true
```

> `IsUpper` / `IsLower` 要求**每个字符**都满足对应大小写（`std::isupper` / `std::islower`）；
> 空串、含数字、含符号都会返回 `false`。

## 子串

```cpp
static std::string      Left ( std::string_view str, std::size_t len );
static std::string_view LeftRef( std::string_view str, std::size_t len ) noexcept;
static std::string      Mid  ( std::string_view str, std::size_t pos, std::size_t len = npos );
static std::string_view MidRef( std::string_view str, std::size_t pos, std::size_t len = npos ) noexcept;
static std::string      Right( std::string_view str, std::size_t len );
static std::string_view RightRef( std::string_view str, std::size_t len ) noexcept;
```

越界规则：

- `Left` / `Right` 的 `len` 超过字符串长度时返回整串；
- `Right` 的 `len == 0` 返回空串；
- `Mid` 的 `pos` 超过长度返回空串；`len` 超过剩余长度时截到末尾。

```cpp
StringUtil::Left( "abcdef", 3 );      // "abc"
StringUtil::Right( "abcdef", 2 );     // "ef"
StringUtil::Mid( "abcdef", 2, 3 );    // "cde"
```

## 拼接与重复

```cpp
template <typename... Args>
static std::string Join( std::string_view separator, Args &&...args );

template <typename Container>
static std::string Join( const Container &container, std::string_view separator );
```

```cpp
StringUtil::Join( ", ", std::string( "a" ), std::string( "b" ) );  // "a, b"
StringUtil::Join( std::vector<std::string>{ "x", "y" }, "-" );     // "x-y"
StringUtil::Join( std::vector<std::string>{}, "-" );              // ""（空容器）
```

- 可变参数版本要求至少 1 个参数，且全部**可转换为 `std::string` 或 `std::string_view`**；
  整数/浮点不能直接传入，请先 `std::to_string` 或使用 `StringFormatter`；
- 容器版本对空容器返回空串；元素需支持 `std::string::operator+=`（即字符串类）。

```cpp
static std::string Repeat( std::string_view str, unsigned times );
```

```cpp
StringUtil::Repeat( "ab", 3 );   // "ababab"
StringUtil::Repeat( "ab", 0 );   // ""
```

## 补位、随机与命名风格

```cpp
static std::string PadLeft ( std::string_view str, std::size_t width, char fill = ' ' );
static std::string PadRight( std::string_view str, std::size_t width, char fill = ' ' );
static std::string RandomString( std::size_t length,
                                 std::string_view charset = "a-zA-Z0-9" );

static std::string CamelToSnake( std::string_view str );
static std::string SnakeToCamel( std::string_view str, bool upper_first = false );
```

`RandomString` 的 `charset` 是**字面字符集合**，每个字符都可能出现在结果中（不支持 `a-z` 这类区间语法）：

```cpp
StringUtil::RandomString( 6, "0123456789" );    // 纯数字
StringUtil::RandomString( 4, "ab" );            // 只含 a / b
```

> 默认 `charset` 为字面量 `"a-zA-Z0-9"`，因此默认结果只可能包含
> `a`、`z`、`A`、`Z`、`0`、`9`、`-` 这 7 个字符。需要完整字母数字集合时请显式传入：
>
> ```cpp
> constexpr std::string_view kAlnum =
>     "abcdefghijklmnopqrstuvwxyz"
>     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
>     "0123456789";
> auto token = StringUtil::RandomString( 16, kAlnum );
> ```
>
> 使用 `std::random_device` + `thread_local std::mt19937`，无需手动播种；`charset` 为空或
> `length == 0` 时返回空串。

补位与命名风格示例：

```cpp
StringUtil::PadLeft( "42", 5, '0' );               // "00042"
StringUtil::PadRight( "ab", 4, '.' );              // "ab.."
StringUtil::CamelToSnake( "HttpServer" );          // "http_server"
StringUtil::SnakeToCamel( "http_server" );         // "HttpServer"
StringUtil::SnakeToCamel( "http_server", false );  // "httpServer"
```

## C 风格转义

```cpp
static std::string EscapeC( std::string_view str );
static std::string UnescapeC( std::string_view str );
```

- `EscapeC`：转义 `\n`、`\r`、`\t`、`\\`、`\"`、`\'`；不可打印字符与非 ASCII 字节逐字节输出 `\xHH`。
- `UnescapeC`：识别 `\n`、`\r`、`\t`、`\\`、`\"`、`\'`、`\xHH`；未知转义保留原样；**不支持八进制与 `\0`**。

```cpp
StringUtil::EscapeC( "a\nb" );        // "a\\nb"
StringUtil::UnescapeC( "a\\x41b" );   // "aAb"
```

## 通配符与字节单位

```cpp
static bool WildcardMatch( std::string_view str, std::string_view pattern ) noexcept;
```

`?` 匹配单个字符，`*` 匹配任意长度（含空）字符串；其余字符按字面量匹配。

```cpp
StringUtil::WildcardMatch( "app.log", "*.log" );   // true
StringUtil::WildcardMatch( "a1", "a?" );           // true
```

```cpp
static double ConvertByteUnit( double value,
                               std::string_view from_unit,
                               std::string_view to_unit ) noexcept;

static std::string HumanizeBytes( std::uint64_t bytes,
                                  int precision = 2,
                                  std::string_view target_unit = {} );
```

- 单位：`B`、`KB`、`MB`、`GB`、`TB`，不区分大小写，按 **1024** 进制。
- `ConvertByteUnit` 遇未知单位返回 `-1.0`。
- `HumanizeBytes` 的 `precision` 取值 0–6；`target_unit` 为空时自动选择最合适单位。

```cpp
StringUtil::ConvertByteUnit( 1, "MB", "KB" );     // 1024
StringUtil::HumanizeBytes( 1536 );                // "1.50 KB"（默认 precision=2）
StringUtil::HumanizeBytes( 1536, 1 );             // "1.5 KB"
StringUtil::HumanizeBytes( 1536, 0 );             // "2 KB"
StringUtil::HumanizeBytes( 1048576, 2, "MB" );    // "1.00 MB"
```

## 数字互转

<a id="tonumber--fromnumber"></a>
```cpp
template <typename T>
static std::optional<T> ToNumber( std::string_view text );

template <typename T>
static std::string FromNumber( T value, int base = 10, int precision = -1 );
```

### `ToNumber<T>`

仅支持算术类型（整型 / 浮点），规则：

1. 先 `Trim`；
2. 整型使用 `strtoll` / `strtoull`，**base 0**（自动识别 `10`、`0x`、`0` 八进制）；
3. 无符号类型拒绝负号；所有整型做范围检查；
4. 浮点使用 `strtof` / `strtod` / `strtold`，拒绝 `inf` / `nan`；
5. 必须**完整匹配**，空串、尾部多余字符、部分匹配都返回 `nullopt`。

```cpp
StringUtil::ToNumber<int>( "42" );        // 42
StringUtil::ToNumber<int>( "0x2A" );      // 42
StringUtil::ToNumber<int>( "042" );       // 34（八进制）
StringUtil::ToNumber<int>( " 42 " );      // 42
StringUtil::ToNumber<int>( "42abc" );     // nullopt
StringUtil::ToNumber<unsigned>( "-1" );   // nullopt
StringUtil::ToNumber<double>( "3.14" );   // 3.14
```

### `FromNumber<T>`

- 整型：`base` 只接受 `8` / `10` / `16`，其他值返回空串；
- 浮点：`precision >= 0` 使用 `std::fixed` 与指定小数位，`-1` 使用 `std::defaultfloat`；
- 失败（如格式化异常）返回空串。

```cpp
StringUtil::FromNumber( 255, 16 );          // "ff"
StringUtil::FromNumber( 255, 2 );           // ""（不支持二进制）
StringUtil::FromNumber( 3.14159 );          // "3.14159"
StringUtil::FromNumber( 3.14159, 10, 2 );   // "3.14"
```

## 正则提取

```cpp
static std::optional<std::string> ExtractBetween( std::string_view str,
                                                  std::string_view start,
                                                  std::string_view end,
                                                  bool include_start = false,
                                                  bool include_end = false );

static std::optional<std::string> ExtractFirst( std::string_view text,
                                                std::string_view pattern,
                                                std::string *error_msg = nullptr );

static std::optional<std::string> ExtractGroup( std::string_view text,
                                                std::string_view pattern,
                                                int group_index = 1,
                                                std::string *error_msg = nullptr );

static std::vector<std::string> ExtractAll( std::string_view text,
                                            std::string_view pattern,
                                            std::string *error_msg = nullptr );

static std::vector<std::string> ExtractAllGroups( std::string_view text,
                                                  std::string_view pattern,
                                                  int group_index = 1,
                                                  std::string *error_msg = nullptr );
```

- 使用 `std::regex` 的 **ECMAScript** 语法；
- 正则编译/执行错误写入 `error_msg`（若不为 `nullptr`），并返回 `nullopt` / 空容器；
- `ExtractBetween` 是纯字符串查找，不涉及正则；找不到返回 `nullopt`。

```cpp
StringUtil::ExtractFirst( "id=42", R"(id=(\d+))" );     // "42"
StringUtil::ExtractGroup( "id=42", R"(id=(\d+))", 1 ); // "42"
StringUtil::ExtractAll( "a1 b2", R"([a-z]\d)" );        // {"a1","b2"}

std::string err;
auto v = StringUtil::ExtractFirst( "x", "(", &err );    // nullopt，err 含正则错误
```

## 编码与位串

```cpp
static std::string ConvertToHexStr( std::string_view data, char separator = ' ' );
static std::string IntToBitString( std::uint64_t value, int count );
```

```cpp
StringUtil::ConvertToHexStr( "AB" );          // "41 42"
StringUtil::ConvertToHexStr( "AB", ':' );     // "41:42"
StringUtil::ConvertToHexStr( "AB", '\0' );    // 字节之间插入 NUL（不推荐）
StringUtil::IntToBitString( 5, 4 );           // "0101"
StringUtil::IntToBitString( 5, 0 );           // ""
```

`separator` 会**原样**插入每两个十六进制字节之间；传 `'\0'` 会得到含内嵌 NUL 的字符串。

`IntToBitString` 的 `count` 会被钳制到 `[0, 64]`；`count > 64` 按 64 处理。

## C 风格格式化

```cpp
template <typename... Args>
static std::string FormatCString( std::string_view format, Args &&...args );
```

使用 `snprintf` 两遍（先测长、再写入）。格式化失败（含参数不匹配）抛 `std::runtime_error`。

```cpp
StringUtil::FormatCString( "%s=%d", "x", 42 );   // "x=42"
```

> 这是库中少见的**会抛异常**的工具函数；热路径或不可抛环境请改用 `StringFormatter`。

## `StringFormatter`

```cpp
class StringFormatter {
public:
    explicit StringFormatter( std::string_view format );

    template <typename... TArgs>
    StringFormatter &Args( TArgs &&...args );

    template <typename... FloatArgs>
    StringFormatter &ArgsF( int precision, FloatArgs &&...args );

    std::string ToString() const noexcept;
};
```

### 占位符规则

- 占位符为 `%1`、`%2`……按 `Args` 调用顺序依次替换；
- `%1` 后紧跟数字时按更长编号处理（如 `%10` 是第 10 个参数，不是“第 1 个参数后跟 0”）；
- 参数不足时，剩余占位符**原样保留**；
- 多余参数被忽略。

### `Args` 的类型转换

| 类型 | 输出 |
| --- | --- |
| `bool` | `"true"` / `"false"` |
| 整型 | `std::to_string` |
| 浮点 | `std::defaultfloat`，精度 6 |
| 字符串 / `string_view` / `const char*` | 原样 |

### `ArgsF`

只接受浮点参数，使用 `std::fixed` 与指定小数位：

```cpp
StringFormatter( "pi=%1" ).ArgsF( 2, 3.14159 ).ToString();   // "pi=3.14"
```

### 示例

```cpp
StringFormatter( "%1 + %2 = %3" ).Args( 1, 2, 3 ).ToString();      // "1 + 2 = 3"
StringFormatter( "ok=%1" ).Args( true ).ToString();                // "ok=true"
StringFormatter( "x=%1" ).Args( 1.23456789 ).ToString();           // "x=1.23457"
StringFormatter( "%1 %2" ).Args( "only" ).ToString();              // "only %2"
```

> `StringFormatter` 不支持 `%%` 转义、宽度/对齐/进制等 printf 特性。需要这些请用 `FormatCString`。

## `StringParser`

```cpp
class StringParser {
public:
    class MatchResult {
    public:
        const std::string &Get( const std::string &name ) const;
        template <typename T> std::optional<T> GetAs( const std::string &name ) const;
    };

    StringParser();
    ~StringParser();

    StringParser &AddRule( const std::string &rule );
    std::vector<MatchResult> Parse( const std::string &text,
                                    std::string *error_msg = nullptr ) const;
};
```

### 规则语法

规则是“字面量 + `{占位符}`”的混合模板：

```
name={name},age={age}
[level] {level} at {time}
```

- 占位符名必须以**字母**开头，可含字母、数字、下划线；
- 未闭合的 `{` 或非法名称在 `AddRule` 阶段抛 `std::invalid_argument`；
- 字面量中的正则特殊字符会被自动转义；字面量空格被转成 `\s`（匹配任意空白）；
- 捕获值模式为 `([^,;]*[^,;\s])`：遇到 `,` 或 `;` 停止，并自动去除尾随空白。

### `Parse`

- 返回**所有规则的、所有非重叠匹配**，按规则注册顺序、文本中出现顺序排列；
- 编译或执行错误写入 `error_msg` 并返回空 `vector`；
- 同一个 `StringParser` 可重复调用 `Parse`，已编译的正则被复用，捕获组不会累积。

### `MatchResult`

```cpp
const std::string &Get( const std::string &name ) const;   // 不存在返回空串引用
```

```cpp
template <typename T>
std::optional<T> GetAs( const std::string &name ) const;
```

`GetAs<T>` 支持：

| `T` | 规则 |
| --- | --- |
| `std::string` | 返回 `Trim` 后的值 |
| `bool` | `1/true/on/yes` → `true`；`0/false/off/no` → `false`（不区分大小写） |
| 整型 / 浮点 | 走 `StringUtil::ToNumber<T>` |

值为空或转换失败返回 `nullopt`。

### 示例

```cpp
StringParser parser;
parser.AddRule( "name={name},age={age}" )
      .AddRule( "score={score}" );

auto matches = parser.Parse( "name=Tom,age=18\nname=Jerry,age=20\nscore=95.5" );

for ( const auto &m : matches ) {
    if ( auto name = m.GetAs<std::string>( "name" ) ) {
        std::cout << *name << '\n';
    }
}

// 第一条匹配：Get("name") == "Tom"，GetAs<int>("age") == 18
// 第三条匹配：GetAs<double>("score") == 95.5
```

### 错误处理示例

```cpp
StringParser parser;
try {
    parser.AddRule( "id={1id}" );          // 非法占位符名
}
catch ( const std::invalid_argument &e ) {
    std::cerr << "规则非法: " << e.what() << '\n';
}

std::string err;
auto result = parser.Parse( "id=42", &err );   // 无规则时返回空
if ( !err.empty() ) {
    std::cerr << "解析错误: " << err << '\n';
}
```

## 线程安全与性能

- `StringUtil` 全部为静态无状态函数，可并发调用（除依赖全局 locale 的格式化外）。
- `StringFormatter` 非线程安全；`ToString()` 为 `noexcept` 且不修改状态，可多次调用。
- `StringParser` 非线程安全；`Parse` 为 `const`，内部编译结果在首次使用时缓存，多个线程共享同一实例仍需外部同步。
- `SplitRef` / `LeftRef` / `MidRef` 等返回 `string_view`，避免拷贝，但要求源字符串存活。
- `FormatCString` 需要两遍 `snprintf`，比 `StringFormatter` 慢，但支持完整 printf 语法。

## 行为边界速查

| 场景 | 行为 |
| --- | --- |
| `Split` 的 `separator` 为空 | 逐字符切分，首尾插入空串（`skip_empty=true` 时省略） |
| `ReplaceAll` 的 `from` 为空 | 返回原串 |
| `Trim` 的字符集 | 空格、`\n`、`\r`、`\t`、`\v`、`\f` |
| `ToNumber<int>("042")` | `34`（八进制，base 0 语义） |
| `ToNumber<unsigned>("-1")` | `nullopt` |
| `ToNumber<double>("inf")` | `nullopt` |
| `FromNumber(255, 2)` | 空串（只支持 8/10/16） |
| `FormatCString` 失败 | 抛 `std::runtime_error` |
| `StringFormatter` 参数不足 | 占位符原样保留 |
| `StringParser::Get` 的键不存在 | 返回空串引用 |
| `StringParser::AddRule` 规则非法 | 抛 `std::invalid_argument` |
| `StringParser::Parse` 正则错误 | `error_msg` 写入详情，返回空列表 |
