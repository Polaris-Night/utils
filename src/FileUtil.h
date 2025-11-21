#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Macros.h"

namespace utils {

class File {
public:
    // 按位枚举时，取值必须为0或2^n
    enum OpenMode : uint32_t
    {
        NotOpen   = 0x0000,
        ReadOnly  = 0x0001,
        WriteOnly = 0x0002,
        ReadWrite = ReadOnly | WriteOnly,
        Append    = 0x0004,
        Truncate  = 0x0008,
    };
    enum Permissions : uint32_t
    {
        ReadOwner  = 0x4000,
        WriteOwner = 0x2000,
        ExeOwner   = 0x1000,
        ReadGroup  = 0x0040,
        WriteGroup = 0x0020,
        ExeGroup   = 0x0010,
        ReadOther  = 0x0004,
        WriteOther = 0x0002,
        ExeOther   = 0x0001,
        None       = 0x0000,
    };

public:
    explicit File() = default;
    File( const std::string &name );
    virtual ~File();
    DISABLE_COPY_MOVE( File )

    /**
     * @brief 关闭文件
     *
     */
    virtual void Close();
    /**
     * @brief 判断文件是否存在
     *
     * @return true
     * @return false
     */
    bool Exists() const;
    /**
     * @brief 刷新缓冲区
     *
     * @return true
     * @return false
     */
    bool Flush();
    /**
     * @brief 获取文件路径
     *
     * @return std::string
     */
    std::string GetFileName() const;
    /**
     * @brief 获取文件权限
     *
     * @return Permissions
     */
    Permissions GetPermissions() const;
    /**
     * @brief 判断文件是否打开
     *
     * @return true
     * @return false
     */
    bool IsOpened() const;
    /**
     * @brief 打开文件
     *
     * @param mode 打开模式
     * @return true
     * @return false
     */
    virtual bool Open( OpenMode mode );
    /**
     * @brief 移除文件，若文件已打开，将关闭文件
     *
     * @return true
     * @return false
     */
    bool Remove();
    /**
     * @brief 以文本形式读取所有文件内容
     *
     * @return std::string
     */
    std::string ReadAll();
    /**
     * @brief 同步文件数据到磁盘
     *
     * @return true
     * @return false
     */
    bool Sync();
    /**
     * @brief 设置文件路径，未打开文件时使用
     *
     * @param name
     */
    void SetFileName( const std::string &name );
    /**
     * @brief 设置文件权限
     *
     * @param permissions
     * @return true
     * @return false
     */
    bool SetPermissions( Permissions permissions );
    /**
     * @brief 向文件写入数据
     *
     * @param data
     * @param size
     */
    void Write( const char *data, int64_t size );
    /**
     * @brief 向文件写入数据
     *
     * @param data
     */
    void Write( const char *data );

protected:
    int Handle();

private:
    /**
     * @brief 初始化文件权限
     *
     */
    void InitPermission();

private:
    std::string  name_;
    std::string  error_string_;
    Permissions  permissions_{ Permissions::None };
    std::fstream f_;
};

inline File::OpenMode operator|( File::OpenMode __a, File::OpenMode __b ) {
    return File::OpenMode( static_cast<int>( __a ) | static_cast<int>( __b ) );
}

inline File::OpenMode &operator|=( File::OpenMode &__a, File::OpenMode __b ) {
    return __a = static_cast<File::OpenMode>( __a | __b );
}

inline File::OpenMode operator&( File::OpenMode __a, File::OpenMode __b ) {
    return File::OpenMode( static_cast<int>( __a ) & static_cast<int>( __b ) );
}

inline const File::OpenMode &operator&=( File::OpenMode &__a, File::OpenMode __b ) {
    return __a = __a & __b;
}

inline File::Permissions operator|( File::Permissions __a, File::Permissions __b ) {
    return File::Permissions( static_cast<int>( __a ) | static_cast<int>( __b ) );
}

inline File::Permissions &operator|=( File::Permissions &__a, File::Permissions __b ) {
    return __a = static_cast<File::Permissions>( __a | __b );
}

inline File::Permissions operator&( File::Permissions __a, File::Permissions __b ) {
    return File::Permissions( static_cast<int>( __a ) & static_cast<int>( __b ) );
}

inline const File::Permissions &operator&=( File::Permissions &__a, File::Permissions __b ) {
    return __a = __a & __b;
}

template <typename T>
inline bool EnumContainsBit( T enum_value, T enum_bit ) {
    return ( enum_value & enum_bit ) == enum_bit;
}

class FileUtil {
public:
    /**
     * @brief 检查指定路径对应的文件或目录是否存在
     *
     * @param path 文件或目录路径（支持 UTF-8 编码字符串）
     * @return true 路径存在
     * @return false 路径不存在或访问出错（如权限不足、I/O 错误）
     */
    [[nodiscard]] static bool Exists( std::string_view path ) noexcept;

    /**
     * @brief 拼接多个路径片段，生成标准化路径
     *
     * 使用平台兼容的路径分隔符进行连接，并自动处理重复斜杠和相对路径。
     * 内部使用 `std::filesystem::path` 实现，具备跨平台能力。
     *
     * @code
     * auto p = FileUtil::JoinPaths("usr", "local", "bin"); // 结果: "usr/local/bin" (Linux) 或 "usr\\local\\bin"
     * (Windows)
     * @endcode
     *
     * @tparam Args 可变参数类型包（需能转换为 std::filesystem::path 构造参数）
     * @param paths 一个或多个路径片段
     * @return std::string 拼接后的规范化路径字符串
     */
    template <typename... Args>
    static std::string JoinPaths( Args &&...paths ) {
        std::filesystem::path result;
        int                   unpack[] = { 0, ( ( result /= std::forward<Args>( paths ) ), 0 )... };
        (void)unpack;
        return result.string();
    }

    /**
     * @brief 删除指定路径的文件或空目录
     *
     * 若目标是非空目录，则删除失败。
     *
     * @param path 待删除的文件或目录路径
     * @return true 删除成功
     * @return false 删除失败（如路径不存在、目录非空、权限不足）
     */
    [[maybe_unused]] static bool Remove( std::string_view path ) noexcept;

    /**
     * @brief 递归删除文件或目录（包括其所有内容）
     *
     * 支持删除非空目录树。若路径指向文件，则仅删除该文件。
     *
     * @param path 待删除的文件或目录路径
     * @return uint32_t 成功删除的条目数量（文件 + 目录），失败返回 0
     */
    [[maybe_unused]] static uint32_t RemoveAll( std::string_view path ) noexcept;

    /**
     * @brief 清理路径尾部多余的分隔符（'/'）
     *
     * 使用 `std::filesystem::path::string()` 自动规范化路径格式，
     * 包括去除尾部斜杠、合并连续斜杠等。
     *
     * @param path 输入路径字符串
     * @return std::string 规范化后的路径，尾部无多余分隔符
     */
    [[nodiscard]] static std::string CleanPath( std::string_view path ) noexcept;

    /**
     * @brief 以文本模式读取文件全部内容为 std::string（假设为 UTF-8 编码）
     *
     * 适用于中小尺寸文本文件读取。大文件建议使用流式处理。
     *
     * @param file_path 文件路径
     * @return std::string 文件内容；若文件不存在、非普通文件或读取失败则返回空字符串
     */
    [[nodiscard]] static std::string Load2Str( std::string_view file_path ) noexcept;

    /**
     * @brief 以二进制模式读取文件全部内容为字节数组
     *
     * 返回原始字节流，适合图片、音频、序列化数据等二进制文件读取。
     *
     * @param file_path 文件路径
     * @return std::vector<uint8_t> 包含文件所有字节；失败时返回空数组
     */
    [[nodiscard]] static std::vector<uint8_t> Load2ByteArray( std::string_view file_path ) noexcept;

    /**
     * @brief 创建目录，若父目录不存在则递归创建。若路径已存在则不做任何操作。
     *
     * 等价于 shell 命令 `mkdir -p`。
     *
     * @param path 目录路径
     * @return true 创建成功
     * @return false 创建失败（如权限不足、路径非法）或路径已存在
     */
    [[maybe_unused]] static bool CreateDirectories( std::string_view path ) noexcept;

    /**
     * @brief 创建单层目录（不递归创建父目录）
     *
     * 类似于 shell 命令 `mkdir`（无 `-p` 选项）。
     *
     * @param path 目录路径
     * @return true 创建成功
     * @return false 创建失败（如父目录不存在、权限问题）或路径已存在
     */
    [[maybe_unused]] static bool CreateDirectory( std::string_view path ) noexcept;

    /**
     * @brief 拷贝文件或目录（支持递归复制整个目录树）
     *
     * 若目标已存在：
     * - 当 `overwrite == true`：覆盖目标
     * - 当 `overwrite == false`：操作失败
     *
     * @param from 源路径
     * @param to 目标路径
     * @param overwrite 是否允许覆盖已有目标
     * @return true 拷贝成功
     * @return false 拷贝失败（如 I/O 错误、权限不足、目标存在且不允许覆盖）
     */
    [[maybe_unused]] static bool Copy( std::string_view from, std::string_view to, bool overwrite = false ) noexcept;

    /**
     * @brief 移动文件或目录（支持跨设备移动）
     *
     * 若目标路径已存在：
     * - 当 `overwrite == true`：先删除目标再移动
     * - 当 `overwrite == false`：操作失败
     *
     * 注意：在某些系统上 `rename` 不支持跨设备移动，此时会尝试降级为 “拷贝+删除”。
     *
     * @param from 源路径
     * @param to 目标路径
     * @param overwrite 是否允许覆盖目标
     * @return true 移动成功
     * @return false 移动失败
     */
    [[maybe_unused]] static bool Move( std::string_view from, std::string_view to, bool overwrite = false ) noexcept;

    /**
     * @brief 获取路径对应的父目录（不含尾部分隔符）
     *
     * 对于 "/home/user/file.txt" 返回 "/home/user"
     * 对于 "/home/user/" 返回 "/home/user"
     *
     * @param path 文件或目录路径
     * @return std::string 父目录路径；若无法获取则返回空字符串
     */
    [[nodiscard]] static std::string DirName( std::string_view path ) noexcept;

    /**
     * @brief 获取路径中的最后一级名称（即文件名或目录名）
     *
     * 对于 "/home/user/file.txt" 返回 "file.txt"
     * 对于 "/home/user/" 返回 "user"
     *
     * @param path 文件或目录路径
     * @return std::string 最后一级名称；若路径为空或无效则返回空字符串
     */
    [[nodiscard]] static std::string BaseName( std::string_view path ) noexcept;

    /**
     * @brief 获取文件扩展名（包含前导点号）
     *
     * 对于 "document.pdf" 返回 ".pdf"
     * 对于 "archive.tar.gz" 返回 ".gz"（最外层扩展名）
     * 对于 "filename" 返回 ""（无扩展名）
     *
     * @param path 文件路径
     * @return std::string 扩展名字符串；若无扩展名则返回空字符串
     */
    [[nodiscard]] static std::string Extension( std::string_view path ) noexcept;

    /**
     * @brief 获取文件主名称（不含扩展名的部分）
     *
     * 对于 "/home/user/document.pdf" 返回 "document"
     * 对于 "app.exe" 返回 "app"
     *
     * @param path 文件或目录路径
     * @return std::string 主名称；若无法提取则返回空字符串
     */
    [[nodiscard]] static std::string Stem( std::string_view path ) noexcept;

    /**
     * @brief 判断路径是否为常规文件
     *
     * 排除目录、符号链接、设备文件等特殊类型。
     *
     * @param path 路径
     * @return true 是常规文件
     * @return false 非文件或路径不存在
     */
    [[nodiscard]] static bool IsFile( std::string_view path ) noexcept;

    /**
     * @brief 判断路径是否为目录
     *
     * @param path 路径
     * @return true 是目录
     * @return false 非目录或路径不存在
     */
    [[nodiscard]] static bool IsDirectory( std::string_view path ) noexcept;

    /**
     * @brief 获取文件大小（单位：字节）
     *
     * 仅对常规文件有效。目录或其他类型返回 0。
     *
     * @param path 文件路径
     * @return uint64_t 文件大小（字节）；若文件不存在或查询失败则返回 0
     */
    [[nodiscard]] static uint64_t FileSize( std::string_view path ) noexcept;

    /**
     * @brief 列出指定目录下的直接子项名称（仅文件/目录名，不含完整路径）
     *
     * @param path 目录路径
     * @param include_hidden 是否包含隐藏项（Linux/macOS 下以 '.' 开头的条目）
     * @return std::vector<std::string> 子项名称列表；若目录不可访问则返回空列表
     */
    [[nodiscard]] static std::vector<std::string> ListDir( std::string_view path,
                                                           bool             include_hidden = false ) noexcept;

    /**
     * @brief 列出指定目录下的所有直接子项的完整路径
     *
     * @param path 目录路径
     * @param include_hidden 是否包含隐藏项
     * @return std::vector<std::string> 完整路径列表；若目录不可访问则返回空列表
     */
    [[nodiscard]] static std::vector<std::string> ListDirFullPaths( std::string_view path,
                                                                    bool             include_hidden = false ) noexcept;

    /**
     * @brief 判断路径是否为绝对路径
     *
     * Linux/Unix: 以 '/' 开头
     * Windows: 以驱动器字母开头（如 "C:\\..."）或 "\\\\server\\share"
     *
     * @param path 路径字符串
     * @return true 是绝对路径
     * @return false 是相对路径
     */
    [[nodiscard]] static bool IsAbsolutePath( std::string_view path ) noexcept;

    /**
     * @brief 将字符串内容写入文件（文本模式）
     *
     * 默认覆盖写入，可选追加模式。
     *
     * @param file_path 目标文件路径
     * @param content 待写入的字符串内容
     * @param append 是否以追加模式写入
     * @return true 写入成功
     * @return false 写入失败（如路径无效、磁盘满、权限不足）
     */
    [[maybe_unused]] static bool WriteStr( std::string_view file_path, std::string_view content,
                                           bool append = false ) noexcept;

    /**
     * @brief 将字节数组写入文件（二进制模式）
     *
     * 默认覆盖写入，可选追加模式。
     *
     * @param file_path 目标文件路径
     * @param data 待写入的字节数据
     * @param append 是否以追加模式写入
     * @return true 写入成功
     * @return false 写入失败
     */
    [[maybe_unused]] static bool WriteBytes( std::string_view file_path, const std::vector<uint8_t> &data,
                                             bool append = false ) noexcept;

    /**
     * @brief 创建一个唯一的临时文件
     *
     * 文件将被创建为空文件。命名采用高精度时间戳保证唯一性。
     *
     * @param prefix 文件名前缀（默认 "tmp"）
     * @param suffix 文件名后缀（如 ".tmp"，可选）
     * @param dir 存放临时文件的目录（默认为系统临时目录）
     * @return std::string 成功时返回临时文件的完整路径；失败返回空字符串
     */
    [[nodiscard]] static std::string CreateTempFile( std::string_view prefix = "tmp", std::string_view suffix = "",
                                                     std::string_view dir = "" ) noexcept;

    /**
     * @brief 创建一个唯一的临时目录
     *
     * 目录会被创建，但调用者需负责后续清理。
     *
     * @param prefix 目录名前缀（默认 "tmp"）
     * @param parent_dir 父目录路径（默认为系统临时目录）
     * @return std::string 成功时返回临时目录的完整路径；失败返回空字符串
     */
    [[nodiscard]] static std::string CreateTempDirectory( std::string_view prefix     = "tmp",
                                                          std::string_view parent_dir = "" ) noexcept;

private:
    /**
     * @brief 获取系统默认的临时目录路径
     *
     * 优先使用 `std::filesystem::temp_directory_path()`，失败时回退到 `/tmp`（Unix）或 `%TEMP%`（Windows）。
     *
     * @return std::string 临时目录路径；若无法获取则返回 "/tmp"
     */
    [[nodiscard]] static std::string GetSystemTempDir() noexcept;
};
}  // namespace utils
