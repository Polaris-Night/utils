#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
#include "WindowPolicy.h"

namespace utils {

/**
 * @brief 滚动窗口（Tumbling）：按规则收尾输出，输出后清空并继续收集
 *
 * 统一模型（与 SlidingWindow 族共享 WindowPolicy 的维度词汇）：
 *   - 结构边界（硬边界）：时间桶（span）与会话间隙（session_gap），在数据入窗前判定；
 *   - 收尾条件（可组合）：条数 / 权重 / 谓词 / 自定义，按 combine 组合
 *     （Any：任一命中即收尾；All：全部满足才收尾）；
 *   - 三个驱动入口：
 *       Add(item)     数据驱动（计数/权重/谓词/自定义，以及数据时间跨桶）；
 *       Poll(now)     缺席驱动（时间推进到 now、会话空闲超时）——没有新数据时也能收尾；
 *       Close()       显式收尾（手动结算 / EOF / 退出前）。
 *
 * 语义细节：
 *   - 时间桶在第一条数据或首次 Poll 时建立锚点；迟到数据（早于当前桶起点）被丢弃并计数；
 *   - emit_empty=true 时，时间推进跨过的空桶逐一输出（受 max_catchup 限制）；
 *   - 同一时间桶内允许因条数/权重等条件多次收尾（部分聚合），桶起点保持不变；
 *   - 非线程安全，多线程使用需外部同步；回调在调用线程同步执行（锁外世界由调用方保证）。
 *
 * @tparam T     元素类型
 * @tparam TimeT 时间点类型（time_t / 整数 tick / std::chrono::time_point 均可）
 */
template <typename T, typename TimeT>
class TumblingWindow {
public:
    using Policy        = WindowPolicy<T, TimeT>;
    using Duration      = typename Policy::Duration;
    using ItemList      = std::vector<T>;
    using CloseCallback = std::function<void( TimeT bucket_start, const ItemList &items )>;
    using ListCallback  = std::function<void( const ItemList &items )>;

    explicit TumblingWindow( Policy policy ) : policy_( std::move( policy ) ) {
        policy_.Validate();
        if ( policy_.count && *policy_.count == 0 ) {
            throw std::invalid_argument( "TumblingWindow: count must be > 0" );
        }
    }

    /**
     * @brief 添加数据（数据驱动）
     *
     * 先处理结构边界（跨时间桶 / 会话断裂），再入窗并检查收尾条件。
     * 迟到数据（早于当前桶起点）不入窗，计入 LateCount()。
     */
    void Add( T item ) {
        const TimeT key = policy_.KeyOf( item );

        // ① 时间桶：初始化 / 迟到判定 / 跨桶收尾
        if ( policy_.span ) {
            EnsureBucket( key );
            if ( key < *bucket_start_ ) {
                ++late_count_;
                return;
            }
            MoveToBucket( BucketOf( key ) );
        }

        // ② 会话间隙：断会话时先收尾旧会话
        if ( policy_.session_gap && last_key_ &&
             SessionExpired( *last_key_, key, *policy_.session_gap ) ) {
            CloseBucket();
            last_key_.reset();
            if ( policy_.span ) {
                bucket_start_   = BucketOf( key );
                bucket_emitted_ = false;
            }
        }

        // ③ 入窗（无时间桶时，用本批首条数据的时间点作为桶标签）
        if ( !policy_.span && !bucket_start_ ) {
            bucket_start_   = key;
            bucket_emitted_ = false;
        }
        if ( !on_add_.empty() ) {
            NotifyAdd( ItemList{ item } );  // 仅在有监听者时拷贝一次
        }
        items_.push_back( std::move( item ) );
        bucket_weight_ += policy_.WeightOf( items_.back() );
        last_key_ = key;

        // ④ 数据驱动收尾（时间桶起点保持不变，允许同桶多次输出）
        if ( DataDrivenClose( items_.back(), key ) ) {
            CloseBucket();
        }
    }

    /**
     * @brief 推进时间（缺席驱动）
     *
     * 时间桶推进到 now 所在桶（可补发空桶）；配置了 session_gap 时，
     * 空闲超过间隙即收尾当前会话。
     *
     * @return 本次收尾的桶数（含补发的空桶与会话收尾）
     */
    std::size_t Poll( TimeT now ) {
        std::size_t closed = 0;
        if ( policy_.span ) {
            if ( !bucket_start_ ) {
                EnsureBucket( now );  // 首次推进：以 now 建立锚点（不输出）
            }
            else {
                closed += MoveToBucket( BucketOf( now ) );
            }
        }

        if ( policy_.session_gap && last_key_ &&
             SessionExpired( *last_key_, now, *policy_.session_gap ) ) {
            if ( CloseBucket() ) {
                ++closed;
            }
            last_key_.reset();
            if ( policy_.span ) {
                bucket_start_.reset();
                first_bucket_start_.reset();
                bucket_emitted_ = false;
            }
        }
        return closed;
    }
    /**
     * @brief 显式收尾（手动结算 / EOF / 退出前）
     *
     * 输出当前桶内数据（为空则不输出），并重置分桶状态；下一次 Add/Poll 重新建立锚点。
     */
    void Close() {
        CloseBucket();
        bucket_start_.reset();
        first_bucket_start_.reset();
        last_key_.reset();
        bucket_emitted_ = false;
        bucket_weight_  = 0.0;
    }

    /**
     * @brief 丢弃当前桶数据但不输出（通过 OnRemove 通知）
     *
     * 时间桶边界保持不变（仅清空内容）。
     */
    void Clear() {
        if ( !items_.empty() ) {
            ItemList discarded = std::move( items_ );
            items_.clear();
            NotifyRemove( discarded );
        }
        bucket_weight_ = 0.0;
        last_key_.reset();
    }

    [[nodiscard]] std::size_t Size() const noexcept { return items_.size(); }

    [[nodiscard]] bool Empty() const noexcept { return items_.empty(); }

    /// 被丢弃的迟到数据条数
    [[nodiscard]] std::size_t LateCount() const noexcept { return late_count_; }

    /// 当前桶起点（尚未建立桶时返回 std::nullopt）
    [[nodiscard]] std::optional<TimeT> BucketStart() const { return bucket_start_; }

    /// 当前桶内聚合权重
    [[nodiscard]] double CurrentWeight() const noexcept { return bucket_weight_; }

    [[nodiscard]] const Policy &GetPolicy() const noexcept { return policy_; }

    /// 数据进入窗口（单条，按事件逐条通知）
    void OnAdd( ListCallback callback ) { on_add_.push_back( std::move( callback ) ); }

    /// 数据被丢弃（Clear）时通知
    void OnRemove( ListCallback callback ) { on_remove_.push_back( std::move( callback ) ); }

    /// 桶收尾输出：参数为桶起点与桶内数据
    void OnClose( CloseCallback callback ) { on_close_.push_back( std::move( callback ) ); }

private:
    void EnsureBucket( TimeT key ) {
        if ( bucket_start_ ) {
            return;
        }
        if ( policy_.align == WindowAlign::FirstItem ) {
            first_bucket_start_ = key;
            bucket_start_       = key;
        }
        else {
            bucket_start_ = AlignTo( key, *policy_.span, policy_.origin );
        }
        bucket_emitted_ = false;
    }

    [[nodiscard]] TimeT BucketOf( TimeT t ) const {
        if ( policy_.align == WindowAlign::FirstItem ) {
            return AlignTo( t, *policy_.span, first_bucket_start_.value_or( t ) );
        }
        return AlignTo( t, *policy_.span, policy_.origin );
    }

    std::size_t MoveToBucket( TimeT target ) {
        if ( !bucket_start_ || !policy_.span ) {
            return 0;
        }
        if ( !( *bucket_start_ < target ) ) {
            return 0;  // 不回拨（墙钟/时间轴回退时保持现状）
        }

        std::size_t closed = 0;
        if ( !items_.empty() ) {
            (void)CloseBucket();
            ++closed;
        }
        else if ( policy_.emit_empty && !bucket_emitted_ ) {
            EmitBucket( *bucket_start_, ItemList{} );  // 当前空桶也要输出
            ++closed;
        }

        if ( policy_.emit_empty ) {
            std::size_t emitted = 0;
            for ( TimeT b = *bucket_start_ + *policy_.span; b < target && emitted < policy_.max_catchup;
                  b += *policy_.span ) {
                EmitBucket( b, ItemList{} );
                ++emitted;
                ++closed;
            }
        }

        bucket_start_   = target;
        bucket_emitted_ = false;
        return closed;
    }

    bool CloseBucket() {
        if ( items_.empty() ) {
            return false;
        }
        EmitBucket( bucket_start_.value_or( TimeT{} ), items_ );
        items_.clear();
        bucket_weight_  = 0.0;
        bucket_emitted_ = true;
        if ( !policy_.span ) {
            bucket_start_.reset();  // 无时间桶：下一批重新以首条数据为标签
        }
        return true;
    }

    void EmitBucket( TimeT start, const ItemList &items ) {
        for ( auto &callback : on_close_ ) {
            if ( callback ) {
                callback( start, items );
            }
        }
    }

    [[nodiscard]] bool DataDrivenClose( const T &item, TimeT key ) const {
        const bool has_count  = policy_.count.has_value();
        const bool has_weight = policy_.weight.has_value();
        const bool has_pred   = static_cast<bool>( policy_.predicate );
        const bool has_custom = static_cast<bool>( policy_.custom );

        if ( !has_count && !has_weight && !has_pred && !has_custom ) {
            return false;
        }

        if ( policy_.combine == RuleCombine::Any ) {
            if ( has_count && items_.size() >= *policy_.count ) {
                return true;
            }
            if ( has_weight && bucket_weight_ >= *policy_.weight ) {
                return true;
            }
            if ( has_pred && policy_.predicate( item ) ) {
                return true;
            }
            return has_custom && policy_.custom( item, key );
        }

        if ( has_count && items_.size() < *policy_.count ) {
            return false;
        }
        if ( has_weight && bucket_weight_ < *policy_.weight ) {
            return false;
        }
        if ( has_pred && !policy_.predicate( item ) ) {
            return false;
        }
        return !has_custom || policy_.custom( item, key );
    }

    void NotifyAdd( const ItemList &items ) const {
        const std::size_t n = on_add_.size();  // 快照：回调中注册的新监听从下一事件开始生效
        for ( std::size_t i = 0; i < n; ++i ) {
            if ( on_add_[i] ) {
                on_add_[i]( items );
            }
        }
    }

    void NotifyRemove( const ItemList &items ) const {
        const std::size_t n = on_remove_.size();
        for ( std::size_t i = 0; i < n; ++i ) {
            if ( on_remove_[i] ) {
                on_remove_[i]( items );
            }
        }
    }

private:
    Policy              policy_;
    ItemList            items_;
    double              bucket_weight_{ 0.0 };
    std::optional<TimeT> last_key_;
    std::optional<TimeT> bucket_start_;
    std::optional<TimeT> first_bucket_start_;
    bool                bucket_emitted_{ false };
    std::size_t         late_count_{ 0 };

    std::vector<ListCallback>  on_add_;
    std::vector<ListCallback>  on_remove_;
    std::vector<CloseCallback> on_close_;
};

}  // namespace utils
