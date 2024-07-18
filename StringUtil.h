#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace utils {
using stringlist = std::vector<std::string>;

template <typename T>
struct CanConvertToString
    : std::disjunction<std::is_convertible<T, std::string>, std::is_convertible<T, std::string_view>> {};

template <typename... Args>
struct IsStringType : std::conjunction<CanConvertToString<Args>...> {};

template <typename T>
constexpr bool IsStringType_v = IsStringType<T>::value;

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
     * @param separator 分隔符
     * @return std::vector<std::string>
     */
    static stringlist Split( const std::string &str, const std::string &separator );
    /**
     * @brief 拼接字符串
     *
     * @tparam Args
     * @param separator 分隔符
     * @param args 字符串列表
     * @return std::string
     */
    template <typename... Args>
    static std::string Join( const std::string &separator, Args... args ) {
        static_assert( IsStringType<Args...>::value, "Join requires argument is string type" );
        static_assert( sizeof...( args ) > 0, "Join requires at least one argument" );
        std::ostringstream oss;
        bool               first = true;

        ( ( oss << ( first ? "" : separator ) << args, first = false ), ... );

        return oss.str();
    }
    /**
     * @brief 针对于字符串容器的拼接函数
     *
     * @tparam Container 容器类型，其元素类型为std::string
     * @param separator 分隔符
     * @param container 字符串容器
     * @return std::string
     */
    template <template <class...> class Container>
    static std::string Join( const std::string &separator, const Container<std::string> &container ) {
        std::string result;
        auto        iter = std::cbegin( container );
        auto        end  = std::cend( container );

        if ( iter != end ) {
            result += *iter;  // 加入第一个元素到结果中
            ++iter;
        }
        // 遍历容器剩余的元素，将分隔符和元素依次连接到结果中
        for ( ; iter != end; ++iter ) {
            result += separator;
            result += *iter;
        }
        return result;
    }
    /**
     * @brief 去除\r\n\t和空格
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
    /**
     * @brief 转换为十六进制字符串
     *
     * @param data 数据
     * @param separator 分隔符
     * @return std::string
     */
    static std::string ConvertToHexStr( const char *data, char separator = ' ' );
    /**
     * @brief 转换为二进制字符串
     *
     * @param value 数值
     * @param count 二进制位数
     * @return std::string
     */
    static std::string IntToBitString( uint64_t value, int count );
};

}  // namespace utils