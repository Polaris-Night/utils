#include "TaskScheduler.h"
#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>

namespace utils {

namespace {

constexpr int kCalendarLookaheadDays = 800;

std::int64_t RandomJitterNanoseconds( const Nanoseconds &upper ) {
    thread_local std::mt19937_64                generator{ std::random_device{}() };
    std::uniform_int_distribution<std::int64_t> distribution( 0, upper.count() );
    return distribution( generator );
}

void ValidateTimeOfDay( int hour, int minute, int second ) {
    if ( hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59 ) {
        throw std::invalid_argument( "CalendarTrigger: invalid time of day" );
    }
}

/**
 * @brief 按给定字段在时区中寻找真实存在的时刻
 *
 *   - Utc / 固定偏移：直接构造；
 *   - Local：枚举候选偏移并回读校验——
 *       存在且唯一（正常）→ 返回；存在两个（秋季回拨）→ 返回较早者；
 *       不存在（春季跳变）→ 返回 std::nullopt。
 */
std::optional<Timestamp> FindOccurrence( const DateTimeParts &requested, const TimeZone &zone ) {
    if ( !zone.IsLocal() ) {
        const auto dt = DateTime::FromParts( requested, zone );
        return dt ? std::optional<Timestamp>( dt->ToTimestamp() ) : std::nullopt;
    }

    const auto utc_candidate = DateTime::FromParts( requested, TimeZone::Utc() );
    if ( !utc_candidate ) {
        return std::nullopt;
    }
    const Timestamp base = utc_candidate->ToTimestamp();

    // 收集基准附近可能出现的本地偏移（覆盖夏令时切换前后）
    std::vector<int> offsets;
    for ( const std::int64_t hour_delta : { -48, -24, -12, 0, 12, 24, 48 } ) {
        const int offset = TimeZone::Local().OffsetMinutesAt( base + std::chrono::hours( hour_delta ) );
        if ( std::find( offsets.begin(), offsets.end(), offset ) == offsets.end() ) {
            offsets.push_back( offset );
        }
    }

    std::optional<Timestamp> earliest;
    for ( const int offset : offsets ) {
        const Timestamp candidate = base - std::chrono::minutes( offset );
        const DateTime  probe( candidate, TimeZone::Local() );
        const auto      back = probe.ToParts();
        if ( back.year == requested.year && back.month == requested.month && back.day == requested.day &&
             back.hour == requested.hour && back.minute == requested.minute && back.second == requested.second ) {
            if ( !earliest.has_value() || candidate < *earliest ) {
                earliest = candidate;
            }
        }
    }
    return earliest;
}

}  // namespace

// ==================== IntervalTrigger ====================

IntervalTrigger::IntervalTrigger( Nanoseconds period, SteadyTime start ) : start_( start ), period_( period ) {
    if ( period_ <= Nanoseconds::zero() ) {
        throw std::invalid_argument( "IntervalTrigger: period must be > 0" );
    }
}

// ==================== CalendarTrigger ====================

CalendarTrigger CalendarTrigger::Daily( int hour, int minute, int second, TimeZone zone ) {
    ValidateTimeOfDay( hour, minute, second );
    return CalendarTrigger( Kind::Daily, 0, hour, minute, second, std::move( zone ) );
}

CalendarTrigger CalendarTrigger::Weekly( Weekday weekday, int hour, int minute, int second, TimeZone zone ) {
    ValidateTimeOfDay( hour, minute, second );
    if ( weekday < Sunday || weekday > Saturday ) {
        throw std::invalid_argument( "CalendarTrigger: invalid weekday" );
    }
    return CalendarTrigger( Kind::Weekly, static_cast<int>( weekday ), hour, minute, second, std::move( zone ) );
}

CalendarTrigger CalendarTrigger::Monthly( int day, int hour, int minute, int second, TimeZone zone ) {
    ValidateTimeOfDay( hour, minute, second );
    if ( day < 1 || day > 31 ) {
        throw std::invalid_argument( "CalendarTrigger: invalid day of month" );
    }
    return CalendarTrigger( Kind::Monthly, day, hour, minute, second, std::move( zone ) );
}

std::optional<DateTime> CalendarTrigger::NextAfter( const DateTime &after ) const {
    const DateTime local_after = after.WithZone( zone_ );
    DateTime       day_cursor  = local_after.StartOf<Days>();

    // 逐日推进并过滤规则日期；上限覆盖 weekly（7 天）、monthly（31 日连续缺月最多 ~62 天）
    // 与 DST 跳变（最多连续 2 天不存在）等所有边界
    for ( int attempt = 0; attempt < kCalendarLookaheadDays; ++attempt ) {
        bool matches = false;
        if ( kind_ == Kind::Daily ) {
            matches = true;
        }
        else if ( kind_ == Kind::Weekly ) {
            matches = ( static_cast<int>( day_cursor.GetWeekday() ) == day_ );
        }
        else {
            matches = ( day_cursor.Day() == day_ );
        }

        if ( matches ) {
            DateTimeParts parts = day_cursor.ToParts();
            parts.hour        = hour_;
            parts.minute      = minute_;
            parts.second      = second_;
            parts.millisecond = 0;
            parts.microsecond = 0;
            parts.nanosecond  = 0;

            if ( const auto occurrence = FindOccurrence( parts, zone_ ) ) {
                if ( *occurrence > after.ToTimestamp() ) {  // 严格晚于
                    return DateTime( *occurrence, zone_ );
                }
            }
        }
        day_cursor = day_cursor.Add<Days>( 1 );
    }
    return std::nullopt;
}

// ==================== TaskScheduler ====================

TaskScheduler::TaskScheduler() : executor_( &inline_executor_ ) {}

TaskScheduler::TaskScheduler( TaskExecutor &executor ) : executor_( &executor ) {}

TaskScheduler::~TaskScheduler() {
    Stop( Milliseconds::max() );  // 必须回收工作线程（其访问本对象成员）
}

void TaskScheduler::Start() {
    std::lock_guard<std::mutex> lock( mutex_ );
    if ( worker_.joinable() ) {
        return;  // 已在运行
    }
    stopping_      = false;
    worker_exited_ = false;
    worker_        = std::thread( [this]() { RunLoop(); } );
}

bool TaskScheduler::Stop( Milliseconds timeout ) {
    std::unique_lock<std::mutex> lock( mutex_ );
    if ( !worker_.joinable() ) {
        return true;  // 未运行
    }
    stopping_ = true;
    cv_.notify_all();

    const bool exited = exit_cv_.wait_for( lock, timeout, [this]() { return worker_exited_; } );
    if ( !exited ) {
        return false;  // 超时：工作线程仍在运行（通常是在途同步任务未返回）
    }
    worker_.join();  // 工作线程退出路径不再获取互斥量，持锁 join 安全
    return true;
}

bool TaskScheduler::IsRunning() const {
    std::lock_guard<std::mutex> lock( mutex_ );
    return worker_.joinable();
}

void TaskScheduler::SetSyncedWallClock( bool enabled ) {
    std::lock_guard<std::mutex> lock( mutex_ );
    use_synced_wall_ = enabled;
}

Timestamp TaskScheduler::WallNow() const {
    return use_synced_wall_ ? DateTime::NowSyncedTimestamp() : DateTime::NowTimestamp();
}

void TaskScheduler::RunLoop() {
    std::unique_lock<std::mutex> lock( mutex_ );

    // 限制墙钟等待粒度：系统改时（前拨/回拨）在至多该间隔内被感知并重新推导 deadline
    constexpr auto kMaxWallWait = std::chrono::minutes( 1 );

    while ( true ) {
        if ( stopping_ ) {
            break;
        }

        const SteadyTime steady_now    = std::chrono::steady_clock::now();
        bool             has_deadline  = false;
        SteadyTime       deadline{};

        if ( !wall_heap_.empty() ) {
            auto delay = wall_heap_.top().due - WallNow();
            if ( delay < Timestamp::duration::zero() ) {
                delay = Timestamp::duration::zero();
            }
            deadline = steady_now + std::chrono::duration_cast<SteadyTime::duration>( delay );
            deadline = std::min( deadline, steady_now + std::chrono::duration_cast<SteadyTime::duration>( kMaxWallWait ) );
            has_deadline = true;
        }
        if ( !steady_heap_.empty() && ( !has_deadline || steady_heap_.top().due < deadline ) ) {
            deadline     = steady_heap_.top().due;
            has_deadline = true;
        }

        if ( has_deadline && deadline <= steady_now ) {
            // 到点：锁外推进核心（内部自会加锁；绝不持锁执行用户代码）
            lock.unlock();
            AdvanceTo( WallNow(), std::chrono::steady_clock::now() );
            lock.lock();
            continue;
        }

        if ( !has_deadline ) {
            cv_.wait( lock, [this]() { return stopping_ || !wall_heap_.empty() || !steady_heap_.empty(); } );
        }
        else {
            cv_.wait_until( lock, deadline, [this]() { return stopping_; } );
        }
    }

    worker_exited_ = true;
    exit_cv_.notify_all();
}

std::optional<TaskId> TaskScheduler::Add( std::shared_ptr<WallTrigger> trigger, Action action, TaskPolicy policy ) {
    if ( !trigger || !action ) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock( mutex_ );

    const DateTime now( wall_now_, TimeZone::Local() );
    auto           first = trigger->NextAfter( now );
    if ( !first ) {
        return std::nullopt;  // 一次性任务已过期
    }
    if ( policy.jitter > Nanoseconds( 0 ) ) {
        *first = first->Add<Nanoseconds>( RandomJitterNanoseconds( policy.jitter ) );  // 仅首触发相位抖动
    }

    auto task          = std::make_shared<Task>();
    task->id           = next_id_++;
    task->wall_trigger = std::move( trigger );
    task->action       = std::move( action );
    task->policy       = policy;
    task->state        = std::make_shared<TaskState>();
    task->scheduled    = true;
    task->wall_next    = first->ToTimestamp();
    task->wall_label   = *first;

    tasks_.emplace( task->id, task );
    wall_heap_.push( WallEntry{ task->wall_next, task->id } );
    ++pending_count_;
    cv_.notify_all();  // 线程适配器需重算最近 deadline
    return task->id;
}

std::optional<TaskId> TaskScheduler::Add( std::shared_ptr<SteadyTrigger> trigger, Action action, TaskPolicy policy ) {
    if ( !trigger || !action ) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock( mutex_ );

    auto first = trigger->NextAfter( steady_now_ );
    if ( !first ) {
        return std::nullopt;
    }
    if ( policy.jitter > Nanoseconds( 0 ) ) {
        *first += std::chrono::duration_cast<SteadyTime::duration>( Nanoseconds( RandomJitterNanoseconds( policy.jitter ) ) );
    }

    auto task            = std::make_shared<Task>();
    task->id             = next_id_++;
    task->steady_trigger = std::move( trigger );
    task->action         = std::move( action );
    task->policy         = policy;
    task->state          = std::make_shared<TaskState>();
    task->scheduled      = true;
    task->is_steady      = true;
    task->steady_anchor  = steady_now_;
    task->wall_anchor    = DateTime( wall_now_, TimeZone::Local() );
    task->steady_next    = *first;

    const auto delta_ns = std::chrono::duration_cast<Nanoseconds>( *first - task->steady_anchor );
    task->wall_label    = task->wall_anchor.Add<Nanoseconds>( delta_ns.count() );

    tasks_.emplace( task->id, task );
    steady_heap_.push( SteadyEntry{ task->steady_next, task->id } );
    ++pending_count_;
    cv_.notify_all();  // 线程适配器需重算最近 deadline
    return task->id;
}

bool TaskScheduler::Cancel( TaskId id ) {
    std::lock_guard<std::mutex> lock( mutex_ );

    const auto it = tasks_.find( id );
    if ( it == tasks_.end() ) {
        return false;
    }
    auto &task = it->second;
    if ( !task->scheduled ) {
        return false;  // 已完成 / 已取消：无计划可取消（保留记录，统计仍可查询）
    }
    task->scheduled = false;
    --pending_count_;
    cv_.notify_all();  // 取消的可能是最近 deadline，工作线程需重算
    return true;  // 堆中的陈旧条目在弹出时按 scheduled 状态跳过
}

std::optional<DateTime> TaskScheduler::NextFireTime( TaskId id ) const {
    std::lock_guard<std::mutex> lock( mutex_ );

    const auto it = tasks_.find( id );
    if ( it == tasks_.end() || !it->second->scheduled ) {
        return std::nullopt;
    }
    return it->second->wall_label;
}

std::size_t TaskScheduler::TaskCount() const {
    std::lock_guard<std::mutex> lock( mutex_ );
    return pending_count_;
}

std::optional<TaskScheduler::TaskStats> TaskScheduler::GetTaskStats( TaskId id ) const {
    std::lock_guard<std::mutex> lock( mutex_ );

    const auto it = tasks_.find( id );
    if ( it == tasks_.end() ) {
        return std::nullopt;
    }
    const auto &state = *it->second->state;
    TaskStats   stats;
    stats.fire_count   = state.fire_count.load( std::memory_order_relaxed );
    stats.late_count   = state.late_count.load( std::memory_order_relaxed );
    stats.skip_count   = state.skip_count.load( std::memory_order_relaxed );
    stats.reject_count = state.reject_count.load( std::memory_order_relaxed );
    return stats;
}

std::size_t TaskScheduler::AdvanceTo( const Timestamp &wall_now, const SteadyTime &steady_now ) {
    std::vector<PendingJob> jobs;

    {
        std::lock_guard<std::mutex> lock( mutex_ );
        wall_now_   = wall_now;
        steady_now_ = steady_now;

        while ( !wall_heap_.empty() && wall_heap_.top().due <= wall_now ) {
            const WallEntry entry = wall_heap_.top();
            wall_heap_.pop();

            const auto it = tasks_.find( entry.id );
            if ( it == tasks_.end() ) {
                continue;
            }
            const auto &task = it->second;
            if ( !task->scheduled || task->is_steady || task->wall_next != entry.due ) {
                continue;
            }
            DispatchWallFire( task, wall_now, jobs );
        }

        while ( !steady_heap_.empty() && steady_heap_.top().due <= steady_now ) {
            const SteadyEntry entry = steady_heap_.top();
            steady_heap_.pop();

            const auto it = tasks_.find( entry.id );
            if ( it == tasks_.end() ) {
                continue;
            }
            const auto &task = it->second;
            if ( !task->scheduled || !task->is_steady || task->steady_next != entry.due ) {
                continue;
            }
            DispatchSteadyFire( task, steady_now, jobs );
        }
    }

    // 锁外提交/执行：绝不在锁内调用用户代码
    std::size_t fired = 0;
    for ( auto &pending : jobs ) {
        if ( executor_->Submit( std::move( pending.fn ) ) ) {
            pending.state->fire_count.fetch_add( 1, std::memory_order_relaxed );
            ++fired;
        }
        else {
            pending.state->in_flight.fetch_sub( 1, std::memory_order_acq_rel );
            pending.state->reject_count.fetch_add( 1, std::memory_order_relaxed );
        }
    }
    return fired;
}

void TaskScheduler::DispatchWallFire( const std::shared_ptr<Task> &task, const Timestamp &wall_now,
                                      std::vector<PendingJob> &jobs ) {
    const Timestamp scheduled = task->wall_next;
    const DateTime  label     = task->wall_label;

    // ① 先推进计划：基于计划时刻递推（固定网格不漂移）；已过期的中间点整体丢弃
    auto next = task->wall_trigger->NextAfter( label );
    if ( next && next->ToTimestamp() <= wall_now ) {
        next = task->wall_trigger->NextAfter( DateTime( wall_now, label.Zone() ) );
    }
    if ( next ) {
        task->wall_next  = next->ToTimestamp();
        task->wall_label = *next;
        wall_heap_.push( WallEntry{ task->wall_next, task->id } );
    }
    else {
        task->scheduled = false;
        --pending_count_;
    }

    // ② 迟到判定与执行决策
    const auto late_by  = wall_now - scheduled;
    const auto grace    = std::chrono::duration_cast<Timestamp::duration>( task->policy.grace );
    const bool too_late = late_by > grace;
    if ( late_by > Timestamp::duration::zero() ) {
        task->state->late_count.fetch_add( 1, std::memory_order_relaxed );
    }
    if ( too_late && task->policy.late == LatePolicy::Skip ) {
        task->state->skip_count.fetch_add( 1, std::memory_order_relaxed );
        return;
    }
    SubmitAction( task, label, jobs );
}

void TaskScheduler::DispatchSteadyFire( const std::shared_ptr<Task> &task, const SteadyTime &steady_now,
                                        std::vector<PendingJob> &jobs ) {
    const SteadyTime scheduled = task->steady_next;
    const DateTime   label     = task->wall_label;

    auto next = task->steady_trigger->NextAfter( scheduled );
    if ( next && *next <= steady_now ) {
        next = task->steady_trigger->NextAfter( steady_now );
    }
    if ( next ) {
        task->steady_next = *next;
        const auto delta_ns = std::chrono::duration_cast<Nanoseconds>( *next - task->steady_anchor );
        task->wall_label    = task->wall_anchor.Add<Nanoseconds>( delta_ns.count() );
        steady_heap_.push( SteadyEntry{ task->steady_next, task->id } );
    }
    else {
        task->scheduled = false;
        --pending_count_;
    }

    const auto late_by  = steady_now - scheduled;
    const auto grace    = std::chrono::duration_cast<SteadyTime::duration>( task->policy.grace );
    const bool too_late = late_by > grace;
    if ( late_by > SteadyTime::duration::zero() ) {
        task->state->late_count.fetch_add( 1, std::memory_order_relaxed );
    }
    if ( too_late && task->policy.late == LatePolicy::Skip ) {
        task->state->skip_count.fetch_add( 1, std::memory_order_relaxed );
        return;
    }
    SubmitAction( task, label, jobs );
}

void TaskScheduler::SubmitAction( const std::shared_ptr<Task> &task, const DateTime &scheduled,
                                  std::vector<PendingJob> &jobs ) {
    auto &state = *task->state;

    if ( task->policy.coalesce && state.in_flight.load( std::memory_order_acquire ) > 0 ) {
        state.skip_count.fetch_add( 1, std::memory_order_relaxed );
        return;
    }
    state.in_flight.fetch_add( 1, std::memory_order_acq_rel );

    const Action               action    = task->action;
    std::shared_ptr<TaskState> state_ptr = task->state;

    PendingJob pending;
    pending.state = state_ptr;
    pending.fn    = [state_ptr, action, scheduled]() {
        try {
            if ( action ) {
                action( scheduled );
            }
        }
        catch ( ... ) {
            // 用户异常不允许拖垮调度器
        }
        state_ptr->in_flight.fetch_sub( 1, std::memory_order_acq_rel );
    };
    jobs.push_back( std::move( pending ) );
}

}  // namespace utils
