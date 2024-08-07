#pragma once

#include <set>
#include <string>
#include <vector>

namespace utils {

struct TarResult {
    bool        success{ false };
    std::string message;

    explicit operator bool() const { return success; }
};

enum TarFormat
{
    kGzip = 0,
    kBzip2,
    kXz,
    kZstd,
    kRaw,
    kUnknown
};

class TarObject {
public:
    TarObject() = default;
    TarObject( const std::string &tar_path, const std::initializer_list<std::string> &init );

    void      SetTarPath( const std::string &tar_path );
    void      AddFile( const std::string &file_path );
    void      AddFile( const std::vector<std::string> &file_list );
    void      AddExclude( const std::string &file_path );
    void      AddExclude( const std::vector<std::string> &file_list );
    void      RemoveFile( const std::string &file_path );
    void      RemoveExclude( const std::string &file_path );
    void      ClearFile();
    void      ClearExclude();
    TarResult Compress() const;

    std::string           GetTarPath() const;
    std::set<std::string> GetFileList() const;
    size_t                GetFileCount() const;

    bool TarExist() const;

private:
    std::string           tar_path_;
    std::set<std::string> file_list_;
    std::set<std::string> exclude_list_;
};

class TarUtil {
public:
    static TarResult Compress( const std::string &tar_path, const std::set<std::string> &source_list );
    static TarResult Decompress( const std::string &tar_path, const std::string &dec_path );
};

}  // namespace utils
