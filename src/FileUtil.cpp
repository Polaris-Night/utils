#include "FileUtil.h"
#include <filesystem>
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

bool FileUtil::Exists( const std::string &path ) {
    try {
        return fs::exists( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

bool FileUtil::Remove( const std::string &path ) {
    try {
        return fs::remove( path );
    }
    catch ( const fs::filesystem_error & ) {
        return false;
    }
}

uint32_t FileUtil::RemoveAll( const std::string &path ) {
    try {
        return fs::remove_all( path );
    }
    catch ( const fs::filesystem_error & ) {
        return 0;
    }
}

std::string FileUtil::CleanPath( const std::string &path ) {
    if ( !path.empty() && path.back() == '/' ) {
        return path.substr( 0, path.size() - 1 );
    }
    return path;
}

}  // namespace utils
