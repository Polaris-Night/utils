#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace utils {

/// 无时间维度窗口使用的默认时间点类型（仅用于模板占位）
using DefaultTimePoint = std::chrono::steady_clock::time_point;

/**
 * @brief 时间桶的对齐方式（滚动窗口分桶 / 组合规则窗口的时间跨度）
 */
enum class WindowAlign
{
    Epoch,      ///< 对齐到时间原点 TimeT{}（如 Unix epoch / tick 0），按跨度等分
    FirstItem,  ///< 对齐到窗口内第一条数据的时间点（边界随首条数据走）
    Explicit,   ///< 对齐到调用方给定的原点（WindowPolicy::origin）
};

/**
 * @brief 多个收尾条件的组合方式
 */
enum class RuleCombine
{
    Any,  ///< 任一条件命中即收尾（如"1 分钟或 1000 条，先到先算"）
    All,  ///< 全部条件满足才收尾
};

/**
 * @brief 两个时间点之间是否已经形成会话断裂
 *
 * @param last 会话内已知的最大时间点
 * @param now  当前时间点（早于 last 时不算断裂，避免无符号/回拨误判）
 * @param gap  会话间隙阈值
 */
template <typename TimeT, typename Duration>
[[nodiscard]] bool SessionExpired( TimeT last, TimeT now, Duration gap ) {
    return last < now && now - last > gap;
}

/**
 * @brief 时间点所在的桶起点
 *
 * t 早于 origin 时返回 origin（不做负向分桶）。
 * TimeT 可为 time_t / 整数 tick / std::chrono::time_point，
 * 只需满足 TimeT - TimeT = Duration、TimeT + Duration = TimeT。
 */
template <typename TimeT, typename Duration>
[[nodiscard]] TimeT AlignTo( TimeT t, Duration span, TimeT origin ) {
    if ( !( origin < t ) ) {
        return origin;
    }
    const Duration delta = t - origin;
    const auto     k     = delta / span;
    return origin + k * span;
}

/**
 * @brief 给定最新时刻与跨度，计算窗口保留的左边界（latest - span）
 *
 * 对无符号时间类型做饱和处理，避免下溢回绕导致误裁剪（如早期 tick 值小于跨度）。
 */
template <typename TimeT, typename Duration>
[[nodiscard]] TimeT CutoffTime( TimeT latest, Duration window ) {
    if constexpr ( std::is_unsigned_v<TimeT> ) {
        return window < latest ? static_cast<TimeT>( latest - window ) : TimeT{ 0 };
    }
    else {
        return latest - window;
    }
}

/**
 * @brief 窗口规则：两族共享的"维度"词汇
 *
 * 同一组维度有两种解释：
 *   - TumblingWindow：条件**命中** → 收尾输出（span 为桶宽）；
 *   - RuleWindow    ：条件**越界** → 淘汰旧数据（span 为保留跨度）。
 *
 * 结构边界（时间 span / 会话间隙）在收尾语义下是"硬边界"，
 * 先于数据入窗判定；其余条件（条数 / 权重 / 谓词 / 自定义）按 combine 组合。
 *
 * @tparam T     元素类型
 * @tparam TimeT 时间点类型
 */
template <typename T, typename TimeT>
struct WindowPolicy {
    using Duration    = decltype( std::declval<TimeT>() - std::declval<TimeT>() );
    using KeyFn       = std::function<TimeT( const T & )>;
    using WeightFn    = std::function<double( const T & )>;
    using PredicateFn = std::function<bool( const T & )>;
    using CustomFn    = std::function<bool( const T &, TimeT )>;

    // ---- 维度 ----
    std::optional<Duration>    span;         ///< 时间跨度：滚动=桶宽，滑动=保留区间
    WindowAlign                align{ WindowAlign::Epoch };
    TimeT                      origin{};     ///< align == Explicit 时的对齐原点
    std::optional<std::size_t> count;        ///< 条数（0 = 即进即出；TumblingWindow 要求 > 0）
    std::optional<double>      weight;       ///< 聚合权重上限（未提供 weight_fn 时每条权重为 1）
    std::optional<Duration>    session_gap;  ///< 会话间隙
    PredicateFn                predicate;    ///< 逐条条件（滚动：为真即收尾；滑动：为假即清空）
    CustomFn                   custom;       ///< 自定义收尾条件（可携带闭包状态）
    RuleCombine                combine{ RuleCombine::Any };

    // ---- 取值函数 ----
    KeyFn    key_fn;   ///< 提取时间点；未提供时统一为 TimeT{}
    WeightFn weight_fn;

    // ---- 输出策略（滚动窗口）----
    bool        emit_empty{ false };   ///< 时间推进跨过空桶时是否输出空桶
    std::size_t max_catchup{ 1024 };   ///< 单次推进最多补发多少个空桶（防长时间挂起后刷爆）

    [[nodiscard]] TimeT KeyOf( const T &item ) const {
        return key_fn ? key_fn( item ) : TimeT{};
    }

    [[nodiscard]] double WeightOf( const T &item ) const {
        return weight_fn ? weight_fn( item ) : 1.0;
    }

    /// 构造期校验：非法参数立即失败，避免运行期出现难以排查的行为
    void Validate() const {
        if ( span && !( Duration( 0 ) < *span ) ) {
            throw std::invalid_argument( "WindowPolicy: span must be > 0" );
        }
        // count == 0 合法：滑动语义为"即进即出"，滚动语义由 TumblingWindow 自行拒绝
        if ( weight && !( *weight >= 0.0 ) ) {
            throw std::invalid_argument( "WindowPolicy: weight must be >= 0" );
        }
        if ( session_gap && !( Duration( 0 ) < *session_gap ) ) {
            throw std::invalid_argument( "WindowPolicy: session_gap must be > 0" );
        }
        if ( max_catchup == 0 ) {
            throw std::invalid_argument( "WindowPolicy: max_catchup must be > 0" );
        }
        if ( align == WindowAlign::FirstItem && !span ) {
            throw std::invalid_argument( "WindowPolicy: align == FirstItem requires span" );
        }
    }
};

}  // namespace utils
