#include "FileUtil.h"
#include <iostream>

namespace fs = std::filesystem;

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

int File::Handle() {
    class Helper : public std::filebuf {
    public:
        int handle() { return _M_file.fd(); }
    };
    if ( !IsOpened() ) {
        return -1;
    }
    return static_cast<Helper &>( *f_.rdbuf() ).handle();
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

    fs::path    p( path );
    std::string result = p.string();

    // 移除末尾的分隔符，但要保留根目录的分隔符
    while ( result.length() > 1 && ( result.back() == '/' || result.back() == '\\' ) ) {
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

    std::ifstream file( file_path.data(), std::ios::binary );
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

    std::ifstream file( file_path.data(), std::ios::binary );
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
        fs::copy_options opts = overwrite ? fs::copy_options::overwrite_existing : fs::copy_options::none;
        fs::copy( from, to, opts );
        return true;
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::Move( std::string_view from, std::string_view to, bool overwrite ) noexcept {
    try {
        const fs::path src( from ), dst( to );
        if ( overwrite && fs::exists( dst ) ) {
            fs::remove_all( dst );
        }
        else if ( fs::exists( dst ) ) {
            return false;
        }
        fs::rename( src, dst );
        return true;
    }
    catch ( const fs::filesystem_error & ) {
        // fallback: copy + remove
        try {
            fs::copy( from, to, fs::copy_options::recursive );
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
        return "/tmp";  // fallback
    }
}

std::string FileUtil::CreateTempFile( std::string_view prefix, std::string_view suffix,
                                      std::string_view dir ) noexcept {
    std::string temp_dir = dir.empty() ? GetSystemTempDir() : std::string( dir );
    fs::path    temp_path( temp_dir );

    auto now = std::chrono::high_resolution_clock::now();
    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>( now.time_since_epoch() ).count();

    temp_path /= ( std::string( prefix ) + "_" + std::to_string( ns ) + std::string( suffix ) );

    try {
        std::ofstream file( temp_path, std::ios::out );
        if ( file ) {
            file.close();
            return temp_path.string();
        }
    }
    catch ( ... ) {
    }

    return "";  // 失败
}

std::string FileUtil::CreateTempDirectory( std::string_view prefix, std::string_view parent_dir ) noexcept {
    std::string parent = parent_dir.empty() ? GetSystemTempDir() : std::string( parent_dir );
    fs::path    dir( parent );

    auto now = std::chrono::high_resolution_clock::now();
    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>( now.time_since_epoch() ).count();

    dir /= ( std::string( prefix ) + "_" + std::to_string( ns ) );

    try {
        if ( fs::create_directory( dir ) ) {
            return dir.string();
        }
    }
    catch ( ... ) {
    }

    return "";
}
}  // namespace utils
