#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "Macros.h"
#include "TaskExecutor.h"

#if defined( __linux__ )
#include <pthread.h>
#endif

namespace utils {

/**
 * @brief 线程池执行器：Submit 仅入队，动作在线程池上异步执行
 *
 * 与 TaskScheduler 组合的语义：
 *   - 派发线程只入队，不被慢回调阻塞（Stop 也因此始终及时）；
 *   - 队列满时 Submit 返回 false（背压），调度器计入 reject_count，
 *     该触发在下次 AdvanceTo 时按迟到策略处理；
 *   - 执行器生命周期必须长于调度器（调度器不拥有执行器）；
 *   - coalesce = false 时同一任务的两回回调可能在不同线程并行，action 需线程安全。
 *
 * 析构语义：排空剩余队列并等待在途任务全部完成后回收线程（不丢已受理的提交；
 * 卡死不可返回的任务会阻塞析构，生产环境请确保回调可返回）。
 */
class ThreadPoolExecutor final : public TaskExecutor {
public:
    /**
     * @brief 构造并启动线程池
     *
     * @param threads        工作线程数；0 = 自动取 max(1, hardware_concurrency)
     * @param queue_capacity 队列容量上限；0 = 无界（慎用：持续过载时内存会增长，
     *                       有界队列通过 Submit 返回 false 提供可控背压）
     */
    explicit ThreadPoolExecutor( std::size_t threads = 0, std::size_t queue_capacity = 256 ) :
        queue_capacity_( queue_capacity ) {
        const std::size_t count = threads != 0 ? threads : DefaultThreadCount();
        workers_.reserve( count );
        for ( std::size_t i = 0; i < count; ++i ) {
            workers_.emplace_back( [this, i]() { WorkerLoop( i ); } );
        }
    }

    DISABLE_COPY_MOVE( ThreadPoolExecutor )

    ~ThreadPoolExecutor() override {
        {
            std::lock_guard<std::mutex> lock( mutex_ );
            stop_ = true;
        }
        not_empty_.notify_all();
        for ( auto &worker : workers_ ) {
            if ( worker.joinable() ) {
                worker.join();
            }
        }
    }

    /**
     * @brief 提交一个待执行动作
     *
     * @param job 待执行动作（空函数视为空操作并受理）
     * @return true 已受理；false 被拒绝（队列满或已停止）
     */
    bool Submit( std::function<void()> job ) override {
        if ( !job ) {
            return true;
        }
        {
            std::lock_guard<std::mutex> lock( mutex_ );
            if ( stop_ || ( queue_capacity_ != 0 && queue_.size() >= queue_capacity_ ) ) {
                return false;
            }
            queue_.push( std::move( job ) );
        }
        not_empty_.notify_one();
        return true;
    }

    /// 工作线程数
    [[nodiscard]] std::size_t ThreadCount() const {
        return workers_.size();
    }

    /// 排队中的动作数
    [[nodiscard]] std::size_t PendingCount() const {
        std::lock_guard<std::mutex> lock( mutex_ );
        return queue_.size();
    }

    /// 执行中的动作数
    [[nodiscard]] std::size_t ActiveCount() const {
        std::lock_guard<std::mutex> lock( mutex_ );
        return active_;
    }

private:
    [[nodiscard]] static std::size_t DefaultThreadCount() {
        const unsigned hc = std::thread::hardware_concurrency();
        return hc == 0 ? 1 : static_cast<std::size_t>( hc );
    }

    static void SetThreadName( std::size_t index ) {
#if defined( __linux__ )
        char name[ 16 ] = {};
        std::snprintf( name, sizeof( name ), "utils-pool-%zu", index );
        pthread_setname_np( pthread_self(), name );
#else
        (void)index;
#endif
    }

    void WorkerLoop( std::size_t index ) {
        SetThreadName( index );

        std::unique_lock<std::mutex> lock( mutex_ );
        while ( true ) {
            not_empty_.wait( lock, [this]() { return stop_ || !queue_.empty(); } );
            if ( queue_.empty() ) {
                break;  // 已停止且剩余队列排空
            }

            std::function<void()> job = std::move( queue_.front() );
            queue_.pop();
            ++active_;

            // 锁外执行：用户代码绝不持锁运行
            lock.unlock();
            try {
                job();
            }
            catch ( ... ) {
                // 用户异常不允许杀死工作线程（与调度器的回调纪律一致）
            }
            lock.lock();
            --active_;
        }
    }

    mutable std::mutex      mutex_;
    std::condition_variable not_empty_;
    std::queue<std::function<void()>> queue_;
    std::size_t             queue_capacity_{ 256 };
    std::size_t             active_{ 0 };
    bool                    stop_{ false };
    std::vector<std::thread> workers_;  ///< 声明在最后：其构造发生在其余成员初始化之后
};

}  // namespace utils
