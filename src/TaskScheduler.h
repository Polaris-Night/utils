#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
#include "DateTime.h"
#include "Macros.h"
#include "TaskExecutor.h"

namespace utils {

/**
 * @brief 任务 id
 */
using TaskId = std::uint64_t;

/**
 * @brief 迟到处理策略（计划时刻已过、发现时已迟到的场合）
 */
enum class LatePolicy
{
    CatchUp,  ///< 补跑一次（合并所有错过点）后重新对齐
    Skip,     ///< 跳过本次，等下一个计划时刻
};

/**
 * @brief 任务策略
 */
struct TaskPolicy {
    LatePolicy   late{ LatePolicy::CatchUp };  ///< 迟到处理（默认补跑一次）
    Milliseconds grace{ 5000 };                ///< 迟到容忍窗口：不超过该窗口一律正常执行
    bool         coalesce{ true };             ///< 上次未执行完成时跳过本次（max_instances = 1）
    Nanoseconds  jitter{ 0 };                  ///< 首次触发的相位抖动上界（防惊群；0 = 无抖动）
};

// ==================== 触发器（纯函数：下一次何时触发） ====================

/**
 * @brief 墙钟轴触发器（日历语义）
 */
class WallTrigger {
public:
    virtual ~WallTrigger() = default;

    /// 返回严格晚于 after 的下一次触发时刻；std::nullopt 表示不再触发
    [[nodiscard]] virtual std::optional<DateTime> NextAfter( const DateTime &after ) const = 0;
};

/**
 * @brief 单调轴触发器（物理时长语义）
 */
class SteadyTrigger {
public:
    using SteadyTime = std::chrono::steady_clock::time_point;

    virtual ~SteadyTrigger() = default;

    /// 返回严格晚于 after 的下一次触发时刻；std::nullopt 表示不再触发
    [[nodiscard]] virtual std::optional<SteadyTime> NextAfter( const SteadyTime &after ) const = 0;
};

/**
 * @brief 一次性触发：在指定绝对时刻触发一次
 */
class AtTrigger final : public WallTrigger {
public:
    explicit AtTrigger( DateTime at ) : at_( std::move( at ) ) {}

    [[nodiscard]] std::optional<DateTime> NextAfter( const DateTime &after ) const override {
        return at_ > after ? std::optional<DateTime>( at_ ) : std::nullopt;
    }

private:
    DateTime at_;
};

/**
 * @brief 固定周期触发（固定网格 = Fixed-Rate，绝不漂移）
 *
 * 周期必须 > 0；网格原点为构造时给定的 start（默认为构造时刻的单调时钟）。
 */
class IntervalTrigger final : public SteadyTrigger {
public:
    explicit IntervalTrigger( Nanoseconds period, SteadyTime start = std::chrono::steady_clock::now() );

    [[nodiscard]] std::optional<SteadyTime> NextAfter( const SteadyTime &after ) const override {
        if ( after < start_ ) {
            return start_;
        }
        const auto k = ( after - start_ ) / period_ + 1;  // 首个严格晚于 after 的网格点
        return start_ + period_ * k;
    }

private:
    SteadyTime  start_;
    Nanoseconds period_;
};

/**
 * @brief 日历触发：每日 / 每周 / 每月固定时钟时间（按指定时区）
 *
 * DST 策略（默认，文档约定）：
 *   - 春季跳变导致目标时刻不存在（如 02:30 被跳过）：跳过该次，等下一周期；
 *   - 秋季回拨导致目标时刻出现两次：只在较早一次触发；
 *   - 每月 day 日遇到无该日的月份（如 31 日遇 2 月）：跳过该月。
 */
class CalendarTrigger final : public WallTrigger {
public:
    /// 每日 hour:minute:second 触发
    static CalendarTrigger Daily( int hour, int minute, int second = 0, TimeZone zone = TimeZone::Local() );

    /// 每周 weekday 的 hour:minute:second 触发
    static CalendarTrigger Weekly( Weekday weekday, int hour, int minute, int second = 0,
                                   TimeZone zone = TimeZone::Local() );

    /// 每月 day 日的 hour:minute:second 触发
    static CalendarTrigger Monthly( int day, int hour, int minute, int second = 0,
                                    TimeZone zone = TimeZone::Local() );

    [[nodiscard]] std::optional<DateTime> NextAfter( const DateTime &after ) const override;

private:
    enum class Kind
    {
        Daily,
        Weekly,
        Monthly,
    };

    CalendarTrigger( Kind kind, int day, int hour, int minute, int second, TimeZone zone ) :
        kind_( kind ), day_( day ), hour_( hour ), minute_( minute ), second_( second ), zone_( std::move( zone ) ) {}

    Kind     kind_;
    int      day_;  ///< Weekly: 0-6（0 = 周日）；Monthly: 1-31
    int      hour_;
    int      minute_;
    int      second_;
    TimeZone zone_;
};

/**
 * @brief 自定义墙钟触发器（适配 cron 库或任意日历规则）
 */
class FunctionalWallTrigger final : public WallTrigger {
public:
    using Fn = std::function<std::optional<DateTime>( const DateTime & )>;

    explicit FunctionalWallTrigger( Fn fn ) : fn_( std::move( fn ) ) {}

    [[nodiscard]] std::optional<DateTime> NextAfter( const DateTime &after ) const override {
        return fn_ ? fn_( after ) : std::nullopt;
    }

private:
    Fn fn_;
};

/**
 * @brief 自定义单调触发器
 */
class FunctionalSteadyTrigger final : public SteadyTrigger {
public:
    using Fn = std::function<std::optional<SteadyTime>( const SteadyTime & )>;

    explicit FunctionalSteadyTrigger( Fn fn ) : fn_( std::move( fn ) ) {}

    [[nodiscard]] std::optional<SteadyTime> NextAfter( const SteadyTime &after ) const override {
        return fn_ ? fn_( after ) : std::nullopt;
    }

private:
    Fn fn_;
};

/**
 * @brief 时间驱动的任务调度器
 *
 * 定位：与数据驱动的 SlidingWindow、物理计时的 Timer 互补的"主动唤醒"组件。
 *
 * 时间语义（双时间轴）：
 *   - steady 轴（IntervalTrigger）：物理时长，免疫系统改时；
 *   - wall 轴（AtTrigger / CalendarTrigger）：日历真相，按 DateTime + 时区计算。
 *
 * 确定性推进：核心是 AdvanceTo(wall_now, steady_now) —— 内部不读时钟、不睡眠。
 * 线程适配（Start/Stop）与外部驱动（事件循环/游戏帧循环/测试）都只是它的驱动方式；
 * 构造时以真实时钟作为初始时间轴起点，确定性场景请先调用一次 AdvanceTo 设定起点。
 *
 * 执行语义：
 *   - 调度与执行分离：动作经 TaskExecutor 提交（默认 InlineExecutor 同步执行）；
 *   - 回调签名 void(const DateTime &scheduled)：scheduled 为计划触发时刻（steady 任务为
 *     锚点换算的逻辑墙钟，稳定不变），可直接用于业务幂等键；
 *   - 回调异常被吞掉（不允许拖垮调度器）；
 *   - coalesce = 上次未完成则跳过本次（max_instances = 1）。
 *
 * 线程安全：所有公开接口可并发调用；回调在无锁上下文执行。
 */
class TaskScheduler {
public:
    using Action     = std::function<void( const DateTime &scheduled )>;
    using SteadyTime = SteadyTrigger::SteadyTime;

    /**
     * @brief 任务统计
     */
    struct TaskStats {
        std::uint64_t fire_count{ 0 };    ///< 已提交执行次数
        std::uint64_t late_count{ 0 };    ///< 迟到触达次数（超过计划时刻）
        std::uint64_t skip_count{ 0 };    ///< 跳过次数（迟到超窗口、执行器占用）
        std::uint64_t reject_count{ 0 };  ///< 执行器拒绝次数（背压）
    };

    TaskScheduler();
    explicit TaskScheduler( TaskExecutor &executor );
    DISABLE_COPY_MOVE( TaskScheduler )

    /// 析构：若线程适配器仍在运行则等待其停止（长时间不返回的同步任务会阻塞析构，生产建议注入异步执行器）
    ~TaskScheduler();

    /**
     * @brief 注册墙钟任务
     *
     * @return std::optional<TaskId> 一次性任务已过期时返回 std::nullopt
     */
    std::optional<TaskId> Add( std::shared_ptr<WallTrigger> trigger, Action action, TaskPolicy policy = {} );

    /**
     * @brief 注册单调任务
     *
     * @return std::optional<TaskId> 触发器无后续触发时返回 std::nullopt
     */
    std::optional<TaskId> Add( std::shared_ptr<SteadyTrigger> trigger, Action action, TaskPolicy policy = {} );

    /**
     * @brief 取消任务（阻止后续触发；在途执行不中断）
     *
     * 任务记录保留（统计仍可查询）。
     *
     * @return true 成功取消了"有计划待触发"的任务；false 任务不存在或已无计划
     */
    bool Cancel( TaskId id );

    /**
     * @brief 下次触发时刻（墙钟；steady 任务为锚点换算的逻辑值）；无计划返回 std::nullopt
     */
    [[nodiscard]] std::optional<DateTime> NextFireTime( TaskId id ) const;

    /**
     * @brief 当前有计划待触发的任务数
     */
    [[nodiscard]] std::size_t TaskCount() const;

    /**
     * @brief 任务统计；任务不存在返回 std::nullopt
     */
    [[nodiscard]] std::optional<TaskStats> GetTaskStats( TaskId id ) const;

    /**
     * @brief 确定性推进：处理两个时间轴到给定时刻为止的到期触发
     *
     * @param wall_now 墙钟当前时刻
     * @param steady_now 单调时钟当前时刻
     * @return std::size_t 本次提交执行的动作数量
     */
    std::size_t AdvanceTo( const Timestamp &wall_now, const SteadyTime &steady_now );

    // ---- 线程适配器（可选；核心 AdvanceTo 已可独立工作）----

    /**
     * @brief 启动线程适配器（幂等）
     *
     * 单 worker 只负责派发：按两条时间轴中最近的 deadline 等待，
     * add/cancel/stop 会唤醒并重算等待；到点调用 AdvanceTo 推进核心。
     */
    void Start();

    /**
     * @brief 停止线程适配器并回收工作线程
     *
     * @param timeout 等待工作线程退出的超时
     * @return true 已完全停止；false 超时（工作线程仍在运行——通常是在途的同步任务未返回，
     *         此时必须保活对象并稍后重试 Stop）
     */
    bool Stop( Milliseconds timeout = std::chrono::seconds( 5 ) );

    /**
     * @brief 线程适配器是否在运行
     */
    [[nodiscard]] bool IsRunning() const;

    /**
     * @brief 线程适配器的墙钟来源
     *
     * @param enabled false = 系统时间（默认）；true = 校准时间（DateTime::NowSynced，
     *                设置过时间种子时用于抗客户端改时场景）
     */
    void SetSyncedWallClock( bool enabled );

private:
    struct TaskState {
        std::atomic<std::int64_t>  in_flight{ 0 };
        std::atomic<std::uint64_t> fire_count{ 0 };
        std::atomic<std::uint64_t> late_count{ 0 };
        std::atomic<std::uint64_t> skip_count{ 0 };
        std::atomic<std::uint64_t> reject_count{ 0 };
    };

    struct Task {
        TaskId     id{ 0 };
        Action     action;
        TaskPolicy policy;
        std::shared_ptr<TaskState> state;

        std::shared_ptr<WallTrigger>   wall_trigger;
        std::shared_ptr<SteadyTrigger> steady_trigger;

        bool      scheduled{ false };
        bool      is_steady{ false };
        Timestamp wall_next{};     ///< wall 任务下次触发
        SteadyTime steady_next{};  ///< steady 任务下次触发
        DateTime  wall_label{};    ///< 下次触发对应的回调标签

        DateTime   wall_anchor{};    ///< steady 任务的逻辑墙钟锚点
        SteadyTime steady_anchor{};  ///< steady 任务的单调时钟锚点
    };

    struct WallEntry {
        Timestamp due{};
        TaskId    id{ 0 };
        bool      operator>( const WallEntry &other ) const { return due > other.due; }
    };

    struct SteadyEntry {
        SteadyTime due{};
        TaskId     id{ 0 };
        bool       operator>( const SteadyEntry &other ) const { return due > other.due; }
    };

    struct PendingJob {
        std::function<void()>      fn;
        std::shared_ptr<TaskState> state;
    };

    using WallHeap   = std::priority_queue<WallEntry, std::vector<WallEntry>, std::greater<WallEntry>>;
    using SteadyHeap = std::priority_queue<SteadyEntry, std::vector<SteadyEntry>, std::greater<SteadyEntry>>;

    void DispatchWallFire( const std::shared_ptr<Task> &task, const Timestamp &wall_now, std::vector<PendingJob> &jobs );
    void DispatchSteadyFire( const std::shared_ptr<Task> &task, const SteadyTime &steady_now,
                             std::vector<PendingJob> &jobs );
    void SubmitAction( const std::shared_ptr<Task> &task, const DateTime &scheduled, std::vector<PendingJob> &jobs );

    [[nodiscard]] Timestamp WallNow() const;
    void                  RunLoop();

private:
    mutable std::mutex mutex_;
    std::unordered_map<TaskId, std::shared_ptr<Task>> tasks_;
    WallHeap   wall_heap_;
    SteadyHeap steady_heap_;
    TaskId     next_id_{ 1 };
    std::size_t pending_count_{ 0 };

    Timestamp  wall_now_{ DateTime::NowTimestamp() };
    SteadyTime steady_now_{ std::chrono::steady_clock::now() };

    InlineExecutor inline_executor_;
    TaskExecutor  *executor_{ nullptr };

    std::thread             worker_;
    std::condition_variable cv_;       ///< 等待 deadline 或 add/cancel/stop 唤醒
    std::condition_variable exit_cv_;  ///< 工作线程退出通知
    bool                    stopping_{ false };
    bool                    worker_exited_{ false };
    bool                    use_synced_wall_{ false };
};

}  // namespace utils
