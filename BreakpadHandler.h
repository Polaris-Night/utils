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

    /**
     * @brief 初始化异常处理
     *
     */
    void Init();

    /**
     * @brief 设置dump生成目录
     *
     * @param path
     */
    void SetDumpPath( const std::string &path );
    /**
     * @brief 设置dump目录最大容量，单位MB
     *
     * @param max_size
     */
    void SetMaxSize( int64_t max_size );
    /**
     * @brief 设置dump文件最大数量
     *
     * @param max_count
     */
    void SetMaxCount( uint32_t max_count );
    /**
     * @brief 主动生成dump
     *
     */
    void GenerateDump();

    /**
     * @brief 获取dump目录路径
     *
     * @return std::string
     */
    std::string GetDumpPath() const;
    /**
     * @brief 获取dump目录下dump文件列表
     *
     * @return std::vector<std::string>
     */
    std::vector<std::string> GetDumpFileList() const;
    /**
     * @brief 获取dump目录最大容量
     *
     * @return int64_t
     */
    int64_t GetMaxSize() const;
    /**
     * @brief 获取dump文件最大数量
     *
     * @return uint32_t
     */
    uint32_t GetMaxCount() const;

private:
    BreakpadHandler();
    class BreakpadHandlerPrivate;
    BreakpadHandlerPrivate *d = nullptr;
};

}  // namespace utils
