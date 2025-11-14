#pragma once

#include <charconv>
#include <iomanip>
#include <optional>
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
     * @brief 根据指定分隔符来分割字符串
     *
     * @param str 字符串
     * @param separator 分隔符
     * @return std::vector<std::string>
     */
    static stringlist Split( std::string_view str, std::string_view separator );
    /**
     * @brief 拼接字符串
     *
     * @tparam Args
     * @param separator 分隔符
     * @param args 字符串列表
     * @return std::string
     */
    template <typename... Args>
    [[nodiscard]] static std::string Join( std::string_view separator, Args... args ) {
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
     * @tparam Container 容器类型
     * @param separator 分隔符
     * @param container 字符串容器
     * @return std::string
     */
    template <typename Container>
    [[nodiscard]] static std::string Join( std::string_view separator, const Container &container ) {
        auto iter = std::cbegin( container );
        auto end  = std::cend( container );

        if ( iter == end ) {
            return {};
        }

        std::string result;
        result += *iter;  // 加入第一个元素到结果中
        ++iter;

        // 遍历容器剩余的元素，将分隔符和元素依次连接到结果中
        for ( ; iter != end; ++iter ) {
            result += separator;
            result += *iter;
        }
        return result;
    }
    /**
     * @brief 去除" \n\r\t\v\f"
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string Trim( std::string_view str ) noexcept;
    /**
     * @brief 生成times个str字符串
     *
     * @param str 字符串
     * @param times 重复次数
     * @return std::string
     */
    static std::string Repeat( std::string_view str, unsigned int times );
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
    static bool StartWith( std::string_view str, std::string_view prefix );
    /**
     * @brief 检查字符串是否以suffix结尾
     *
     * @param str
     * @param suffix
     * @return true
     * @return false
     */
    static bool EndWith( std::string_view str, std::string_view suffix );
    /**
     * @brief 检查字符串是否包含指定的内容
     *
     * @param str
     * @param token
     * @return true
     * @return false
     */
    static bool Contains( std::string_view str, std::string_view token ) noexcept;
    /**
     * @brief 转换为十六进制字符串
     *
     * @param data 数据
     * @param separator 分隔符
     * @return std::string
     */
    static std::string ConvertToHexStr( std::string_view data, char separator = ' ' );
    /**
     * @brief 转换为二进制字符串
     *
     * @param value 数值
     * @param count 二进制位数
     * @return std::string
     */
    static std::string IntToBitString( uint64_t value, int count );
    /**
     * @brief 将字符串安全转换为指定数值类型 T
     *
     * 支持整型（int, long, unsigned 等）和浮点型（float, double），自动识别进制（十进制、0x 十六进制、0 八进制）。
     *
     * @tparam T 目标数值类型，必须是算术类型（arithmetic type）
     * @param[in] str 输入字符串
     * @return std::optional<T> 转换成功则返回值；若输入为空、包含非法字符或无法完全解析，则返回 std::nullopt
     *
     * @note
     * - 忽略首尾空白字符
     * - 要求整个字符串为合法数字表示，**不允许部分匹配**（如 "123abc" 会失败）
     * - 对于任何非合法数字格式的输入（除空白外），均返回 std::nullopt
     * - 浮点数不保证支持 "inf" 或 "nan"
     *
     * @example
     *   ToNumber<int>("42") = 42
     *   ToNumber<int>("  0xFF ") = 255
     *   ToNumber<double>("3.14") = 3.14
     *   ToNumber<int>("") = nullopt
     *   ToNumber<int>("abc") = nullopt
     *   ToNumber<int>("123abc") = nullopt （非完全解析）
     */
    template <typename T>
    static std::optional<T> ToNumber( std::string_view str ) noexcept {
        static_assert( std::is_arithmetic_v<T>, "ToNumber only supports arithmetic types" );
        if ( str.empty() ) {
            return std::nullopt;
        }
        str = Trim( str );
        if ( str.empty() ) {
            return std::nullopt;
        }
        T           value{};
        const char *begin      = str.data();
        const char *end        = begin + str.size();
        const char *parsed_end = begin;
        if constexpr ( std::is_integral_v<T> ) {
            int base = DetectBase( str );
            if ( base == 16 ) {
                // 处理十六进制，跳过 "0x" 或 "0X" 前缀
                begin += 2;
            }
            const auto [ptr, ec] = std::from_chars( begin, end, value, base );
            if ( ec != std::errc{} || ptr == begin ) {
                return std::nullopt;
            }
            parsed_end = ptr;
        }
        else if constexpr ( std::is_floating_point_v<T> ) {
            const auto [ptr, ec] = std::from_chars( begin, end, value );
            if ( ec != std::errc{} || ptr == begin ) {
                return std::nullopt;
            }
            parsed_end = ptr;
        }
        if ( parsed_end != end ) {
            return std::nullopt;
        }
        return value;
    }
    /**
     * @brief 将数值类型转换为字符串
     *
     * 整型支持 8、10、16 进制，浮点型支持指定小数位数（%f）或自动格式（%g）
     *
     * @tparam T 数值类型（必须是算术类型）
     * @param value 输入数值
     * @param base 整型进制（仅整型有效），可选 8、10、16；默认 10
     * @param precision 小数位数（仅浮点型有效），-1 表示自动精度（%g），>=0 表示固定小数位（%f）
     * @return std::string 转换成功则返回结果；失败（如参数非法、缓冲区溢出等）则返回空字符串 ""
     */
    template <typename T>
    static std::string FromNumber( T value, int base = 10, int precision = -1 ) noexcept {
        static_assert( std::is_arithmetic_v<T>, "FromNumber only supports arithmetic types" );
        if constexpr ( std::is_integral_v<T> ) {
            char                 buffer[64]{};
            std::to_chars_result result;
            switch ( base ) {
                case 8:
                    result = std::to_chars( buffer, buffer + sizeof( buffer ), value, 8 );
                    break;
                case 10:
                    result = std::to_chars( buffer, buffer + sizeof( buffer ), value );
                    break;
                case 16:
                    result = std::to_chars( buffer, buffer + sizeof( buffer ), value, 16 );
                    break;
                default:
                    return {};
            }
            if ( result.ec != std::errc{} ) {
                return {};
            }
            return std::string( buffer, static_cast<size_t>( result.ptr - buffer ) );
        }
        else if constexpr ( std::is_floating_point_v<T> ) {
            std::ostringstream oss;
            try {
                if ( precision >= 0 ) {
                    oss << std::fixed << std::setprecision( precision ) << value;
                }
                else {
                    oss << std::defaultfloat << value;
                }
                const std::string &str = oss.str();
                return str.empty() ? std::string{} : str;
            }
            catch ( ... ) {
                return {};
            }
        }
        return {};
    }
    /**
     * @brief 提取字符串片段
     *
     * @param str 源字符串
     * @param start 起始字符串
     * @param end 结束字符串
     * @param include_start 返回值是否包含起始字符串
     * @param include_end 返回值是否包含结束字符串
     * @return std::optional<std::string>
     */
    static std::optional<std::string_view> ExtractBetween( std::string_view str, std::string_view start,
                                                           std::string_view end, bool include_start = false,
                                                           bool include_end = false );
    /**
     * @brief 提取文本中第一个完整匹配的子串
     *
     * 使用指定正则表达式搜索输入文本，返回首个完整匹配结果。
     * 若未找到匹配项或正则编译失败，则返回空 optional。
     *
     * @param text 输入文本（支持 std::string_view 避免拷贝）
     * @param pattern 正则表达式模式字符串
     * @param error_msg [out] 可选参数，用于接收错误描述信息；
     *                        若无错误则保持为空；传入 nullptr 表示忽略错误信息
     *
     * @return 匹配成功时返回 std::optional<std::string> 含值状态；
     *         否则返回 std::nullopt（可能是无匹配或正则非法）
     */
    static std::optional<std::string> ExtractFirst( std::string_view text, std::string_view pattern,
                                                    std::string *error_msg = nullptr );
    /**
     * @brief 提取第一个匹配中的指定捕获组内容
     *
     * 使用带括号捕获组的正则表达式，提取第 group_index 个捕获组的内容。
     * 捕获组索引从 0 开始，但通常使用 1 表示第一个有意义的子表达式。
     *
     * @param text 输入文本
     * @param pattern 包含捕获组的正则表达式，例如 R"(User: (\w+))"
     * @param group_index 要提取的捕获组索引，默认为 1
     * @param error_msg [out] 错误信息输出指针（可为空）
     *
     * @return 成功提取则返回字符串；若无匹配、越界或正则错误则返回 nullopt
     *
     * @note 如果 group_index >= match.size()，会设置越界错误信息
     */
    static std::optional<std::string> ExtractGroup( std::string_view text, std::string_view pattern,
                                                    size_t group_index = 1, std::string *error_msg = nullptr );
    /**
     * @brief 全局提取所有完整匹配项（非重叠）
     *
     * 在整个文本中查找所有符合正则表达式的子串，返回列表。
     * 匹配是按顺序进行的，不重叠。
     *
     * @param text 输入文本
     * @param pattern 正则表达式模式
     * @param error_msg [out] 接收可能的编译或执行错误信息
     *
     * @return 匹配到的所有字符串组成的 vector；
     *         若正则无效则返回空 vector，并通过 error_msg 通知错误
     */
    static std::vector<std::string> ExtractAll( std::string_view text, std::string_view pattern,
                                                std::string *error_msg = nullptr );
    /**
     * @brief 全局提取所有匹配中的指定捕获组内容
     *
     * 遍历所有匹配结果，提取每个匹配中指定索引的捕获组。
     * 若某些匹配缺少该捕获组（如条件捕获），会在 error_msg 中提示。
     *
     * @param text 输入文本
     * @param pattern 带捕获组的正则表达式
     * @param group_index 要提取的捕获组索引（默认为 1）
     * @param error_msg [out] 错误或警告信息输出
     *
     * @return 所有成功提取的捕获组字符串列表；
     *         若正则本身错误则返回空列表
     */
    static std::vector<std::string> ExtractAllGroups( std::string_view text, std::string_view pattern,
                                                      size_t group_index = 1, std::string *error_msg = nullptr );

private:
    /// 清除错误输出缓冲区
    static void ClearError( std::string *error_msg );
    /// 设置错误信息（内部工具，支持变参拼接）
    template <typename... Args>
    static void SetError( std::string *error_msg, Args &&...args ) {
        if ( error_msg ) {
            error_msg->clear();
            ( ..., ( error_msg->append( args ) ) );
        }
    }
    /// 检测整数进制：支持 0x（16进制）、0（8进制）、其余为10进制
    static int DetectBase( std::string_view str ) noexcept;
};

}  // namespace utils