# 文件与归档

头文件：`FileUtil.h`、`LockedFile.h`、`TarUtil.h` · 命名空间：`utils`

| 组件 | 头文件 | 用途 |
| --- | --- | --- |
| `File` | `FileUtil.h` | 面向对象的文件句柄：打开、读写、权限、同步 |
| `FileUtil` | `FileUtil.h` | 无状态静态工具：路径、读写、复制/移动、临时文件、遍历 |
| `LockedFile` | `LockedFile.h` | 在 `File` 基础上加 POSIX `fcntl` 文件锁 |
| `TarUtil` / `TarObject` | `TarUtil.h` | 基于系统 `tar` 的压缩/解压 |

所有文件系统接口都在失败时返回 `bool` / 空字符串 / 空容器，**不抛异常**；唯一例外是
`WalkDirectory` 的前置条件与遍历错误（见下文）。

## 快速示例

```cpp
#include "FileUtil.h"
#include <iostream>
using namespace utils;

const std::string dir  = FileUtil::JoinPaths( "logs", "2026" );
const std::string path = FileUtil::JoinPaths( dir, "app.log" );

FileUtil::CreateDirectories( dir );
FileUtil::WriteStr( path, "hello\n", /* append = */ false );

File file( path );
if ( file.Open( File::ReadOnly | File::Append ) ) {   // 组合模式
    std::cout << file.ReadAll() << '\n';
    file.Close();
}

for ( const auto &name : FileUtil::ListDir( dir ) ) {
    std::cout << name << '\n';
}
```

## `File`

```cpp
class File {
public:
    enum OpenMode : std::uint32_t {
        NotOpen    = 0x0000,
        ReadOnly   = 0x0001,
        WriteOnly  = 0x0002,
        ReadWrite  = ReadOnly | WriteOnly,
        Append     = 0x0004,
        Truncate   = 0x0008
    };

    enum Permissions : std::uint32_t {
        ReadOwner = 0x4000, WriteOwner = 0x2000, ExeOwner = 0x1000,
        ReadGroup = 0x0040, WriteGroup = 0x0020, ExeGroup = 0x0010,
        ReadOther = 0x0004, WriteOther = 0x0002, ExeOther = 0x0001,
        None      = 0x0000
    };

    File();
    explicit File( const std::string &name );
    virtual ~File();

    bool Open( OpenMode mode );          // virtual
    void Close();                        // virtual
    bool Exists() const;
    bool Flush();
    bool Sync();
    bool IsOpened() const;
    bool Remove();
    bool SetPermissions( Permissions permissions );
    Permissions GetPermissions() const;
    void SetFileName( const std::string &name );
    std::string GetFileName() const;

    std::string ReadAll();
    void Write( const char *data, std::int64_t size );
    void Write( const char *data );      // 以 '\0' 结尾
};
```

### 打开模式

`OpenMode` 是位标志，用 `|` 组合：

| 组合 | 含义 |
| --- | --- |
| `ReadOnly` | 只读，文件必须存在 |
| `WriteOnly` | 只写，文件不存在则创建 |
| `WriteOnly \| Truncate` | 截断后写入 |
| `ReadWrite` | 可读可写 |
| `Append` | 追加；可与 `WriteOnly` / `ReadWrite` 组合 |

```cpp
File f( "data.bin" );
f.Open( File::WriteOnly | File::Truncate );
f.Write( "abc", 3 );
f.Close();

f.Open( File::ReadOnly );
std::string all = f.ReadAll();
```

- 不传 `Append` 且不传 `Truncate` 时，`WriteOnly` / `ReadWrite` 的初始位置在文件开头；
- `Append | Truncate` 是非法组合，`Open` 直接返回 `false`；
- `ReadWrite` 打开不存在的文件时会先创建一个空文件；
- `Write(const char *)` 按 `\0` 结尾计算长度，**不能写含 `\0` 的二进制**；
  二进制请用 `Write(data, size)`。

### 生命周期与错误

- 析构自动 `Close()`；
- `Remove()` 会先关闭再删除；文件不存在返回 `false`；
- `SetFileName` 只在**未打开**时生效；
- `ReadAll()` 会先 `seekg` 到文件开头，再读整个文件；空文件 / 打开失败返回空串；
- `Write` 失败不抛异常，也不返回状态；需要确认落盘请调用 `Flush()` / `Sync()`：
  - `Flush()` 刷新用户态缓冲；
  - `Sync()` 刷新到设备（更重）。

### 权限

`Permissions` 采用 POSIX 风格的 4 位八进制位（属主/组/其他）。`GetPermissions()` 在非 POSIX 平台
可能返回 `None`。

```cpp
File f( "secret.txt" );
f.Open( File::WriteOnly | File::Truncate );
f.SetPermissions( File::ReadOwner | File::WriteOwner );   // 0600
```

位运算与判断：

```cpp
File::OpenMode mode = File::ReadOnly | File::WriteOnly;
bool writable = EnumContainsBit( mode, File::WriteOnly );
```

`|`、`|=`、`&`、`&=` 已对 `OpenMode` 和 `Permissions` 重载。

## `FileUtil`

全部为 `static`，`noexcept`（`JoinPaths` 除外，它可能分配内存）。

### 路径

```cpp
static bool        Exists( std::string_view path ) noexcept;
static std::string CleanPath( std::string_view path ) noexcept;
static bool        IsAbsolutePath( std::string_view path ) noexcept;
static std::string DirName( std::string_view path ) noexcept;
static std::string BaseName( std::string_view path ) noexcept;
static std::string Extension( std::string_view path ) noexcept;
static std::string Stem( std::string_view path ) noexcept;

template <typename... Args>
static std::string JoinPaths( Args &&...paths );
```

| 函数 | 示例 | 结果 |
| --- | --- | --- |
| `CleanPath` | `"a/b///"` | `"a/b"` |
| `DirName` | `"a/b/c.txt"` | `"a/b"` |
| `BaseName` | `"a/b/c.txt"` | `"c.txt"` |
| `Extension` | `"a/b/c.tar.gz"` | `".gz"` |
| `Stem` | `"a/b/c.tar.gz"` | `"c.tar"` |
| `JoinPaths` | `("a", "b", "c.txt")` | `"a/b/c.txt"` |

- `JoinPaths` 使用 `std::filesystem::path`，跨平台；
- `CleanPath` 保留根路径（`"/"` 不会变成空串）；
- `Extension` 返回**最外层**扩展名（含点）；无扩展名返回空串。

### 读取与写入

```cpp
static std::string              Load2Str( std::string_view file_path ) noexcept;
static std::vector<std::uint8_t> Load2ByteArray( std::string_view file_path ) noexcept;

static bool WriteStr( std::string_view file_path, std::string_view content,
                      bool append = false ) noexcept;
static bool WriteBytes( std::string_view file_path, const std::vector<std::uint8_t> &data,
                        bool append = false ) noexcept;
```

- `Load2Str` / `Load2ByteArray` 只接受常规文件；不存在、目录、读取失败都返回空；
- `WriteStr` / `WriteBytes` 不会自动创建父目录，需先 `CreateDirectories`；
- `append = false` 覆盖写（等价于截断），`true` 追加。

```cpp
FileUtil::CreateDirectories( "out" );
FileUtil::WriteStr( "out/a.txt", "line\n" );
FileUtil::WriteStr( "out/a.txt", "next\n", true );   // 追加
```

### 目录

```cpp
static bool CreateDirectory ( std::string_view path ) noexcept;   // 单层
static bool CreateDirectories( std::string_view path ) noexcept;  // mkdir -p

static bool Remove   ( std::string_view path ) noexcept;          // 文件或空目录
static std::uint32_t RemoveAll( std::string_view path ) noexcept; // 递归

static bool Copy( std::string_view from, std::string_view to, bool overwrite = false ) noexcept;
static bool Move( std::string_view from, std::string_view to, bool overwrite = false ) noexcept;

static bool IsFile( std::string_view path ) noexcept;
static bool IsDirectory( std::string_view path ) noexcept;
static std::uint64_t FileSize( std::string_view path ) noexcept;

static std::vector<std::string> ListDir( std::string_view path,
                                         bool include_hidden = false ) noexcept;
static std::vector<std::string> ListDirFullPaths( std::string_view path,
                                                  bool include_hidden = false ) noexcept;
```

| 函数 | 说明 |
| --- | --- |
| `Remove` | 文件或**空目录**；非空目录失败返回 `false` |
| `RemoveAll` | 递归删除，返回成功删除的条目数；路径不存在返回 `0` |
| `Copy` | `overwrite=false` 且目标存在时失败；目录复制为递归 |
| `Move` | 优先 `rename`；跨设备失败时降级为“拷贝 + 删除源” |
| `FileSize` | 仅常规文件；失败返回 `0` |
| `ListDir` | 只返回名称（非完整路径），不递归 |
| `ListDirFullPaths` | 返回完整路径，不递归 |

`include_hidden=false` 时跳过以 `.` 开头的条目。

### 临时文件与目录

```cpp
static std::string CreateTempFile( std::string_view prefix = "tmp",
                                   std::string_view suffix = "",
                                   std::string_view dir = "" ) noexcept;

static std::string CreateTempDirectory( std::string_view prefix = "tmp",
                                        std::string_view parent_dir = "" ) noexcept;
```

- 原子独占创建，避免竞态；
- 成功返回完整路径，失败返回空串；
- `dir` / `parent_dir` 为空时使用系统临时目录；
- 调用方负责清理。

```cpp
std::string tmp = FileUtil::CreateTempFile( "utils-", ".log" );
if ( !tmp.empty() ) {
    FileUtil::WriteStr( tmp, "debug\n" );
    FileUtil::Remove( tmp );
}
```

### 目录遍历

```cpp
void WalkDirectory(
    const std::string &root_path,
    std::function<void( const std::string &,
                        const std::filesystem::directory_entry & )> callback,
    bool recursive = false,
    const std::vector<std::string> &include_files = {},
    const std::vector<std::string> &exclude_files = {},
    const std::vector<std::string> &include_extensions = {},
    const std::vector<std::string> &exclude_extensions = {} );
```

**注意：`WalkDirectory` 是 `FileUtil` 中唯一的非静态成员**，需通过实例调用：

```cpp
FileUtil util;
util.WalkDirectory( "src", []( const std::string &path,
                               const std::filesystem::directory_entry &entry ) {
    std::cout << path << '\n';
}, /* recursive = */ true, {}, {}, { ".h", ".cpp" }, { ".bak" } );
```

- `root_path` 不是目录时抛 `std::invalid_argument`；
- 遍历过程中的访问错误抛 `std::runtime_error`；
- 不进入**指向目录的符号链接**（避免环）；
- `include_files` / `exclude_files` 按文件名匹配；`include_extensions` / `exclude_extensions` 按扩展名匹配（带点）；
- 过滤规则为空表示“不过滤”。

## `LockedFile`

```cpp
class LockedFile final : public File {
public:
    enum LockMode : std::uint32_t {
        NoLock = 0,
        ReadLock,
        WriteLock
    };

    LockedFile();
    explicit LockedFile( const std::string &name );
    ~LockedFile();

    bool Open( OpenMode mode ) override;

    bool Lock( LockMode mode, bool is_block = true );
    bool Unlock();
    bool IsLocked() const;
    LockMode GetLockMode() const;
};
```

基于 POSIX `fcntl` 的**进程级**建议锁：

- `ReadLock` 为共享锁，`WriteLock` 为排他锁；
- `is_block = true` 时阻塞等待，`false` 时立即返回失败；
- 析构自动解锁并关闭；
- 锁只对**同一文件的不同进程**生效；同进程内多个 `LockedFile` 不互斥。

```cpp
#include "LockedFile.h"

LockedFile file( "data.db" );
if ( file.Open( File::ReadWrite ) && file.Lock( LockedFile::WriteLock ) ) {
    file.Write( "update", 6 );
    file.Unlock();
}
```

## `TarUtil` / `TarObject`

```cpp
struct TarResult {
    bool        success = false;
    std::string message;
    explicit operator bool() const;
};

enum TarFormat {
    kGzip = 0, kBzip2, kXz, kZstd, kRaw, kUnknown
};
```

`TarObject` 用于“收集文件列表后压缩”：

```cpp
class TarObject {
public:
    TarObject();
    TarObject( const std::string &tar_path,
               const std::initializer_list<std::string> &init );

    void SetTarPath( const std::string &tar_path );
    void AddFile( const std::string &file );
    void AddFile( const std::vector<std::string> &files );
    void AddExclude( const std::string &pattern );
    void AddExclude( const std::vector<std::string> &patterns );
    void RemoveFile( const std::string &file );
    void RemoveExclude( const std::string &pattern );
    void ClearFile();
    void ClearExclude();

    TarResult Compress() const;

    std::string            GetTarPath() const;
    std::set<std::string>  GetFileList() const;
    std::size_t            GetFileCount() const;
    bool                   TarExist() const;
};
```

```cpp
TarObject tar( "logs.tar.gz", { "logs" } );
tar.AddExclude( "*.tmp" );

if ( auto result = tar.Compress() ) {          // explicit operator bool
    std::cout << "ok\n";
}
else {
    std::cerr << result.message << '\n';
}
```

静态便捷接口：

```cpp
static TarResult TarUtil::Compress( const std::string &tar_path,
                                    const std::set<std::string> &source_list );
static TarResult TarUtil::Decompress( const std::string &tar_path,
                                      const std::string &dec_path );
```

- `Compress` 固定使用 `tar -czf`（**gzip**）；目标扩展名不会改变压缩格式，建议使用 `.tar.gz`；
- `Compress` 对绝对路径会以“父目录 + 文件名”入包，归档内为相对路径；
- `Decompress` 使用 `tar -xf`，由 tar 自动识别 gzip/bzip2/xz/zstd；
- `Decompress` 会**自动创建** `dec_path`（若不为空）；
- 失败原因在 `TarResult::message`；
- `TarFormat` 枚举为格式标注预留，当前压缩接口不读取它。

## 线程安全与平台

- `File` / `LockedFile` **非线程安全**：同一句柄的多线程读写需外部同步。
- `FileUtil` 的静态函数本身无共享状态，可并发调用；但并发写同一路径的结果取决于文件系统。
- `LockedFile`、`TarUtil` 仅在 Linux / POSIX 验证；Windows 未验证。
- 路径使用 `std::filesystem::path`，但返回值为 `std::string`（本地编码）。
- 所有 `FileUtil` 接口不会抛异常（`JoinPaths` 可能因内存分配抛 `std::bad_alloc`）。

## 行为边界速查

| 场景 | 行为 |
| --- | --- |
| `Open` 一个不存在的只读文件 | `false` |
| `ReadAll` 在未打开时 | 空串 |
| `Write(const char*)` 写入含 `\0` 的数据 | 在第一个 `\0` 处截断 |
| `Remove` 非空目录 | `false` |
| `RemoveAll` 路径不存在 | `0` |
| `Copy` 目标存在且 `overwrite=false` | `false` |
| `Move` 跨设备 | 拷贝后删除源 |
| `FileSize` 对目录 | `0` |
| `ListDir` 对不可访问目录 | 空列表 |
| `CreateTempFile` 失败 | 空串 |
| `WalkDirectory` 的 root 不是目录 | 抛 `std::invalid_argument` |
| `WalkDirectory` 遍历出错 | 抛 `std::runtime_error` |
| `TarUtil` 失败 | `TarResult.success == false`，原因在 `message` |
