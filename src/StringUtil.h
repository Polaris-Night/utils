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
     * @return std::vector<std::string> 返回子串的向量
     */
    [[nodiscard]] static std::vector<std::string> Split( std::string_view str, std::string_view separator,
                                                         bool skip_empty = false ) noexcept;
    /**
     * @brief 将字符串按指定分隔符拆分为子串引用列表
     *
     * @param str 字符串
     * @param separator 分隔符
     * @param skip_empty 是否跳过空子串
     * @return std::vector<std::string_view> 子串引用向量
     *
     * @example
     *   - SplitRef("a,,b,c", ",", false) → {"a", "", "b", "c"}
     *   - SplitRef("a,,b,c", ",", true)  → {"a", "b", "c"}
     *   - SplitRef("abc", "", false)     → {"", "a", "b", "c", ""}
     */
    [[nodiscard]] static std::vector<std::string_view> SplitRef( std::string_view str, std::string_view separator,
                                                                 bool skip_empty = false ) noexcept;
    /**
     * @brief 替换第一个匹配的子串
     * @param str 源字符串
     * @param from 要被替换的内容
     * @param to 替换后的内容
     * @return 替换后的字符串副本
     */
    [[nodiscard]] static std::string ReplaceFirst( std::string_view str, std::string_view from,
                                                   std::string_view to ) noexcept;
    /**
     * @brief 全局替换所有匹配的子串
     * @param str 源字符串
     * @param from 要被替换的内容
     * @param to 替换后的内容
     * @return 所有匹配替换后的字符串
     *
     * @note 注意避免重叠问题导致无限循环（已内部规避）
     */
    [[nodiscard]] static std::string ReplaceAll( std::string_view str, std::string_view from,
                                                 std::string_view to ) noexcept;
    /**
     * @brief 对字符串中的特殊字符进行 C 风格转义
     *
     * 转义下列字符：
     *   \\n → \\n
     *   \\r → \\r
     *   \\t → \\t
     *   \\\\ → \\\\
     *   \\\" → \\\"
     *   \\' → \\\'
     *   其他不可打印字符（< 32 或 > 126）→ \\xHH 形式
     *
     * @param str 输入字符串（通常为 ASCII 或 UTF-8）
     * @return 转义后的字符串
     *
     * @note
     *   - 仅对控制字符和引号进行转义
     *   - UTF-8 多字节字符不会被拆解（因为其字节值 > 127）
     *   - 输出总是可打印 ASCII
     *
     * @example
     *   EscapeC("Hello\nWorld\"") → "Hello\\nWorld\\\""
     */
    [[nodiscard]] static std::string EscapeC( std::string_view str ) noexcept;
    /**
     * @brief 解码 C 风格转义字符序列
     *
     * 支持以下转义：
     *   \\n, \\r, \\t, \\\\\\, \\\", \\\', \\xHH（十六进制）
     *
     * @param str 已转义的字符串
     * @return 原始二进制数据或文本
     *
     * @note
     *   - 非法 \\xHH 忽略反斜杠保留原样
     *   - 连续两个反斜杠变成一个 \\
     *   - 不支持八进制转义（\\123）
     *   - 对未知转义符（如 \\z）保留反斜杠和字符
     *
     * @example
     *   UnescapeC("Hello\\nWorld") → "Hello\nWorld"
     *   UnescapeC("Value:\\x20\\xFF") → "Value: \xFF"
     */
    [[nodiscard]] static std::string UnescapeC( std::string_view str ) noexcept;
    /**
     * @brief 使用通配符模式匹配字符串（支持 * 和 ?）
     *
     * 支持：
     *   - `?` 匹配任意单个字符
     *   - `*` 匹配任意数量字符（包括零个）
     *
     * @param str 待匹配字符串
     * @param pattern 通配符模式（如 "*.txt", "file?.dat"）
     * @return true 如果完全匹配
     *
     * @example
     *   WildcardMatch("config.ini", "*.ini")     → true
     *   WildcardMatch("data1.dat", "data?.dat") → true
     *   WildcardMatch("abc", "a*c")              → true
     *   WildcardMatch("acd", "ab*d")             → false
     */
    [[nodiscard]] static bool WildcardMatch( std::string_view str, std::string_view pattern ) noexcept;
    /**
     * @brief 在指定字节单位之间进行数值转换（基于 1024 进制）
     *
     * 将一个带源单位的数值转换为目标单位下的等效值。
     * 所有单位基于 1024 换算（即 1 KB = 1024 B, 1 MB = 1024 KB）。
     *
     * @param value 数值
     * @param from_unit 源单位，支持 "B", "KB", "MB", "GB", "TB"（不区分大小写）
     * @param to_unit 目标单位，同上
     * @return 转换后的数值；若单位无效或计算出错则返回 -1.0
     *
     * @example
     *   ConvertByteUnit(1.0, "MB", "KB") → 1024.0
     *   ConvertByteUnit(512, "KB", "B")  → 524288.0
     *   ConvertByteUnit(2.5, "GB", "MB") → 2560.0
     */
    [[nodiscard]] static double ConvertByteUnit( double value, std::string_view from_unit,
                                                 std::string_view to_unit ) noexcept;
    /**
     * @brief 将字节数转换为可读格式的字符串
     *
     * 可将字节数格式化为带单位的可读字符串。例如：
     *   - 自动模式：选择最合适单位（KB, MB...）
     *   - 指定单位：强制输出为某单位（如 always in "MB"）
     *
     * @param bytes 字节数（以 B 为单位）
     * @param precision 小数点后保留位数（0 ~ 6），默认 2
     * @param target_unit 目标单位（如 "MB"）；若为空或无效，则自动选择最合适单位
     * @return 格式化字符串，例如 "1.23 MB"
     *
     * @example
     *   HumanizeBytes(1024)             → "1.00 KB"
     *   HumanizeBytes(1024, 1, "KB")    → "1.0 KB"
     *   HumanizeBytes(1572864, 1, "MB") → "1.5 MB"
     *   HumanizeBytes(999, 2, "XYZ")    → "999.00 B" （无效单位 → 自动）
     */
    [[nodiscard]] static std::string HumanizeBytes( uint64_t bytes, int precision = 2,
                                                    std::string_view target_unit = {} ) noexcept;
    /**
     * @brief 提取字符串左边指定长度的子字符串
     * @param str 输入字符串引用
     * @param len 要提取的长度，如果超过字符串长度则返回整个字符串
     * @return 返回左边子字符串的拷贝
     */
    [[nodiscard]] static std::string Left( std::string_view str, size_t len ) noexcept;
    /**
     * @brief 提取字符串左边指定长度的子字符串引用
     * @param str 输入字符串引用
     * @param len 要提取的长度，如果超过字符串长度则返回整个字符串引用
     * @return 返回左边子字符串引用
     */
    [[nodiscard]] static std::string_view LeftRef( std::string_view str, size_t len ) noexcept;
    /**
     * @brief 从指定位置开始提取子字符串
     * @param str 输入字符串引用
     * @param pos 起始位置，如果超出字符串长度则返回空字符串
     * @param len 要提取的长度，如果为npos则提取到字符串末尾
     * @return 返回子字符串的拷贝
     */
    [[nodiscard]] static std::string Mid( std::string_view str, size_t pos,
                                          size_t len = std::string_view::npos ) noexcept;
    /**
     * @brief 从指定位置开始提取子字符串引用
     * @param str 输入字符串引用
     * @param pos 起始位置，如果超出字符串长度则返回空字符串引用
     * @param len 要提取的长度，如果为npos则提取到字符串末尾
     * @return 返回子字符串引用
     */
    [[nodiscard]] static std::string_view MidRef( std::string_view str, size_t pos,
                                                  size_t len = std::string_view::npos ) noexcept;
    /**
     * @brief 提取字符串右边指定长度的子字符串
     * @param str 输入字符串引用
     * @param len 要提取的长度，如果超过字符串长度则返回整个字符串
     * @return 返回右边子字符串的拷贝
     */
    [[nodiscard]] static std::string Right( std::string_view str, size_t len ) noexcept;
    /**
     * @brief 提取字符串右边指定长度的子字符串引用
     * @param str 输入字符串引用
     * @param len 要提取的长度，如果超过字符串长度则返回整个字符串引用
     * @return 返回右边子字符串引用
     */
    [[nodiscard]] static std::string_view RightRef( std::string_view str, size_t len ) noexcept;
    /**
     * @brief 去除" \n\r\t\v\f"
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string Trim( std::string_view str ) noexcept;
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
        static_assert( sizeof...( args ) > 0, "Join requires at least one argument" );
        static_assert( ( IsStringType_v<Args>, ... ), "Join requires argument is string type" );
        std::ostringstream oss;
        bool               first = true;

        ( ( oss << ( first ? "" : separator ) << std::forward<Args>( args ), first = false ), ... );

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
     * @brief 可变参数格式化字符串 (C风格格式化)
     * @tparam Args 可变参数类型
     * @param format 格式化字符串
     * @param args 参数列表
     * @return 格式化后的字符串
     * @throw std::runtime_error 当格式化失败时抛出异常
     */
    template <typename... Args>
    [[nodiscard]] static std::string FormatCString( std::string_view format, Args &&...args ) {
        auto size = std::snprintf( nullptr, 0, format.data(), std::forward<Args>( args )... );
        if ( size < 0 ) {
            throw std::runtime_error( "Format string error in StringUtil::FormatCString" );
        }
        auto buffer = std::make_unique<char[]>( size + 1 );
        std::snprintf( buffer.get(), size + 1, format.data(), std::forward<Args>( args )... );
        return std::string( buffer.get(), size );
    }
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
    static std::string ToUpper( std::string_view str );
    /**
     * @brief 字符串转小写
     *
     * @param str 字符串
     * @return std::string
     */
    static std::string ToLower( std::string_view str );
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
     * @brief 忽略大小写的字符串比较
     *
     * @param str1 第一个字符串
     * @param str2 第二个字符串
     * @return true 如果两个字符串相等（忽略大小写）
     * @return false 如果两个字符串不相等
     */
    static bool EqualsIgnoreCase( std::string_view str1, std::string_view str2 ) noexcept;
    /**
     * @brief 转换为二进制字符串
     *
     * @param value 数值
     * @param count 二进制位数
     * @return std::string
     */
    static std::string IntToBitString( uint64_t value, int count ) noexcept;
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
    /**
     * @brief 在字符串左侧填充字符至指定长度
     * @param str 原字符串
     * @param width 目标长度
     * @param fill_char 填充字符
     * @return 填充后的字符串
     */
    [[nodiscard]] static std::string PadLeft( std::string_view str, size_t width, char fill_char = ' ' ) noexcept;
    /**
     * @brief 在字符串右侧填充字符至指定长度
     * @param str 原字符串
     * @param width 目标长度
     * @param fill_char 填充字符
     * @return 填充后的字符串
     */
    [[nodiscard]] static std::string PadRight( std::string_view str, size_t width, char fill_char = ' ' ) noexcept;
    /**
     * @brief 生成随机字符串
     * @param length 字符串长度
     * @param charset 字符集
     * @return 随机字符串
     */
    [[nodiscard]] static std::string RandomString(
        size_t           length,
        std::string_view charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789" ) noexcept;
    /**
     * @brief 检查字符串是否为数字
     * @param str 输入字符串
     * @return 是否为数字
     */
    static bool IsNumeric( std::string_view str ) noexcept;
    /**
     * @brief 检查字符串是否全为大写
     * @param str 输入字符串
     * @return 是否全为大写
     */
    static bool IsUpper( std::string_view str ) noexcept;
    /**
     * @brief 检查字符串是否全为小写
     * @param str 输入字符串
     * @return 是否全为小写
     */
    static bool IsLower( std::string_view str ) noexcept;

private:
    /// 清除错误输出缓冲区
    static void ClearError( std::string *error_msg ) noexcept;
    /// 设置错误信息（内部工具，支持变参拼接）
    template <typename... Args>
    static void SetError( std::string *error_msg, Args &&...args ) noexcept {
        if ( error_msg ) {
            error_msg->clear();
            ( ..., ( error_msg->append( std::forward<Args>( args ) ) ) );
        }
    }
    /// 检测整数进制：支持 0x（16进制）、0（8进制）、其余为10进制
    static int DetectBase( std::string_view str ) noexcept;
};

/**
 * @brief 字符串格式化工具类，支持类似QString::arg的格式化功能
 *
 * 该类支持使用 %1, %2, %3... 作为占位符的格式化字符串，
 * 并提供 Args 和 ArgsF 方法来替换这些占位符。
 * 如果提供的参数数量少于占位符数量，未对应的占位符将保留在结果中。
 *
 * 浮点数处理有两种方式：
 * 1. 使用 Args 方法：自动采用 %g 格式（std::defaultfloat），根据数值大小自动选择定点或科学计数法
 * 2. 使用 ArgsF 方法：可以指定小数位精度，采用定点表示法（std::fixed）
 *
 */
class StringFormat {
public:
    /**
     * @brief 构造格式化对象
     * @param format 格式化字符串，使用 %1, %2, %3... 作为占位符
     */
    explicit StringFormat( std::string_view format ) : format_( format ) {}

    /**
     * @brief 添加格式化参数，支持可变参数模板
     *
     * 自动将参数转换为字符串并替换对应的占位符。
     * 对于浮点数参数，使用 %g 格式（std::defaultfloat）自动选择最佳表示形式。
     *
     * @tparam TArgs 参数类型，支持整数、浮点数、布尔值和字符串类型
     * @param args 参数列表
     * @return 返回当前对象引用，支持链式调用
     *
     * @note 示例：
     *   StringFormat("Value is %1").Args(3.14159).ToString() = "Value is 3.14159" (使用%g格式)
     *   StringFormat("Result: %1").Args(true).ToString() = "Result: true" (将布尔值转换为字符串)
     *   StringFormat("Name: %1").Args("John").ToString() = "Name: John" (将字符串参数转换为字符串)
     *   StringFormat("Count: %1").Args(0).ToString() = "Count: 0" (将整数参数转换为字符串)
     *
     * @note 特殊情况：
     *   - 如果占位符数量多于参数数量，多余的占位符将保留在结果中
     */
    template <typename... TArgs>
    StringFormat &Args( TArgs &&...args ) {
        static_assert( sizeof...( args ) > 0, "Args requires at least one argument" );
        ( ( ReplacePlaceholder( ConvertToString( std::forward<TArgs>( args ) ) ) ), ... );
        return *this;
    }

    /**
     * @brief 添加浮点型格式化参数并指定精度
     *
     * 专门用于浮点数格式化，允许指定小数位数。
     * 使用定点表示法（std::fixed）格式化浮点数。
     *
     * @tparam FloatArgs 浮点型参数类型，必须是 float 或 double 类型
     * @param precision 小数位精度
     * @param args 浮点型参数列表
     * @return 返回当前对象引用，支持链式调用
     *
     * @note 示例：
     *   StringFormat("Price: %1").ArgsF(2, 12.3456).ToString() = "Price: 12.35" (保留2位小数)
     *
     * @note 特殊情况：
     *   - 如果占位符数量多于参数数量，多余的占位符将保留在结果中
     */
    template <typename... FloatArgs>
    StringFormat &ArgsF( int precision, FloatArgs... args ) {
        static_assert( sizeof...( args ) > 0, "ArgsF requires at least one argument" );
        static_assert( ( std::is_floating_point_v<FloatArgs> && ... ),
                       "All FloatArgs must be floating point types (float or double)" );
        ( ( ReplacePlaceholder( ConvertFloatToString( args, precision ) ) ), ... );
        return *this;
    }

    /**
     * @brief 转换为最终的字符串结果
     * @return 格式化后的字符串
     */
    std::string ToString() const noexcept { return format_; }

private:
    std::string format_;
    size_t      current_arg_index_ = 1;

    // 内部辅助函数：替换格式字符串中的占位符
    void ReplacePlaceholder( const std::string &value ) {
        std::string placeholder = "%" + std::to_string( current_arg_index_ );
        size_t      pos         = 0;
        while ( ( pos = format_.find( placeholder, pos ) ) != std::string::npos ) {
            format_.replace( pos, placeholder.length(), value );
            pos += ( value.length() == 0 ) ? 1 : value.length();
        }
        current_arg_index_++;
    }

    // 内部辅助函数：将各种类型转换为字符串
    template <typename T>
    static std::string ConvertToString( const T &value ) {
        if constexpr ( std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_floating_point_v<T> ) {
            // 整数类型转换
            return std::to_string( value );
        }
        else if constexpr ( std::is_same_v<T, bool> ) {
            // 布尔类型转换
            return value ? "true" : "false";
        }
        else if constexpr ( std::is_convertible_v<T, std::string_view> ) {
            // 可转换为字符串视图的类型
            return std::string{ std::string_view{ value } };
        }
        else if constexpr ( std::is_convertible_v<T, std::string> ) {
            // 可转换为字符串的类型
            return value;
        }
        else if constexpr ( std::is_floating_point_v<T> ) {
            // 浮点类型转换，使用默认的 %g 格式
            std::ostringstream oss;
            oss << std::defaultfloat << value;
            return oss.str();
        }
        else {
            static_assert( std::is_arithmetic_v<T> || std::is_same_v<T, bool> ||
                               std::is_convertible_v<T, std::string_view> || std::is_convertible_v<T, std::string> ||
                               std::is_floating_point_v<T>,
                           "Type must be convertible to string or arithmetic type" );
            return std::to_string( value );
        }
    }

    // 特化浮点型转换函数
    template <typename FloatType>
    static std::string ConvertFloatToString( FloatType value, int precision ) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision( precision ) << value;
        return oss.str();
    }
};

}  // namespace utils