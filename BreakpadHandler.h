#pragma once

#include <string>
#include <vector>

namespace utils {

class BreakpadHandler {
public:
    BreakpadHandler( const BreakpadHandler & ) = delete;
    BreakpadHandler( BreakpadHandler && )      = delete;
    ~BreakpadHandler();
    BreakpadHandler &operator=( const BreakpadHandler & ) = delete;
    BreakpadHandler &operator=( BreakpadHandler && )      = delete;

    static BreakpadHandler &Instance();

    void Init();

    void SetDumpPath( const std::string &path );
    void SetMaxSize( int64_t max_size );
    void SetMaxCount( uint32_t max_count );
    void GenerateDump();

    std::string              GetDumpPath() const;
    std::vector<std::string> GetDumpFileList() const;
    int64_t                  GetMaxSize() const;
    uint32_t                 GetMaxCount() const;

private:
    BreakpadHandler();
    class BreakpadHandlerPrivate;
    BreakpadHandlerPrivate *d = nullptr;
};

}  // namespace utils
