#include "BreakpadHandler.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stack>
#include "FileUtil.h"
#include "GitHash.hpp"
#include "Macros.h"

#if defined( PLATFORM_OS_LINUX )
    #include "client/linux/handler/exception_handler.h"
#elif defined( PLATFORM_OS_WINDOWS )
    #include "client/windows/handler/exception_handler.h"
#elif defined( PLATFORM_OS_OSX )
    #include "client/mac/handler/exception_handler.h"
#endif

template <typename... Args>
void console_log( const char *fmt, Args &&...args ) {
    if constexpr ( sizeof...( args ) > 0 ) {
        std::fprintf( stderr, fmt, std::forward<Args>( args )... );
    }
    else {
        std::fprintf( stderr, "%s", fmt );
    }
    std::fprintf( stderr, "\n" );
}

#define CONSOLE_LOG( ... ) console_log( __VA_ARGS__ )
#define UNUSE( x )         (void)x;

namespace fs = std::filesystem;

namespace {

bool DumpCallback
#if defined( PLATFORM_OS_LINUX )
    ( const google_breakpad::MinidumpDescriptor &descriptor, void *context, bool succeeded )
#elif defined( PLATFORM_OS_WINDOWS )
    ( const wchar_t *dump_dir, const wchar_t *minidump_id, void *context, EXCEPTION_POINTERS *exinfo,
      MDRawAssertionInfo *assertion, bool succeeded )
#elif defined( PLATFORM_OS_OSX )
    ( const char *dump_dir, const char *minidump_id, void *context, bool succeeded )
#endif
{
    UNUSE( context )
#if defined( PLATFORM_OS_LINUX )
    CONSOLE_LOG( "%s, dump path: %s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump",
                 descriptor.path() );
#elif defined( PLATFORM_OS_WINDOWS )
    UNUSE( assertion )
    UNUSE( exinfo )
    CONSOLE_LOG( "%s, dump path: %S/%S\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump",
                 dump_dir, minidump_id );
#elif defined( PLATFORM_OS_OSX )
    CONSOLE_LOG( "%s, dump path: %s/%s\n", succeeded ? "Succeed to write minidump" : "Failed to write minidump",
                 dump_dir, minidump_id );
#endif
    return succeeded;
}

std::vector<fs::path> ForeachDir( const fs::path &root_path, const std::string &extension ) {
    std::vector<fs::path> result;
    std::stack<fs::path>  dir_stack;

    dir_stack.push( root_path );
    try {
        while ( !dir_stack.empty() ) {
            fs::path current_path = dir_stack.top();
            dir_stack.pop();
            for ( const auto &entry : fs::directory_iterator( current_path ) ) {
                if ( entry.is_directory() ) {
                    dir_stack.push( entry.path() );
                }
                else if ( entry.is_regular_file() && entry.path().extension() == extension ) {
                    result.emplace_back( entry.path() );
                }
            }
        }
    }
    catch ( const fs::filesystem_error &e ) {
        CONSOLE_LOG( "Error foreach dir, reason: %s", e.what() );
    }

    return result;
}

void CleanEmptyDir( const fs::path &root_path ) {
    if ( root_path.empty() ) {
        return;
    }

    // 先收集所有子目录，再按路径长度从深到浅尝试删除空目录
    std::vector<fs::path> dirs;
    std::stack<fs::path>  dir_stack;
    dir_stack.push( root_path );

    while ( !dir_stack.empty() ) {
        fs::path current_path = dir_stack.top();
        dir_stack.pop();

        std::error_code      iter_ec;
        fs::directory_iterator it( current_path, iter_ec );
        if ( iter_ec ) {
            CONSOLE_LOG( "Error clean empty dir, reason: %s", iter_ec.message().c_str() );
            continue;
        }
        for ( fs::directory_iterator end; it != end; it.increment( iter_ec ) ) {
            if ( iter_ec ) {
                CONSOLE_LOG( "Error clean empty dir, reason: %s", iter_ec.message().c_str() );
                break;
            }
            std::error_code type_ec;
            if ( it->is_directory( type_ec ) && !type_ec ) {
                dirs.emplace_back( it->path() );
                dir_stack.push( it->path() );
            }
        }
    }

    // 子目录路径必然长于其父目录，按路径长度降序排序即可保证先删子目录、再删父目录
    std::sort( dirs.begin(), dirs.end(), []( const fs::path &lhs, const fs::path &rhs ) {
        return lhs.string().size() > rhs.string().size();
    } );
    for ( const auto &dir : dirs ) {
        std::error_code remove_ec;
        fs::remove( dir, remove_ec );  // 非空目录删除失败即可，无需额外处理
    }
}

}  // namespace


namespace utils {
class BreakpadHandler::BreakpadHandlerPrivate {
public:
    void RecycleDumpFiles() {
        try {
            std::vector<fs::path> files = ForeachDir( dump_root_path, extension );
            if ( files.size() <= 1 ) {
                return;
            }

            std::error_code ec;
            uintmax_t       total_size = 0;
            for ( auto &file : files ) {
                auto size = fs::file_size( file, ec );
                if ( !ec ) {
                    total_size += size;
                }
            }
            std::sort( files.begin(), files.end(), []( const fs::path &lhs, const fs::path &rhs ) {
                return fs::last_write_time( lhs ) < fs::last_write_time( rhs );
            } );

            const uintmax_t max_bytes = max_size > 0 ? static_cast<uintmax_t>( max_size ) * 1024 * 1024 : 0;

            // 修复原实现的条件优先级问题：容量或数量任一超限即触发回收（至少保留最后一个）
            while ( ( total_size > max_bytes || files.size() > max_count ) && files.size() > 1 ) {
                auto size = fs::file_size( files.front(), ec );
                if ( !ec && size <= total_size ) {
                    total_size -= size;
                }
                CONSOLE_LOG( "Remove dump file: %s", files.front().string().c_str() );
                fs::remove( files.front(), ec );
                files.erase( files.begin() );
            }
        }
        catch ( const fs::filesystem_error &e ) {
            CONSOLE_LOG( "Error recycle dump files: %s", e.what() );
        }
    }

    bool CreateDumpDir() {
        try {
            fs::create_directories( dump_path );
            if ( !fs::is_directory( dump_path ) ) {
                CONSOLE_LOG( "Error: %s is not exist", dump_path.c_str() );
                return false;
            }
        }
        catch ( const fs::filesystem_error &e ) {
            CONSOLE_LOG( "Error create dump dir: %s", e.what() );
            return false;
        }
        return true;
    }

public:
    std::unique_ptr<google_breakpad::ExceptionHandler> handler;
    std::string                                        extension = ".dmp";
    fs::path                                           dump_root_path;
    std::string                                        dump_path;
    int64_t                                            max_size  = 5;
    uint32_t                                           max_count = 5;
};

BreakpadHandler::BreakpadHandler() : d( std::make_unique<BreakpadHandlerPrivate>() ) {}

BreakpadHandler::~BreakpadHandler() = default;

BreakpadHandler &BreakpadHandler::Instance() {
    static BreakpadHandler instance;
    return instance;
}

void BreakpadHandler::Init() {
    if ( d->dump_root_path.empty() || d->dump_path.empty() ) {
        CONSOLE_LOG( "Error: dump path is empty, init breakpad handle failed" );
        return;
    }
    CleanEmptyDir( d->dump_root_path );
    d->CreateDumpDir();
    CONSOLE_LOG( "Set dump path: %s", d->dump_path.c_str() );
#if defined( PLATFORM_OS_LINUX )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>(
        google_breakpad::MinidumpDescriptor( d->dump_path ), nullptr, DumpCallback, nullptr, true, -1 );
#elif defined( PLATFORM_OS_WINDOWS )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>( d->dump_path, nullptr, DumpCallback, nullptr,
                                                                      google_breakpad::ExceptionHandler::HANDLER_ALL );
#elif defined( PLATFORM_OS_OSX )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>( d->dump_path, nullptr, DumpCallback, nullptr,
                                                                      true, nullptr );
#endif
    d->RecycleDumpFiles();
}

void BreakpadHandler::SetDumpRootPath( const std::string &path, bool git_hash_as_dump_path ) {
    fs::path abs_path;
    try {
        if ( git_hash_as_dump_path && !path.empty() ) {
            abs_path = FileUtil::JoinPaths( fs::absolute( path ), std::string( GitHash::shortSha1 ) );
        }
        else {
            abs_path = fs::absolute( path );
        }
        d->dump_path      = abs_path.string();
        d->dump_root_path = git_hash_as_dump_path ? abs_path.parent_path() : std::move( abs_path );
    }
    catch ( const fs::filesystem_error &e ) {
        CONSOLE_LOG( "Error: %s", e.what() );
        return;
    }
}

void BreakpadHandler::SetMaxSize( int64_t max_size ) {
    d->max_size = max_size;
}

void BreakpadHandler::SetMaxCount( uint32_t max_count ) {
    d->max_count = max_count;
}

void BreakpadHandler::GenerateDump() {
    if ( d->handler ) {
        d->handler->WriteMinidump();
    }
}

std::string BreakpadHandler::GetDumpRootPath() const {
    return d->dump_root_path.string();
}

std::vector<std::string> BreakpadHandler::GetDumpFileList() const {
    std::vector<std::string> dump_files;
    try {
        auto files = ForeachDir( d->dump_root_path, d->extension );
        for ( auto &file : files ) {
            dump_files.emplace_back( file.string() );
        }
    }
    catch ( const fs::filesystem_error &e ) {
        CONSOLE_LOG( "Error: %s", e.what() );
    }
    return dump_files;
}

int64_t BreakpadHandler::GetMaxSize() const {
    return d->max_size;
}

uint32_t BreakpadHandler::GetMaxCount() const {
    return d->max_count;
}

}  // namespace utils