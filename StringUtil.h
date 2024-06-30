#pragma once

#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace utils {
using stringlist = std::vector<std::string>;
class StringUtil {
public:
    /**
     * @brief 压缩字符串，去除空格
     *
     * @param tokens 字符串表
     * @return std::vector<std::string>
     */
    static stringlist Compact( const stringlist &tokens );
    /**
     * @brief 根据指定分隔符来分割字符串
     *
     * @param str 字符串
     * @param delim 分隔符
     * @param trim_empty 是否去除空字符串
     * @return std::vector<std::string>
     */
    static stringlist Split( const std::string &str, const std::string &delim, const bool trim_empty = false );

    /**
     * @brief 拼接字符串
     *
     * @param tokens 字符串数组
     * @param delim 分隔符
     * @return std::string
     */
    static std::string Join( const stringlist &tokens, const std::string &delim, const bool trim_empty = false );
    /**
     * @brief 去除\r\n\t
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string Trim( const std::string &str );
    /**
     * @brief 生成times个str字符串
     *
     * @param str 字符串
     * @param times 重复次数
     * @return std::string
     */
    static std::string Repeat( const std::string &str, unsigned int times );
    /**
     * @brief 字符串转大写
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string ToUpper( const std::string &str );
    /**
     * @brief 字符串转小写
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string ToLower( const std::string &str );
    /**
     * @brief 检查字符串是否以prefix开头
     *
     * @param str
     * @param prefix
     * @return true
     * @return false
     */
    static bool StartWith( const std::string &str, const std::string &prefix );
    /**
     * @brief 检查字符串是否以suffix结尾
     *
     * @param str
     * @param suffix
     * @return true
     * @return false
     */
    static bool EndWith( const std::string &str, const std::string &suffix );
    /**
     * @brief 检查字符串是否包含指定的内容
     *
     * @param str
     * @param token
     * @return true
     * @return false
     */
    static bool Contains( const std::string &str, const std::string &token );
};

}  // namespace utils