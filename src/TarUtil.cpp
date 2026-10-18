#include "TarUtil.h"
#include <cstdlib>
#include <filesystem>
#include <vector>
#include "Macros.h"

#if !defined( PLATFORM_OS_WINDOWS )
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace {

/**
 * @brief 对单个 shell 参数做安全引用
 *
 * 避免路径中的空格、引号、分号等字符被 shell 解析或造成命令注入。
 */
std::string ShellQuote( const std::string &arg ) {
#if defined( PLATFORM_OS_WINDOWS )
    // Windows cmd：双引号包裹，内部双引号转义
    std::string quoted = "\"";
    for ( char c : arg ) {
        if ( c == '"' ) {
            quoted += "\"\"";
        }
        else {
            quoted += c;
        }
    }
    quoted += "\"";
    return quoted;
#else
    // POSIX sh：单引号包裹，内部单引号替换为 '\''
    std::string quoted = "'";
    for ( char c : arg ) {
        if ( c == '\'' ) {
            quoted += "'\\''";
        }
        else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
#endif
}

/**
 * @brief 执行命令并返回其退出码
 *
 * @param args 命令及参数列表（逐参数安全引用）
 * @param message [out] 失败时的错误信息（含命令行）
 * @return int 进程退出码；无法执行时返回 -1
 */
int RunCommand( const std::vector<std::string> &args, std::string &message ) {
    std::string cmd;
    for ( const auto &arg : args ) {
        if ( !cmd.empty() ) {
            cmd += ' ';
        }
        cmd += ShellQuote( arg );
    }

    int status = std::system( cmd.c_str() );
    if ( status == -1 ) {
        message = "failed to execute command: " + cmd;
        return -1;
    }

#if !defined( PLATFORM_OS_WINDOWS )
    if ( !WIFEXITED( status ) ) {
        message = "command terminated abnormally: " + cmd;
        return -1;
    }
    status = WEXITSTATUS( status );
#endif

    if ( status != 0 ) {
        message = "command failed with exit code " + std::to_string( status ) + ": " + cmd;
    }
    return status;
}

}  // namespace

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
    TarResult result;
    if ( tar_path_.empty() ) {
        result.message = "tar file path is empty";
        return result;
    }
    if ( file_list_.empty() ) {
        result.message = "no file to compress";
        return result;
    }

    // -P 需作为独立参数，且所有参数都会被安全引用
    std::vector<std::string> cmd{ "tar", "-czf", tar_path_, "-P" };
    for ( const auto &exclude : exclude_list_ ) {
        cmd.emplace_back( "--exclude=" + exclude );
    }
    for ( const auto &file : file_list_ ) {
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

    result.success = RunCommand( cmd, result.message ) == 0;
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
    TarResult result;
    if ( tar_path.empty() ) {
        result.message = "tar file path is empty";
        return result;
    }
    if ( source_list.empty() ) {
        result.message = "no file to compress";
        return result;
    }

    // 注意：不能使用 -czfP 组合写法，GNU tar 会把 "P" 当作 -f 的归档文件名参数
    std::vector<std::string> cmd{ "tar", "-czf", tar_path, "-P" };
    for ( const auto &file : source_list ) {
        fs::path path( file );
        if ( path.is_absolute() ) {
            // 与 TarObject::Compress 行为一致：以 -C 父目录 + 文件名入包，归档内为相对路径
            cmd.emplace_back( "-C" );
            cmd.emplace_back( path.parent_path().string() );
            cmd.emplace_back( path.filename().string() );
        }
        else {
            cmd.emplace_back( file );
        }
    }

    result.success = RunCommand( cmd, result.message ) == 0;
    return result;
}

TarResult TarUtil::Decompress( const std::string &tar_path, const std::string &dec_path ) {
    TarResult result;
    if ( tar_path.empty() ) {
        result.message = "tar file path is empty";
        return result;
    }

    std::error_code ec;
    if ( !fs::exists( tar_path, ec ) ) {
        result.message = "tar file does not exist: " + tar_path;
        return result;
    }

    if ( !dec_path.empty() ) {
        std::error_code dir_ec;
        fs::create_directories( dec_path, dir_ec );  // 目录已存在时不会报错
        if ( dir_ec ) {
            result.message = "failed to create directory: " + dec_path;
            return result;
        }
    }

    // 使用 -xf 由 tar 自动识别压缩格式（gzip/bzip2/xz/zstd）
    std::vector<std::string> cmd{ "tar", "-xf", tar_path };
    if ( !dec_path.empty() ) {
        cmd.emplace_back( "-C" );
        cmd.emplace_back( dec_path );
    }

    result.success = RunCommand( cmd, result.message ) == 0;
    return result;
}

}  // namespace utils
