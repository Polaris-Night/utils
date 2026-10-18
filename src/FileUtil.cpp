#include "FileUtil.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <stack>

#if defined( PLATFORM_OS_WINDOWS )
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

// 生成 64 位随机十六进制串，用于临时文件/目录命名
std::string RandomToken() {
    thread_local static std::mt19937_64 gen{ std::random_device{}() };
    char                                buf[17] = {};
    std::snprintf( buf, sizeof( buf ), "%016llx", static_cast<unsigned long long>( gen() ) );
    return buf;
}

}  // namespace

namespace utils {

File::File( const std::string &name ) : name_( name ) {
    InitPermission();
}

File::~File() {
    if ( IsOpened() ) {
        Sync();
        Close();
    }
}

void File::Close() {
    f_.close();
}

bool File::Exists() const {
    return FileUtil::Exists( name_ );
}

bool File::Flush() {
    if ( !IsOpened() ) {
        return false;
    }
    try {
        f_.flush();
        return true;
    }
    catch ( const std::ios_base::failure & ) {
        return false;
    }
}

std::string File::GetFileName() const {
    return name_;
}

File::Permissions File::GetPermissions() const {
    return permissions_;
}

bool File::IsOpened() const {
    return f_.is_open();
}

bool File::Open( OpenMode mode ) {
    std::ios::openmode open_mode{};
    auto               CalcMode = [&mode, &open_mode]( OpenMode m, std::ios::openmode append_value ) {
        if ( EnumContainsBit( mode, m ) ) {
            open_mode |= append_value;
        }
    };
    CalcMode( OpenMode::ReadOnly, std::ios::in );
    CalcMode( OpenMode::WriteOnly, std::ios::out );
    CalcMode( OpenMode::Append, std::ios::app );
    CalcMode( OpenMode::Truncate, std::ios_base::trunc );
    // 检查是否冲突
    if ( EnumContainsBit( open_mode, std::ios::app ) && EnumContainsBit( open_mode, std::ios::trunc ) ) {
        return false;
    }
    // 同时指定读和写时，不会主动创建文件，若文件不存在，则需先手动创建
    if ( EnumContainsBit( mode, OpenMode::ReadOnly ) && EnumContainsBit( mode, OpenMode::WriteOnly ) && !Exists() ) {
        std::ofstream new_file( name_ );
        if ( new_file.is_open() ) {
            new_file.close();
        }
        else {
            // 无法创建，直接返回
            return false;
        }
    }
    f_.open( name_, open_mode );
    InitPermission();
    return f_.is_open();
}

bool File::Remove() {
    if ( IsOpened() ) {
        Close();
    }
    return FileUtil::Remove( name_ );
}

std::string File::ReadAll() {
    if ( !IsOpened() ) {
        return {};
    }
    std::stringstream buffer;
    f_.seekg( 0, std::ios::beg );
    buffer << f_.rdbuf();
    return buffer.str();
}

bool File::Sync() {
    if ( !IsOpened() ) {
        return false;
    }
    return f_.sync() == 0;
}

void File::SetFileName( const std::string &name ) {
    name_ = name;
}

bool File::SetPermissions( Permissions permissions ) {
    fs::perms perms_result = fs::perms::none;
    auto      CalcPerms    = [&permissions, &perms_result]( Permissions p, fs::perms append_value ) {
        if ( EnumContainsBit( permissions, p ) ) {
            perms_result |= append_value;
        }
    };
    CalcPerms( Permissions::ReadOwner, fs::perms::owner_read );
    CalcPerms( Permissions::WriteOwner, fs::perms::owner_write );
    CalcPerms( Permissions::ExeOwner, fs::perms::owner_exec );
    CalcPerms( Permissions::ReadGroup, fs::perms::group_read );
    CalcPerms( Permissions::WriteGroup, fs::perms::group_write );
    CalcPerms( Permissions::ExeGroup, fs::perms::group_exec );
    CalcPerms( Permissions::ReadOther, fs::perms::others_read );
    CalcPerms( Permissions::WriteOther, fs::perms::others_write );
    CalcPerms( Permissions::ExeOther, fs::perms::others_exec );
    try {
        fs::permissions( name_, perms_result );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
    permissions_ = permissions;
    return true;
}

void File::Write( const char *data, int64_t size ) {
    if ( !IsOpened() || data == nullptr || size < 1 ) {
        return;
    }
    f_.write( data, size );
}

void File::Write( const char *data ) {
    if ( !IsOpened() || data == nullptr ) {
        return;
    }
    f_ << data;
}

#if defined( __GNUC__ ) || defined( __clang__ )
#    define UTILS_NO_SANITIZE_VPTR __attribute__( ( no_sanitize( "vptr" ) ) )
#else
#    define UTILS_NO_SANITIZE_VPTR
#endif

#if defined( __GLIBCXX__ )
namespace {

// libstdc++ 未公开 filebuf 的底层文件描述符，约定俗成的做法是派生 filebuf 后用 _M_file.fd() 取 fd。
// 把 *rdbuf() 当作 Helper 使用在严格意义上是 UB（vptr 不匹配，Helper 不新增数据成员故布局一致），
// 因此该访问刻意关闭 vptr 检查（只影响本函数，其余 UB 检查照旧生效）。
class FilebufHelper : public std::filebuf {
public:
    int handle() { return _M_file.fd(); }
};

UTILS_NO_SANITIZE_VPTR
int FilebufHandle( std::filebuf &buffer ) {
    return static_cast<FilebufHelper &>( buffer ).handle();
}

}  // namespace
#endif

int File::Handle() {
    if ( !IsOpened() ) {
        return -1;
    }
#if defined( __GLIBCXX__ )
    return FilebufHandle( *f_.rdbuf() );
#else
    return -1;  // 其他标准库不保证提供获取底层句柄的方式
#endif
}

void File::InitPermission() {
    if ( !Exists() ) {
        return;
    }
    Permissions result         = Permissions::None;
    fs::perms   perms          = fs::perms::none;
    auto        CalcPermission = [&result, &perms]( fs::perms p, Permissions append_value ) {
        if ( EnumContainsBit( perms, p ) ) {
            result |= append_value;
        }
    };
    try {
        perms = fs::status( name_ ).permissions();
    }
    catch ( const fs::filesystem_error & ) {
        return;
    }
    CalcPermission( fs::perms::owner_read, Permissions::ReadOwner );
    CalcPermission( fs::perms::owner_write, Permissions::WriteOwner );
    CalcPermission( fs::perms::owner_exec, Permissions::ExeOwner );
    CalcPermission( fs::perms::group_read, Permissions::ReadGroup );
    CalcPermission( fs::perms::group_write, Permissions::WriteGroup );
    CalcPermission( fs::perms::group_exec, Permissions::ExeGroup );
    CalcPermission( fs::perms::others_read, Permissions::ReadOther );
    CalcPermission( fs::perms::others_write, Permissions::WriteOther );
    CalcPermission( fs::perms::others_exec, Permissions::ExeOther );
    permissions_ = result;
}

bool FileUtil::Exists( std::string_view path ) noexcept {
    try {
        return fs::exists( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::Remove( std::string_view path ) noexcept {
    try {
        return fs::remove( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

uint32_t FileUtil::RemoveAll( std::string_view path ) noexcept {
    try {
        return static_cast<uint32_t>( fs::remove_all( path ) );
    }
    catch ( const fs::filesystem_error & ) {
        return 0;
    }
}

std::string FileUtil::CleanPath( std::string_view path ) noexcept {
    if ( path.empty() ) {
        return {};
    }

    std::string result( path );

    // 移除末尾的多余分隔符，但要保留根目录分隔符
#if defined( PLATFORM_OS_WINDOWS )
    const auto is_separator = []( char c ) { return c == '/' || c == '\\'; };
#else
    const auto is_separator = []( char c ) { return c == '/'; };  // POSIX 下 '\\' 是合法文件名字符
#endif
    while ( result.length() > 1 && is_separator( result.back() ) ) {
#if defined( PLATFORM_OS_WINDOWS )
        if ( result.length() == 3 && result[1] == ':' ) {
            break;  // 保留盘符根目录，如 "C:\"
        }
#endif
        result.pop_back();
    }

    return result;
}

std::string FileUtil::Load2Str( std::string_view file_path ) noexcept {
    std::error_code ec;
    const auto      file_size = fs::file_size( file_path, ec );
    if ( ec || !fs::is_regular_file( file_path ) ) {
        return {};
    }

    std::string content;
    content.resize( file_size );

    // file_path 是 string_view，可能不保证以 '\0' 结尾，需构造 std::string
    std::ifstream file( std::string( file_path ), std::ios::binary );
    if ( !file ) {
        return {};
    }

    file.read( content.data(), static_cast<std::streamsize>( file_size ) );
    return file.good() ? content : std::string{};
}

std::vector<uint8_t> FileUtil::Load2ByteArray( std::string_view file_path ) noexcept {
    std::error_code ec;
    const auto      file_size = fs::file_size( file_path, ec );
    if ( ec || !fs::is_regular_file( file_path ) ) {
        return {};
    }

    std::vector<uint8_t> buffer;
    buffer.resize( file_size );

    // file_path 是 string_view，可能不保证以 '\0' 结尾，需构造 std::string
    std::ifstream file( std::string( file_path ), std::ios::binary );
    if ( !file ) {
        return {};
    }

    file.read( reinterpret_cast<char *>( buffer.data() ), static_cast<std::streamsize>( file_size ) );
    return file.good() ? buffer : std::vector<uint8_t>{};
}

bool FileUtil::CreateDirectories( std::string_view path ) noexcept {
    try {
        return fs::create_directories( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::CreateDirectory( std::string_view path ) noexcept {
    try {
        return fs::create_directory( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::Copy( std::string_view from, std::string_view to, bool overwrite ) noexcept {
    try {
        if ( !fs::exists( from ) ) {
            return false;
        }
        if ( fs::exists( to ) && !overwrite ) {
            return false;
        }
        fs::copy_options opts = fs::copy_options::none;
        if ( fs::is_directory( from ) ) {
            opts |= fs::copy_options::recursive;  // 目录须递归复制整棵目录树
        }
        if ( overwrite ) {
            opts |= fs::copy_options::overwrite_existing;
        }
        fs::copy( from, to, opts );
        return true;
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::Move( std::string_view from, std::string_view to, bool overwrite ) noexcept {
    try {
        // 源不存在时直接失败：不得删除已存在的目标路径
        if ( !fs::exists( from ) ) {
            return false;
        }
        if ( fs::exists( to ) ) {
            if ( !overwrite ) {
                return false;
            }
            fs::remove_all( to );
        }
        fs::rename( from, to );
        return true;
    }
    catch ( const fs::filesystem_error & ) {
        // rename 失败（如跨设备移动）时降级为“复制 + 删除源”
        try {
            fs::copy_options opts = fs::copy_options::recursive | fs::copy_options::overwrite_existing;
            fs::copy( from, to, opts );
            fs::remove_all( from );
            return true;
        }
        catch ( ... ) {
            return false;
        }
    }
}

std::string FileUtil::DirName( std::string_view path ) noexcept {
    try {
        return fs::path( path ).parent_path().string();
    }
    catch ( ... ) {
        return "";
    }
}

std::string FileUtil::BaseName( std::string_view path ) noexcept {
    try {
        return fs::path( path ).filename().string();
    }
    catch ( ... ) {
        return "";
    }
}

std::string FileUtil::Extension( std::string_view path ) noexcept {
    try {
        return fs::path( path ).extension().string();
    }
    catch ( ... ) {
        return "";
    }
}

std::string FileUtil::Stem( std::string_view path ) noexcept {
    try {
        return fs::path( path ).stem().string();
    }
    catch ( ... ) {
        return "";
    }
}

bool FileUtil::IsFile( std::string_view path ) noexcept {
    try {
        return fs::is_regular_file( path );
    }
    catch ( ... ) {
        return false;
    }
}

bool FileUtil::IsDirectory( std::string_view path ) noexcept {
    try {
        return fs::is_directory( path );
    }
    catch ( ... ) {
        return false;
    }
}

uint64_t FileUtil::FileSize( std::string_view path ) noexcept {
    std::error_code ec;
    auto            size = fs::file_size( path, ec );
    return ec ? 0 : size;
}

std::vector<std::string> FileUtil::ListDir( std::string_view path, bool include_hidden ) noexcept {
    std::vector<std::string> result;
    try {
        for ( const auto &entry : fs::directory_iterator( path ) ) {
            std::string name = entry.path().filename().string();
            if ( !include_hidden && !name.empty() && name[0] == '.' ) {
                continue;
            }
            result.push_back( std::move( name ) );
        }
    }
    catch ( ... ) {
    }
    return result;
}

std::vector<std::string> FileUtil::ListDirFullPaths( std::string_view path, bool include_hidden ) noexcept {
    std::vector<std::string> result;
    try {
        for ( const auto &entry : fs::directory_iterator( path ) ) {
            std::string name = entry.path().filename().string();
            if ( !include_hidden && !name.empty() && name[0] == '.' ) {
                continue;
            }
            result.push_back( entry.path().string() );
        }
    }
    catch ( ... ) {
    }
    return result;
}

bool FileUtil::IsAbsolutePath( std::string_view path ) noexcept {
    try {
        return fs::path( path ).is_absolute();
    }
    catch ( ... ) {
        return false;
    }
}

bool FileUtil::WriteStr( std::string_view file_path, std::string_view content, bool append ) noexcept {
    std::ofstream file( std::string( file_path ), append ? std::ios::app : std::ios::trunc );
    if ( !file ) {
        return false;
    }
    file.write( content.data(), static_cast<std::streamsize>( content.size() ) );
    return file.good();
}

bool FileUtil::WriteBytes( std::string_view file_path, const std::vector<uint8_t> &data, bool append ) noexcept {
    std::ofstream file( std::string( file_path ), std::ios::binary | ( append ? std::ios::app : std::ios::trunc ) );
    if ( !file ) {
        return false;
    }
    if ( !data.empty() ) {
        file.write( reinterpret_cast<const char *>( data.data() ), static_cast<std::streamsize>( data.size() ) );
    }
    return file.good();
}

std::string FileUtil::GetSystemTempDir() noexcept {
    try {
        return fs::temp_directory_path().string();
    }
    catch ( ... ) {
#if defined( PLATFORM_OS_WINDOWS )
        // Windows 下依次回退到 TEMP/TMP 环境变量
        if ( const char *temp = std::getenv( "TEMP" ) ) {
            return temp;
        }
        if ( const char *temp = std::getenv( "TMP" ) ) {
            return temp;
        }
        return ".";
#else
        return "/tmp";  // POSIX 标准临时目录
#endif
    }
}

std::string FileUtil::CreateTempFile( std::string_view prefix, std::string_view suffix,
                                      std::string_view dir ) noexcept {
    std::string temp_dir = dir.empty() ? GetSystemTempDir() : std::string( dir );
    fs::path    dir_path( temp_dir );

    // 随机命名 + 独占创建，同名冲突时重试
    for ( int attempt = 0; attempt < 100; ++attempt ) {
        fs::path candidate = dir_path / ( std::string( prefix ) + "_" + RandomToken() + std::string( suffix ) );

#if defined( PLATFORM_OS_WINDOWS )
        int fd = ::_open( candidate.string().c_str(), _O_CREAT | _O_EXCL | _O_WRONLY, _S_IREAD | _S_IWRITE );
        if ( fd >= 0 ) {
            ::_close( fd );
            return candidate.string();
        }
#else
        int fd = ::open( candidate.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600 );
        if ( fd >= 0 ) {
            ::close( fd );
            return candidate.string();
        }
#endif
        if ( errno != EEXIST ) {
            break;  // 目录不存在、权限不足等错误，重试无意义
        }
    }

    return "";  // 失败
}

std::string FileUtil::CreateTempDirectory( std::string_view prefix, std::string_view parent_dir ) noexcept {
    std::string parent = parent_dir.empty() ? GetSystemTempDir() : std::string( parent_dir );
    fs::path    parent_path( parent );

    // 随机命名 + 独占创建，目录已存在时重试
    for ( int attempt = 0; attempt < 100; ++attempt ) {
        fs::path        candidate = parent_path / ( std::string( prefix ) + "_" + RandomToken() );
        std::error_code ec;
        if ( fs::create_directory( candidate, ec ) ) {
            return candidate.string();
        }
        if ( ec ) {
            break;  // 权限不足、父目录不存在等错误，重试无意义
        }
    }

    return "";  // 失败
}

void FileUtil::WalkDirectory( const std::string                                                      &root_path,
                              std::function<void( const std::string &, const fs::directory_entry & )> callback,
                              bool recursive, const std::vector<std::string> &include_files,
                              const std::vector<std::string> &exclude_files,
                              const std::vector<std::string> &include_extensions,
                              const std::vector<std::string> &exclude_extensions ) {
    // 验证路径是否存在且为目录
    if ( !fs::exists( root_path ) ) {
        throw std::invalid_argument( "Path does not exist: " + root_path );
    }
    if ( !fs::is_directory( root_path ) ) {
        throw std::invalid_argument( "Path is not a directory: " + root_path );
    }

    // 标准化路径
    fs::path root = fs::weakly_canonical( root_path );

    // 构建集合以加速查找
    std::set<std::string> include_files_set( include_files.begin(), include_files.end() );
    std::set<std::string> exclude_files_set( exclude_files.begin(), exclude_files.end() );
    std::set<std::string> include_exts_set( include_extensions.begin(), include_extensions.end() );
    std::set<std::string> exclude_exts_set( exclude_extensions.begin(), exclude_extensions.end() );

    // 判断是否应包含该条目
    auto should_include = [&]( const std::string &name, const std::string &ext ) -> bool {
        if ( !include_files.empty() && include_files_set.find( name ) == include_files_set.end() ) {
            return false;
        }
        if ( !exclude_files.empty() && exclude_files_set.find( name ) != exclude_files_set.end() ) {
            return false;
        }
        if ( !include_extensions.empty() && include_exts_set.find( ext ) == include_exts_set.end() ) {
            return false;
        }
        if ( !exclude_extensions.empty() && exclude_exts_set.find( ext ) != exclude_exts_set.end() ) {
            return false;
        }
        return true;
    };

    // 使用栈进行DFS遍历
    std::stack<fs::path> dir_stack;
    dir_stack.push( root );

    while ( !dir_stack.empty() ) {
        fs::path current_dir = dir_stack.top();
        dir_stack.pop();

        try {
            for ( const auto &entry : fs::directory_iterator( current_dir ) ) {
                std::string name = entry.path().filename().string();
                std::string ext  = entry.path().extension().string();

                // 如果是文件且不符合过滤条件，则跳过
                if ( !entry.is_directory() && !should_include( name, ext ) ) {
                    continue;
                }

                // 执行回调
                callback( entry.path().string(), entry );

                // 若为目录且允许递归，则压入栈中
                if ( entry.is_directory() && recursive ) {
                    // 不进入指向目录的符号链接，避免符号链接环导致死循环
                    std::error_code ec;
                    if ( !entry.is_symlink( ec ) ) {
                        dir_stack.push( entry.path() );
                    }
                }
            }
        }
        catch ( const fs::filesystem_error &e ) {
            throw std::runtime_error( "Error walking through directory: " + current_dir.string() + " - " + e.what() );
        }
    }
}
}  // namespace utils
