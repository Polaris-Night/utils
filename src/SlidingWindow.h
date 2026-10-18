#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <functional>
#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

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
        for ( const auto &callback : on_add_ ) {
            if ( callback ) {
                callback( items );
            }
        }
    }

    void NotifyRemove( const ItemList &items ) const {
        if ( items.empty() ) {
            return;
        }
        for ( const auto &callback : on_remove_ ) {
            if ( callback ) {
                callback( items );
            }
        }
    }

protected:
    std::vector<AddCallback>    on_add_;
    std::vector<RemoveCallback> on_remove_;
};

/**
 * @brief 按计数限制的滑动窗口
 *
 * 固定容量窗口，超出容量时自动淘汰最早进入的数据（FIFO）。
 * 适用于“只关心最近 N 条”的场景，与时间无关。
 *
 * @tparam T 数据元素类型
 */
template <typename T>
class CountWindow final : public SlidingWindow<T> {
public:
    /**
     * @brief 构造计数窗口
     *
     * @param max_len 窗口最大容量；为 0 时数据“即进即出”（加入后立即被淘汰）
     */
    explicit CountWindow( std::size_t max_len ) : max_len_( max_len ) {}

    void Add( T item ) override {
        typename SlidingWindow<T>::ItemList added;
        if ( !this->on_add_.empty() ) {
            added.push_back( item );  // 仅在有监听者时拷贝一次
        }
        records_.push_back( std::move( item ) );

        typename SlidingWindow<T>::ItemList removed;
        if ( records_.size() > max_len_ ) {
            removed.push_back( std::move( records_.front() ) );
            records_.pop_front();
        }

        this->NotifyAdd( added );
        this->NotifyRemove( removed );
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    void Clear() override {
        typename SlidingWindow<T>::ItemList removed( std::make_move_iterator( records_.begin() ),
                                                     std::make_move_iterator( records_.end() ) );
        records_.clear();
        this->NotifyRemove( removed );
    }

    /**
     * @brief 窗口最大容量
     */
    [[nodiscard]] std::size_t Capacity() const noexcept { return max_len_; }

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
    std::size_t   max_len_;
    std::deque<T> records_;
};

/**
 * @brief 按时间范围滑动的窗口
 *
 * 以 key_fn 提取的时间点将数据保持升序排列，窗口范围为 [latest - window_size, latest]。
 * 新增数据时按时间顺序插入（末尾追加为快路径，乱序数据二分定位），
 * 并在裁剪冷却期后批量清理超期数据。
 *
 * 时间类型 TimeT 完全参数化，只需满足：
 *   - 可比较（<）；
 *   - TimeT - TimeT 得到间隔类型 Duration；
 *   - TimeT - Duration 得到新的 TimeT。
 * 因此 time_t / int64_t 时间戳 / std::chrono::time_point / 游戏 tick 均可直接使用。
 *
 * @tparam T     数据元素类型
 * @tparam TimeT 时间点类型
 */
template <typename T, typename TimeT>
class TimeWindow final : public SlidingWindow<T> {
public:
    using Duration = decltype( std::declval<TimeT>() - std::declval<TimeT>() );
    using KeyFn    = std::function<TimeT( const T & )>;

    /**
     * @brief 构造时间窗口
     *
     * @param window_size    窗口时间跨度
     * @param key_fn         从数据项提取时间点的函数
     * @param prune_cooldown 两次裁剪之间的最小数据时间间隔（Duration{} 表示每次添加都检查裁剪）
     */
    TimeWindow( Duration window_size, KeyFn key_fn, Duration prune_cooldown = Duration{} ) :
        window_size_( window_size ), key_fn_( std::move( key_fn ) ), prune_cooldown_( prune_cooldown ) {}

    void Add( T item ) override {
        const TimeT t = key_fn_( item );

        typename SlidingWindow<T>::ItemList added;
        if ( !this->on_add_.empty() ) {
            added.push_back( item );  // 仅在有监听者时拷贝一次；保证回调载荷是“本次加入的数据”
        }

        // 快路径：数据通常按时间递增到达；慢路径：乱序/补录数据二分定位
        if ( records_.empty() || !( t < key_fn_( records_.back() ) ) ) {
            records_.push_back( std::move( item ) );
        }
        else {
            const auto pos = LowerBound( t );
            records_.insert( pos, std::move( item ) );
        }

        this->NotifyAdd( added );
        PruneCheck();
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    void Clear() override {
        typename SlidingWindow<T>::ItemList removed( std::make_move_iterator( records_.begin() ),
                                                     std::make_move_iterator( records_.end() ) );
        records_.clear();
        last_prune_.reset();
        this->NotifyRemove( removed );
    }

    /**
     * @brief 窗口内最新数据的时间点，空窗口返回 std::nullopt
     */
    [[nodiscard]] std::optional<TimeT> LatestTime() const {
        if ( records_.empty() ) {
            return std::nullopt;
        }
        return key_fn_( records_.back() );
    }

    /**
     * @brief 按时间范围查询窗口内的数据（闭区间 [start, end]）
     *
     * @param start 起始时间（含），std::nullopt 表示不限制
     * @param end 结束时间（含），std::nullopt 表示不限制
     * @return 时间范围内的数据列表；start > end 时返回空列表
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList Query( std::optional<TimeT> start,
                                                             std::optional<TimeT> end ) const {
        auto lo = records_.begin();
        auto hi = records_.end();

        if ( start ) {
            lo = LowerBound( *start );
        }
        if ( end ) {
            hi = UpperBound( *end );
        }
        if ( hi < lo ) {
            return {};  // 参数区间无效（start > end）
        }
        return { lo, hi };
    }

    /**
     * @brief 获取最近 n 条数据（按时间顺序）
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

    [[nodiscard]] typename std::vector<T>::const_iterator begin() const noexcept { return records_.begin(); }
    [[nodiscard]] typename std::vector<T>::const_iterator end() const noexcept { return records_.end(); }

private:
    using RecordIter = typename std::vector<T>::iterator;
    using ConstIter  = typename std::vector<T>::const_iterator;

    [[nodiscard]] RecordIter LowerBound( TimeT target ) {
        return std::lower_bound( records_.begin(), records_.end(), target,
                                 [this]( const T &item, const TimeT &tp ) { return key_fn_( item ) < tp; } );
    }

    [[nodiscard]] ConstIter LowerBound( TimeT target ) const {
        return std::lower_bound( records_.begin(), records_.end(), target,
                                 [this]( const T &item, const TimeT &tp ) { return key_fn_( item ) < tp; } );
    }

    [[nodiscard]] ConstIter UpperBound( TimeT target ) const {
        return std::upper_bound( records_.begin(), records_.end(), target,
                                 [this]( const TimeT &tp, const T &item ) { return tp < key_fn_( item ); } );
    }

    /**
     * @brief 计算裁剪点 latest - window_size
     *
     * 对无符号时间类型做饱和处理：当窗口跨度大于当前时间值时取 0，
     * 避免下溢回绕导致误裁剪全部数据（如早期 tick 值小于窗口跨度）。
     */
    [[nodiscard]] static TimeT CutoffTime( TimeT latest, Duration window ) {
        if constexpr ( std::is_unsigned_v<TimeT> ) {
            return window < latest ? static_cast<TimeT>( latest - window ) : TimeT{ 0 };
        }
        else {
            return latest - window;
        }
    }

    void PruneCheck() {
        if ( records_.empty() ) {
            return;
        }

        const TimeT latest = key_fn_( records_.back() );

        if ( last_prune_.has_value() ) {
            // 数据时间未前进（乱序/补录）时跳过裁剪：既避免无符号回绕，
            // 也保证冷却期语义基于“数据时间”单调推进
            if ( !( *last_prune_ < latest ) ) {
                return;
            }
            if ( latest - *last_prune_ < prune_cooldown_ ) {
                return;
            }
        }

        const auto cutoff  = CutoffTime( latest, window_size_ );
        const auto cut_pos = LowerBound( cutoff );

        typename SlidingWindow<T>::ItemList removed;
        if ( cut_pos != records_.begin() ) {
            removed.assign( std::make_move_iterator( records_.begin() ), std::make_move_iterator( cut_pos ) );
            records_.erase( records_.begin(), cut_pos );
        }
        last_prune_ = latest;

        this->NotifyRemove( removed );
    }

private:
    Duration             window_size_;
    KeyFn                key_fn_;
    Duration             prune_cooldown_;
    std::vector<T>       records_;
    std::optional<TimeT> last_prune_;
};

/**
 * @brief 按聚合权重限制的滑动窗口
 *
 * 窗口容量由窗口内数据的聚合权重（如总字节数、总大小）决定：
 * 添加数据后若聚合权重超过 max_capacity，则从最早的数据开始批量淘汰，
 * 直到权重回落到上限以内。
 *
 * 说明：单条数据的权重本身超过上限时，该数据会“即进即出”（窗口可能为空），
 * 以保证窗口的不变式：聚合权重始终不超过上限（除非窗口为空）。
 *
 * @tparam T 数据元素类型
 */
template <typename T>
class ThresholdWindow final : public SlidingWindow<T> {
public:
    using WeightFn = std::function<double( const T & )>;

    /**
     * @brief 构造阈值窗口
     *
     * @param max_capacity 聚合权重上限（需 >= 0）
     * @param weight_fn    从数据项提取权重的函数（应为纯函数，权重在入窗时计算并缓存）
     */
    ThresholdWindow( double max_capacity, WeightFn weight_fn ) :
        max_capacity_( max_capacity ), weight_fn_( std::move( weight_fn ) ) {}

    void Add( T item ) override {
        const double weight = weight_fn_( item );

        typename SlidingWindow<T>::ItemList added;
        if ( !this->on_add_.empty() ) {
            added.push_back( item );  // 仅在有监听者时拷贝一次
        }
        records_.push_back( Entry{ std::move( item ), weight } );
        current_weight_ += weight;

        // 批量淘汰超重部分（权重已缓存，无需重新计算）
        typename SlidingWindow<T>::ItemList removed;
        while ( current_weight_ > max_capacity_ && !records_.empty() ) {
            current_weight_ -= records_.front().weight;
            removed.push_back( std::move( records_.front().item ) );
            records_.pop_front();
        }
        if ( current_weight_ < 0.0 ) {
            current_weight_ = 0.0;  // 浮点误差保护
        }

        this->NotifyAdd( added );
        this->NotifyRemove( removed );
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    void Clear() override {
        typename SlidingWindow<T>::ItemList removed;
        removed.reserve( records_.size() );
        for ( auto &entry : records_ ) {
            removed.push_back( std::move( entry.item ) );
        }
        records_.clear();
        current_weight_ = 0.0;
        this->NotifyRemove( removed );
    }

    /**
     * @brief 当前聚合权重
     */
    [[nodiscard]] double CurrentWeight() const noexcept { return current_weight_; }

    /**
     * @brief 聚合权重上限
     */
    [[nodiscard]] double MaxCapacity() const noexcept { return max_capacity_; }

    /**
     * @brief 获取最近 n 条数据（按进入顺序）
     *
     * @param n 返回条数；n 为 0 返回空列表，超过窗口长度返回全部
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList GetRecent( std::size_t n ) const {
        typename SlidingWindow<T>::ItemList result;
        if ( n == 0 || records_.empty() ) {
            return result;
        }
        const std::size_t begin = records_.size() > n ? records_.size() - n : 0;
        result.reserve( records_.size() - begin );
        for ( std::size_t i = begin; i < records_.size(); ++i ) {
            result.push_back( records_[i].item );
        }
        return result;
    }

private:
    struct Entry {
        T      item;
        double weight{ 0.0 };
    };

    double            max_capacity_;
    double            current_weight_{ 0.0 };
    WeightFn          weight_fn_;
    std::deque<Entry> records_;
};

/**
 * @brief 按时间间隙切分的会话窗口
 *
 * 由数据流的活跃度决定窗口边界：当新数据的时间点与会话内最大时间点
 * 的间隔超过 session_timeout 时，视为会话断裂，旧会话数据作为一批归档并移除。
 *
 * 说明：
 *   - 会话超时是被动检测的（纯内存组件无后台定时器），即在下一条数据到达时才能发现；
 *     当前会话的最终归档可通过 Flush() 主动触发；
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
            (void)CloseSession();  // 归档旧会话（含 on_remove 通知）
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

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

    /**
     * @brief 清空窗口（等价于放弃当前会话，数据作为一批移除并通知）
     */
    void Clear() override { (void)CloseSession(); }

    /**
     * @brief 主动结束当前会话：将全部数据作为一批归档并移除（返回被归档的数据）
     *
     * 适用于会话的最终收尾（如进程退出前），或需要手动切分会话的场景。
     *
     * @return 被归档的数据列表；空会话返回空列表且不触发回调
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList Flush() { return CloseSession(); }

    [[nodiscard]] typename std::deque<T>::const_iterator begin() const noexcept { return records_.begin(); }
    [[nodiscard]] typename std::deque<T>::const_iterator end() const noexcept { return records_.end(); }

private:
    /**
     * @brief 结束当前会话：取出全部数据、重置会话状态并触发移除通知
     */
    [[nodiscard]] typename SlidingWindow<T>::ItemList CloseSession() {
        typename SlidingWindow<T>::ItemList archived( std::make_move_iterator( records_.begin() ),
                                                      std::make_move_iterator( records_.end() ) );
        records_.clear();
        session_last_.reset();
        this->NotifyRemove( archived );
        return archived;
    }

private:
    Duration             session_timeout_;
    KeyFn                key_fn_;
    std::deque<T>        records_;
    std::optional<TimeT> session_last_;
};

/**
 * @brief 按连续业务条件过滤的窗口
 *
 * 只保留连续满足 predicate 的数据；一旦新数据打破条件，
 * 窗口内历史数据作为一批移除并通知，且打破条件的数据本身不进入窗口。
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
            // 条件被打破：清空当前连续序列（打破条件的数据本身不入窗）
            typename SlidingWindow<T>::ItemList removed( std::make_move_iterator( records_.begin() ),
                                                         std::make_move_iterator( records_.end() ) );
            records_.clear();
            this->NotifyRemove( removed );
        }
    }

    [[nodiscard]] std::size_t Size() const noexcept override { return records_.size(); }

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
};

}  // namespace utils
