#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "WindowPolicy.h"

namespace utils {

/**
 * @brief 滑动窗口抽象基类
 *
 * 定义滑动窗口的最小契约：添加、长度查询、清空。
 * 子类实现各自的淘汰策略与底层数据结构。
 *
 * 统一约定：
 *   - 数据变动通过 on_add / on_remove 回调对外通知，元素为变动批次（批量通知）；
 *   - 状态先更新、后通知；回调载荷中的元素类型为 T 的拷贝/移动结果；
 *   - Clear() 会将全部数据作为一批移除并通知（空窗口不触发通知）；
 *   - 回调在触发线程同步执行，重入 Add() 会同步递归触发通知；
 *     回调中注册新监听是安全的，新监听从下一事件起生效；
 *   - Poll(now)：在"没有新数据"时推进时间（惰性裁剪 / 会话超时），是与 Add 并列的
 *     "缺席驱动"入口，返回本次处理的单位数；无时间维度的窗口为空操作（返回 0）；
 *   - 数据离开窗口统一走 on_remove；"会话完成"等语义事件另经 Close() 通知（见 SessionWindow）；
 *   - 窗口非线程安全，多线程访问需外部同步。
 *
 * @tparam T 数据元素类型
 */
template <typename T>
class SlidingWindow {
public:
    using ItemList       = std::vector<T>;
    using AddCallback    = std::function<void( const ItemList & )>;
    using RemoveCallback = std::function<void( const ItemList & )>;

    virtual ~SlidingWindow() = default;

    /**
     * @brief 添加一条数据到窗口，必要时按各自的淘汰策略移出旧数据
     */
    virtual void Add( T item ) = 0;

    /**
     * @brief 当前窗口内的数据条数
     */
    [[nodiscard]] virtual std::size_t Size() const noexcept = 0;

    /**
     * @brief 窗口是否为空
     */
    [[nodiscard]] bool Empty() const noexcept { return Size() == 0; }

    /**
     * @brief 清空窗口
     *
     * 注意：清空会作为“批量移除”触发 on_remove 回调（窗口为空时不触发）。
     */
    virtual void Clear() = 0;

    /**
     * @brief 注册数据加入监听（可注册多个，按注册顺序触发）
     *
     * @param callback 只接收本次加入数据的回调
     */
    void OnAdd( AddCallback callback ) { on_add_.push_back( std::move( callback ) ); }

    /**
     * @brief 注册数据移除监听（可注册多个，按注册顺序触发）
     *
     * @param callback 接收被移除数据批次的回调
     */
    void OnRemove( RemoveCallback callback ) { on_remove_.push_back( std::move( callback ) ); }

protected:
    void NotifyAdd( const ItemList &items ) const {
        if ( items.empty() ) {
            return;
        }
        // 索引式遍历 + 数量快照：回调中注册新监听不会使迭代器失效，
        // 且新监听从下一事件起生效
        const std::size_t n = on_add_.size();
        for ( std::size_t i = 0; i < n; ++i ) {
            if ( on_add_[i] ) {
                on_add_[i]( items );
            }
        }
    }

    void NotifyRemove( const ItemList &items ) const {
        if ( items.empty() ) {
            return;
        }
        const std::size_t n = on_remove_.size();
        for ( std::size_t i = 0; i < n; ++i ) {
            if ( on_remove_[i] ) {
                on_remove_[i]( items );
            }
        }
    }

protected:
    std::vector<AddCallback>    on_add_;
    std::vector<RemoveCallback> on_remove_;
};

/**
 * @brief 组合规则滑动窗口：多个维度同时生效（时间 + 条数 + 权重），越界即淘汰最旧
 *
 * 这是滑动族的统一引擎，与 TumblingWindow 共享同一份 WindowPolicy，解释相反：
 *   - TumblingWindow ：条件**命中** ⇒ 收尾输出（span 为桶宽）；
 *   - RuleWindow     ：条件**越界** ⇒ 从最旧一端淘汰（span 为保留跨度）。
 *
 * 可表达"保留最近 5 秒且最多 1000 条、总权重不超过 64KB"这类组合约束；
 * CountWindow / TimeWindow / ThresholdWindow 都是它的单维度便捷特例。
 *
 * 时间维度的"最新时刻"：
 *   - Add 时取最新数据的时间点（有序插入并缓存 key / weight）；
 *   - Poll(now) 时取 now，因此**空闲时也能淘汰过期数据**，不必等下一条数据。
 *
 * 注意：session_gap / predicate / custom 属于"全清"或"收尾"语义，
 * 不参与本窗口的淘汰组合，分别由 SessionWindow / PredicateWindow / TumblingWindow 承担。
 *
 * 非线程安全，多线程使用需外部同步。
 */
template <typename T, typename TimeT>
class RuleWindow : public SlidingWindow<T> {
public:
    using Policy   = WindowPolicy<T, TimeT>;
    using Duration = typename Policy::Duration;
    using KeyFn    = typename Policy::KeyFn;
    using ItemList = typename SlidingWindow<T>::ItemList;

    struct Entry {
        T      item;
        TimeT  key{};
        double weight{ 0.0 };
    };
    using Storage = std::vector<Entry>;

    /**
     * @brief 构造
     *
     * @param policy        规则（时间 / 条数 / 权重，至少配置一项）
     * @param time_cooldown 两次时间裁剪之间的最小时间推进量（Duration{} 表示每次检查）
     */
    explicit RuleWindow( Policy policy, Duration time_cooldown = Duration{} ) :
        policy_( std::move( policy ) ), time_cooldown_( time_cooldown ) {
        policy_.Validate();
        if ( policy_.session_gap || policy_.predicate || policy_.custom ) {
            throw std::invalid_argument(
                "RuleWindow: session_gap / predicate / custom are not supported"
                " (use SessionWindow / PredicateWindow / TumblingWindow)" );
        }
        if ( !policy_.span && !policy_.count && !policy_.weight ) {
            throw std::invalid_argument( "RuleWindow: at least one of span / count / weight is required" );
        }
    }

    /// 添加数据：按时间有序插入（缓存 key 与权重），随后按全部维度淘汰
    void Add( T item ) override {
        const TimeT  key    = policy_.KeyOf( item );
        const double weight = policy_.WeightOf( item );

        if ( !this->on_add_.empty() ) {
            this->NotifyAdd( ItemList{ item } );  // 仅在有监听者时拷贝一次
        }

        const auto pos = std::upper_bound( entries_.begin(), entries_.end(), key,
                                           []( const TimeT &tp, const Entry &entry ) { return tp < entry.key; } );
        entries_.insert( pos, Entry{ std::move( item ), key, weight } );
        total_weight_ += weight;

        (void)Evict( std::nullopt, true );
    }

    /**
     * @brief 推进时间（缺席驱动）：以 now 为最新时刻淘汰过期数据
     *
     * 显式推进不受裁剪冷却限制；返回本次淘汰的条数。
     */
    std::size_t Poll( TimeT now ) {
        if ( !policy_.span || entries_.empty() ) {
            return 0;  // 条数/权重维度在 Add 时已维持，无需时间推进
        }
        const TimeT latest = ( entries_.back().key < now ) ? now : entries_.back().key;
        return Evict( latest, false );
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return entries_.size(); }

    /// 清空窗口（作为一批移除通知）
    void Clear() override {
        if ( entries_.empty() ) {
            return;
        }
        ItemList removed;
        removed.reserve( entries_.size() );
        for ( auto &entry : entries_ ) {
            removed.push_back( std::move( entry.item ) );
        }
        entries_.clear();
        total_weight_ = 0.0;
        last_time_evict_.reset();
        this->NotifyRemove( removed );
    }

    /// 当前聚合权重
    [[nodiscard]] double TotalWeight() const noexcept { return total_weight_; }

    /// 窗口内最新数据的时间点（空窗口返回 std::nullopt）
    [[nodiscard]] std::optional<TimeT> LatestTime() const {
        if ( entries_.empty() ) {
            return std::nullopt;
        }
        return entries_.back().key;
    }

    [[nodiscard]] const Policy &GetPolicy() const noexcept { return policy_; }

    /// 有序视图（按时间升序；含缓存的 key / weight）
    [[nodiscard]] const Storage &Entries() const noexcept { return entries_; }

    /// 数据副本（按时间升序）
    [[nodiscard]] ItemList Items() const {
        ItemList result;
        result.reserve( entries_.size() );
        for ( const auto &entry : entries_ ) {
            result.push_back( entry.item );
        }
        return result;
    }

    /**
     * @brief 获取最近 n 条数据
     *
     * @param n 返回条数；n 为 0 返回空列表，超过窗口长度返回全部
     */
    [[nodiscard]] ItemList GetRecent( std::size_t n ) const {
        if ( n == 0 || entries_.empty() ) {
            return {};
        }
        const std::size_t begin = entries_.size() > n ? entries_.size() - n : 0;
        ItemList          result;
        result.reserve( entries_.size() - begin );
        for ( std::size_t i = begin; i < entries_.size(); ++i ) {
            result.push_back( entries_[i].item );
        }
        return result;
    }

    /**
     * @brief 按时间范围查询（闭区间 [start, end]）
     *
     * @param start 起始时间（含），std::nullopt 表示不限制
     * @param end   结束时间（含），std::nullopt 表示不限制
     */
    [[nodiscard]] ItemList Query( std::optional<TimeT> start, std::optional<TimeT> end ) const {
        std::size_t lo = 0;
        std::size_t hi = entries_.size();

        if ( start ) {
            lo = static_cast<std::size_t>(
                std::lower_bound( entries_.begin(), entries_.end(), *start,
                                  []( const Entry &entry, const TimeT &tp ) { return entry.key < tp; } ) -
                entries_.begin() );
        }
        if ( end ) {
            hi = static_cast<std::size_t>(
                std::upper_bound( entries_.begin(), entries_.end(), *end,
                                  []( const TimeT &tp, const Entry &entry ) { return tp < entry.key; } ) -
                entries_.begin() );
        }
        if ( hi <= lo ) {
            return {};
        }

        ItemList result;
        result.reserve( hi - lo );
        for ( std::size_t i = lo; i < hi; ++i ) {
            result.push_back( entries_[i].item );
        }
        return result;
    }

    /// 按时间升序遍历数据（只暴露元素，不暴露内部 Entry）
    class ItemIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T *;
        using reference         = const T &;

        ItemIterator() = default;
        explicit ItemIterator( typename Storage::const_iterator it ) : it_( it ) {}

        reference operator*() const { return it_->item; }
        pointer   operator->() const { return &it_->item; }

        ItemIterator &operator++() {
            ++it_;
            return *this;
        }
        ItemIterator operator++( int ) {
            ItemIterator tmp = *this;
            ++it_;
            return tmp;
        }

        bool operator==( const ItemIterator &other ) const { return it_ == other.it_; }
        bool operator!=( const ItemIterator &other ) const { return it_ != other.it_; }

    private:
        typename Storage::const_iterator it_{};
    };

    [[nodiscard]] ItemIterator begin() const { return ItemIterator( entries_.begin() ); }
    [[nodiscard]] ItemIterator end() const { return ItemIterator( entries_.end() ); }

private:
    [[nodiscard]] bool TimeEvictionAllowed( TimeT base ) const {
        if ( !( Duration( 0 ) < time_cooldown_ ) ) {
            return true;  // 未配置冷却
        }
        if ( !last_time_evict_ ) {
            return true;
        }
        if ( !( *last_time_evict_ < base ) ) {
            return false;  // 数据时间未前进
        }
        return !( base - *last_time_evict_ < time_cooldown_ );
    }

    /**
     * @brief 按全部维度淘汰最旧数据
     *
     * @param latest          指定的最新时刻；std::nullopt 表示取窗口内最新数据的时间点
     * @param respect_cooldown 是否遵守时间裁剪冷却（Add=true，显式 Poll=false）
     * @return 本次淘汰的条数
     */
    std::size_t Evict( std::optional<TimeT> latest, bool respect_cooldown ) {
        std::size_t drop = 0;

        if ( policy_.span && !entries_.empty() ) {
            const TimeT base = latest.value_or( entries_.back().key );
            if ( !respect_cooldown || TimeEvictionAllowed( base ) ) {
                const TimeT cutoff = CutoffTime( base, *policy_.span );
                while ( drop < entries_.size() && entries_[drop].key < cutoff ) {
                    ++drop;
                }
                last_time_evict_ = base;
            }
        }
        if ( policy_.count ) {
            while ( entries_.size() - drop > *policy_.count ) {
                ++drop;
            }
        }
        if ( policy_.weight ) {
            double remaining = total_weight_;
            for ( std::size_t i = 0; i < drop; ++i ) {
                remaining -= entries_[i].weight;
            }
            while ( drop < entries_.size() && remaining > *policy_.weight ) {
                remaining -= entries_[drop].weight;
                ++drop;
            }
        }

        if ( drop == 0 ) {
            return 0;
        }

        ItemList removed;
        removed.reserve( drop );
        for ( std::size_t i = 0; i < drop; ++i ) {
            total_weight_ -= entries_[i].weight;
            removed.push_back( std::move( entries_[i].item ) );
        }
        entries_.erase( entries_.begin(), entries_.begin() + static_cast<std::ptrdiff_t>( drop ) );
        if ( total_weight_ < 0.0 ) {
            total_weight_ = 0.0;  // 浮点误差保护
        }
        this->NotifyRemove( removed );
        return drop;
    }

private:
    Policy                   policy_;
    Storage                  entries_;
    double                   total_weight_{ 0.0 };
    Duration                 time_cooldown_{};
    std::optional<TimeT>     last_time_evict_;
};

// ==================== 单维度便捷特例（同一引擎的不同配置） ====================

/**
 * @brief 按计数限制的滑动窗口（RuleWindow 的条数特例）
 *
 * 固定容量窗口，超出容量时自动淘汰最早进入的数据（FIFO）。
 * max_len 为 0 时数据“即进即出”。
 */
template <typename T>
class CountWindow final : public RuleWindow<T, DefaultTimePoint> {
public:
    explicit CountWindow( std::size_t max_len ) :
        RuleWindow<T, DefaultTimePoint>( MakePolicy( max_len ) ), max_len_( max_len ) {}

    /// 窗口最大容量
    [[nodiscard]] std::size_t Capacity() const noexcept { return max_len_; }

private:
    static typename RuleWindow<T, DefaultTimePoint>::Policy MakePolicy( std::size_t max_len ) {
        typename RuleWindow<T, DefaultTimePoint>::Policy policy;
        policy.count = max_len;
        return policy;
    }

    std::size_t max_len_;
};

/**
 * @brief 按时间范围滑动的窗口（RuleWindow 的时间特例）
 *
 * 以 key_fn 提取的时间点保持升序，窗口范围为 [latest - window_size, latest]。
 * 新增数据按时间顺序插入（末尾追加为快路径，乱序数据二分定位）；
 * Add 时按 prune_cooldown 节流裁剪，Poll(now) 可随时主动裁剪。
 *
 * 时间类型 TimeT 完全参数化，只需满足：
 *   - 可比较（<）；
 *   - TimeT - TimeT 得到间隔类型 Duration；
 *   - TimeT - Duration 得到新的 TimeT。
 * 因此 time_t / int64_t 时间戳 / std::chrono::time_point / 游戏 tick 均可直接使用。
 */
template <typename T, typename TimeT>
class TimeWindow final : public RuleWindow<T, TimeT> {
public:
    using Base     = RuleWindow<T, TimeT>;
    using Duration = typename Base::Duration;
    using KeyFn    = typename Base::KeyFn;

    /**
     * @brief 构造时间窗口
     *
     * @param window_size    窗口时间跨度
     * @param key_fn         从数据项提取时间点的函数
     * @param prune_cooldown 两次裁剪之间的最小数据时间间隔（Duration{} 表示每次添加都检查裁剪）
     */
    TimeWindow( Duration window_size, KeyFn key_fn, Duration prune_cooldown = Duration{} ) :
        Base( MakePolicy( window_size, std::move( key_fn ) ), prune_cooldown ) {}

private:
    static typename Base::Policy MakePolicy( Duration span, KeyFn key_fn ) {
        typename Base::Policy policy;
        policy.span   = span;
        policy.key_fn = std::move( key_fn );
        return policy;
    }
};

/**
 * @brief 按聚合权重限制的滑动窗口（RuleWindow 的权重特例）
 *
 * 添加数据后若聚合权重超过 max_capacity，则从最早的数据开始批量淘汰，
 * 直到权重回落到上限以内。单条数据本身超过上限时“即进即出”。
 */
template <typename T>
class ThresholdWindow final : public RuleWindow<T, DefaultTimePoint> {
public:
    using WeightFn = std::function<double( const T & )>;

    ThresholdWindow( double max_capacity, WeightFn weight_fn ) :
        RuleWindow<T, DefaultTimePoint>( MakePolicy( max_capacity, std::move( weight_fn ) ) ),
        max_capacity_( max_capacity ) {}

    /// 当前聚合权重
    [[nodiscard]] double CurrentWeight() const noexcept { return this->TotalWeight(); }

    /// 聚合权重上限
    [[nodiscard]] double MaxCapacity() const noexcept { return max_capacity_; }

private:
    static typename RuleWindow<T, DefaultTimePoint>::Policy MakePolicy( double max_capacity, WeightFn weight_fn ) {
        typename RuleWindow<T, DefaultTimePoint>::Policy policy;
        policy.weight    = max_capacity;
        policy.weight_fn = std::move( weight_fn );
        return policy;
    }

    double max_capacity_;
};

/**
 * @brief 按时间间隙切分的会话窗口
 *
 * 由数据流的活跃度决定窗口边界：当新数据的时间点与会话内最大时间点
 * 的间隔超过 session_timeout 时，视为会话断裂，旧会话数据作为一批归档并移除。
 *
 * 说明：
 *   - 驱动方式：Add（数据驱动，新数据与上一条的间隔超时即切分）、
 *     Poll(now)（缺席驱动，空闲超时后主动结束会话，无需等下一条数据）、
 *     Close()（显式结束当前会话）；
 *   - 会话结束同时通过 OnRemove（数据离开窗口）与 OnClose（语义事件）通知；
 *   - 假设数据近似按时间有序到达；会话内的乱序数据按到达顺序保存，
 *     且不会影响会话边界的判定（以会话内最大时间为基准）。
 *
 * @tparam T     数据元素类型
 * @tparam TimeT 时间点类型
 */
template <typename T, typename TimeT>
class SessionWindow final : public SlidingWindow<T> {
public:
    using Duration = decltype( std::declval<TimeT>() - std::declval<TimeT>() );
    using KeyFn    = std::function<TimeT( const T & )>;

    /**
     * @brief 构造会话窗口
     *
     * @param session_timeout 会话超时时间（相邻数据时间间隔超过该值则会话断裂）
     * @param key_fn          从数据项提取时间点的函数
     */
    SessionWindow( Duration session_timeout, KeyFn key_fn ) :
        session_timeout_( session_timeout ), key_fn_( std::move( key_fn ) ) {}

    void Add( T item ) override {
        const TimeT t = key_fn_( item );

        // 会话断裂检测：以会话内最大时间点为基准（间隔比较前先确保时间点递增，避免无符号回绕）
        if ( session_last_.has_value() && *session_last_ < t && t - *session_last_ > session_timeout_ ) {
            (void)CloseSession();  // 归档旧会话（含 on_remove / on_close 通知）
        }

        if ( !session_last_.has_value() || *session_last_ < t ) {
            session_last_ = t;
        }

        typename SlidingWindow<T>::ItemList added;
        if ( !this->on_add_.empty() ) {
            added.push_back( item );  // 仅在有监听者时拷贝一次
        }
        records_.push_back( std::move( item ) );
        this->NotifyAdd( added );
    }

    /**
     * @brief 推进时间：空闲超过 session_timeout 时主动结束会话（缺席驱动）
     *
     * 与 Add 中的数据驱动切分等价，但不需要等待下一条数据。
     *
     * @return 本次结束的会话数（0 或 1）
     */
    std::size_t Poll( TimeT now ) {
        if ( session_last_ && SessionExpired( *session_last_, now, session_timeout_ ) ) {
            (void)CloseSession();
            return 1;
        }
        return 0;
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    /**
     * @brief 清空窗口（放弃当前会话，数据作为一批移除并通知，但不触发 OnClose）
     */
    void Clear() override { (void)CloseSession( false ); }

    /**
     * @brief 主动结束当前会话：将全部数据作为一批归档并移除
     *
     * 适用于会话的最终收尾（如进程退出前），或需要手动切分会话的场景。
     *
     * @return 被归档的数据列表；空会话返回空列表且不触发回调
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList Close() { return CloseSession(); }

    /**
     * @brief 注册会话完成监听（语义事件，与 OnRemove 并行通知）
     *
     * OnRemove 表示“数据离开窗口”，OnClose 表示“一个会话已完成”；
     * 会话自然结束（Add 切分 / Poll 超时 / Close）时两者都会触发；
     * Clear() 属于放弃会话，只触发 OnRemove。
     */
    using CloseCallback = std::function<void( const typename SlidingWindow<T>::ItemList & )>;
    void OnClose( CloseCallback callback ) { on_close_.push_back( std::move( callback ) ); }

    [[nodiscard]] typename std::deque<T>::const_iterator begin() const noexcept { return records_.begin(); }
    [[nodiscard]] typename std::deque<T>::const_iterator end() const noexcept { return records_.end(); }

private:
    /**
     * @brief 结束当前会话：取出全部数据、重置会话状态并触发移除通知
     *
     * @param completed true 表示会话正常完成（额外触发 OnClose）；false 表示放弃（仅 OnRemove）
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList CloseSession( bool completed = true ) {
        typename SlidingWindow<T>::ItemList archived( std::make_move_iterator( records_.begin() ),
                                                      std::make_move_iterator( records_.end() ) );
        records_.clear();
        session_last_.reset();
        this->NotifyRemove( archived );
        if ( completed && !archived.empty() ) {
            for ( auto &callback : on_close_ ) {
                if ( callback ) {
                    callback( archived );
                }
            }
        }
        return archived;
    }

private:
    Duration             session_timeout_;
    KeyFn                key_fn_;
    std::deque<T>        records_;
    std::optional<TimeT> session_last_;
    std::vector<CloseCallback> on_close_;
};

/**
 * @brief 按连续业务条件过滤的窗口
 *
 * 只保留连续满足 predicate 的数据；一旦新数据打破条件，
 * 窗口内历史数据作为一批移除并通知，且打破条件的数据本身不进入窗口
 * （通过 OnReject 通知该数据）。
 * 适用于“连续 N 次失败”“连续上升序列”等场景。
 *
 * @tparam T 数据元素类型
 */
template <typename T>
class PredicateWindow final : public SlidingWindow<T> {
public:
    using Predicate = std::function<bool( const T & )>;

    /**
     * @brief 构造条件窗口
     *
     * @param predicate 保留条件（返回 true 的数据进入窗口）
     */
    explicit PredicateWindow( Predicate predicate ) : predicate_( std::move( predicate ) ) {}

    void Add( T item ) override {
        if ( predicate_( item ) ) {
            typename SlidingWindow<T>::ItemList added;
            if ( !this->on_add_.empty() ) {
                added.push_back( item );  // 仅在有监听者时拷贝一次
            }
            records_.push_back( std::move( item ) );
            this->NotifyAdd( added );
        }
        else {
            // 条件被打破：清空当前连续序列（打破条件的数据本身不入窗，经 OnReject 通知）
            typename SlidingWindow<T>::ItemList removed( std::make_move_iterator( records_.begin() ),
                                                         std::make_move_iterator( records_.end() ) );
            records_.clear();
            this->NotifyRemove( removed );
            for ( auto &callback : on_reject_ ) {
                if ( callback ) {
                    callback( item );
                }
            }
        }
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    /**
     * @brief 注册"条件被打破"监听：参数为打破条件的原始数据
     *
     * 该数据不入窗（既不触发 OnAdd 也不在 OnRemove 批次中），此前无通道可获取。
     */
    using RejectCallback = std::function<void( const T & )>;
    void OnReject( RejectCallback callback ) { on_reject_.push_back( std::move( callback ) ); }

    void Clear() override {
        typename SlidingWindow<T>::ItemList removed( std::make_move_iterator( records_.begin() ),
                                                     std::make_move_iterator( records_.end() ) );
        records_.clear();
        this->NotifyRemove( removed );
    }

    /**
     * @brief 获取最近 n 条数据（按进入顺序）
     *
     * @param n 返回条数；n 为 0 返回空列表，超过窗口长度返回全部
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList GetRecent( std::size_t n ) const {
        if ( n == 0 || records_.empty() ) {
            return {};
        }
        const std::size_t begin = records_.size() > n ? records_.size() - n : 0;
        return { records_.begin() + static_cast<std::ptrdiff_t>( begin ), records_.end() };
    }

    [[nodiscard]] typename std::deque<T>::const_iterator begin() const noexcept { return records_.begin(); }
    [[nodiscard]] typename std::deque<T>::const_iterator end() const noexcept { return records_.end(); }

private:
    Predicate     predicate_;
    std::deque<T> records_;
    std::vector<RejectCallback> on_reject_;
};

}  // namespace utils
