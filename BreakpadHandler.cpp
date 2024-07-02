#include "BreakpadHandler.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include "Macros.h"

#define CONSOLE_LOG( fmt, ... ) std::fprintf( stderr, fmt "\n", __VA_ARGS__ )
#define UNUSE( x )              (void)x;

namespace fs = std::filesystem;

#if defined( PLATFORM_OS_LINUX )
    #include "client/linux/handler/exception_handler.h"
#elif defined( PLATFORM_OS_WINDOWS )
    #include "client/windows/handler/exception_handler.h"
#elif defined( PLATFORM_OS_OSX )
    #include "client/mac/handler/exception_handler.h"
#endif

static bool DumpCallback
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

namespace utils {
class BreakpadHandler::BreakpadHandlerPrivate {
public:
    void RecycleDumpFiles() {
        try {
            std::vector<fs::path> files;
            uintmax_t             total_size = 0;
            for ( auto &entry : fs::directory_iterator( dump_path ) ) {
                if ( !entry.is_regular_file() || entry.path().extension() != extension ) {
                    continue;
                }
                files.emplace_back( entry.path() );
                total_size += fs::file_size( entry.path() );
            }
            std::sort( files.begin(), files.end(), []( const fs::path &lhs, const fs::path &rhs ) {
                return fs::last_write_time( lhs ) < fs::last_write_time( rhs );
            } );
            while ( total_size > max_size * 1024 * 1024 || files.size() > max_count ) {
                CONSOLE_LOG( "Remove dump file: %s", files.front().c_str() );
                total_size -= fs::file_size( files.front() );
                fs::remove( files.front() );
                files.erase( files.begin() );
            }
        }
        catch ( const fs::filesystem_error &e ) {
            CONSOLE_LOG( "Error: %s", e.what() );
        }
    }

public:
    std::unique_ptr<google_breakpad::ExceptionHandler> handler;
    std::string                                        extension = ".dmp";
    fs::path                                           dump_path;
    int64_t                                            max_size  = 5;
    int32_t                                            max_count = 5;
};

BreakpadHandler::BreakpadHandler() : d( new BreakpadHandlerPrivate() ) {}

BreakpadHandler::~BreakpadHandler() {
    delete d;
    d = nullptr;
}

BreakpadHandler &BreakpadHandler::Instance() {
    static BreakpadHandler instance;
    return instance;
}

void BreakpadHandler::Init() {
#if defined( PLATFORM_OS_LINUX )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>(
        google_breakpad::MinidumpDescriptor( d->dump_path.string() ), nullptr, DumpCallback, nullptr, true, -1 );
#elif defined( PLATFORM_OS_WINDOWS )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>(
        d->dump_path.string(), nullptr, DumpCallback, nullptr, google_breakpad::ExceptionHandler::HANDLER_ALL );
#elif defined( PLATFORM_OS_OSX )
    d->handler = std::make_unique<google_breakpad::ExceptionHandler>( d->dump_path.string(), nullptr, DumpCallback,
                                                                      nullptr, true, nullptr );
#endif
    d->RecycleDumpFiles();
}

void BreakpadHandler::SetDumpPath( const std::string &path ) {
    fs::path abs_path;
    try {
        abs_path = fs::absolute( path );
        fs::create_directories( abs_path );
        if ( !fs::is_directory( abs_path ) ) {
            CONSOLE_LOG( "Error: %s is not exist", path.c_str() );
            return;
        }
    }
    catch ( const fs::filesystem_error &e ) {
        CONSOLE_LOG( "Error: %s", e.what() );
        return;
    }
    d->dump_path = std::move( abs_path );
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

std::string BreakpadHandler::GetDumpPath() const {
    return d->dump_path.string();
}

std::vector<std::string> BreakpadHandler::GetDumpFileList() const {
    std::vector<std::string> dump_files;
    try {
        for ( auto &p : fs::directory_iterator( d->dump_path ) ) {
            if ( p.is_regular_file() && p.path().extension() == d->extension ) {
                dump_files.push_back( p.path().string() );
            }
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