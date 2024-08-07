#include "TarUtil.h"
#include <stdio.h>
#include <filesystem>
#include "Macros.h"
#include "StringUtil.h"

#if defined( PLATFORM_OS_LINUX )
    #define POPEN  popen
    #define PCLOSE pclose
#elif defined( PLATFORM_OS_WINDOWS )
    #define POPEN  _popen
    #define PCLOSE _pclose
#endif

namespace fs = std::filesystem;

namespace utils {

TarObject::TarObject( const std::string &tar_path, const std::initializer_list<std::string> &init ) :
    tar_path_( tar_path ), file_list_( init ) {}

void TarObject::SetTarPath( const std::string &tar_path ) {
    tar_path_ = tar_path;
}

void TarObject::AddFile( const std::string &file_path ) {
    file_list_.emplace( file_path );
}

void TarObject::AddFile( const std::vector<std::string> &file_list ) {
    for ( auto &file : file_list ) {
        file_list_.emplace( file );
    }
}

void TarObject::AddExclude( const std::string &file_path ) {
    exclude_list_.emplace( file_path );
}

void TarObject::AddExclude( const std::vector<std::string> &file_list ) {
    for ( auto &file : file_list ) {
        exclude_list_.emplace( file );
    }
}

void TarObject::RemoveFile( const std::string &file_path ) {
    auto iter = file_list_.find( file_path );
    if ( iter != file_list_.end() ) {
        file_list_.erase( iter );
    }
}

void TarObject::RemoveExclude( const std::string &file_path ) {
    auto iter = exclude_list_.find( file_path );
    if ( iter != exclude_list_.end() ) {
        exclude_list_.erase( iter );
    }
}

void TarObject::ClearFile() {
    file_list_.clear();
}

void TarObject::ClearExclude() {
    exclude_list_.clear();
}

std::string TarObject::GetTarPath() const {
    return tar_path_;
}

std::set<std::string> TarObject::GetFileList() const {
    return file_list_;
}

size_t TarObject::GetFileCount() const {
    return file_list_.size();
}

TarResult TarObject::Compress() const {
    TarResult  result;
    stringlist cmd{ "tar", "-czf", tar_path_, "-P" };
    for ( auto &exclude : exclude_list_ ) {
        cmd.emplace_back( "--exclude='" + exclude + "'" );
    }
    // cmd.insert( cmd.end(), file_list_.begin(), file_list_.end() );
    for ( auto &file : file_list_ ) {
        fs::path path( file );
        if ( path.is_absolute() ) {
            cmd.emplace_back( "-C" );
            cmd.emplace_back( path.parent_path().string() );
            cmd.emplace_back( path.filename().string() );
        }
        else {
            cmd.emplace_back( file );
        }
    }
    if ( std::system( StringUtil::Join( " ", cmd ).c_str() ) != 0 ) {
        result.success = false;
    }
    else {
        result.success = true;
    }
    return result;
}

bool TarObject::TarExist() const {
    try {
        return fs::exists( tar_path_ );
    }
    catch ( ... ) {
        return false;
    }
}

TarResult TarUtil::Compress( const std::string &tar_path, const std::set<std::string> &source_list ) {
    TarResult  result;
    stringlist cmd{ "tar", "-czfP", tar_path };
    cmd.insert( cmd.end(), source_list.begin(), source_list.end() );
    if ( std::system( StringUtil::Join( " ", cmd ).c_str() ) != 0 ) {
        result.success = false;
    }
    else {
        result.success = true;
    }
    return result;
}

TarResult TarUtil::Decompress( const std::string &tar_path, const std::string &dec_path ) {
    return {};
}

}  // namespace utils
