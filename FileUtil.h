#pragma once

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

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
    File( const File & ) = delete;
    File( File && )      = delete;
    ~File();
    File &operator=( const File & ) = delete;
    File &operator=( File && )      = delete;

    /**
     * @brief 关闭文件
     *
     */
    void Close();
    /**
     * @brief 判断文件是否存在
     *
     * @return true
     * @return false
     */
    bool Exists() const;
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
    bool Open( OpenMode mode );
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
     * @brief 设置文件路径，请在关闭文件状态下使用
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

private:
    /**
     * @brief 初始化文件权限
     *
     */
    void InitPermission();

private:
    std::string  name_;
    std::string  error_string_;
    Permissions  permissions_;
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
     * @brief 检查文件或目录是否存在
     *
     * @param path
     * @return true
     * @return false
     */
    static bool Exists( const std::string &path );
    /**
     * @brief 拼接路径，以'/'进行间隔
     *
     * @tparam Args
     * @param path
     */
    template <typename... Args>
    static std::string JoinPaths( Args... path ) {
        std::ostringstream oss;
        int                unpack[] = { 0, ( oss << '/' << CleanPath( path ), 0 )... };
        (void)unpack;  // 防止编译器警告未使用变量
        return oss.str();
    }
    /**
     * @brief 删除文件或空目录
     *
     * @param path
     * @return true
     * @return false
     */
    static bool Remove( const std::string &path );
    /**
     * @brief 递归删除文件或目录
     *
     * @param path
     * @return uint32_t 删除的文件及目录数量
     */
    static uint32_t RemoveAll( const std::string &path );
    /**
     * @brief 若路径末尾存在'/'，则去除
     *
     * @param path
     * @return std::string
     */
    static std::string CleanPath( const std::string &path );
};
}  // namespace utils
