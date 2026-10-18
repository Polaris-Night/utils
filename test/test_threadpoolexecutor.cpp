#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include "ThreadPoolExecutor.h"
#include "gtest/gtest.h"

using namespace utils;

namespace {

bool WaitUntil( const std::function<bool()> &predicate, std::chrono::milliseconds timeout = std::chrono::seconds( 3 ) ) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while ( std::chrono::steady_clock::now() < deadline ) {
        if ( predicate() ) {
            return true;
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }
    return predicate();
}

}  // namespace

TEST( ThreadPoolExecutorTest, ExecutesAllSubmittedJobs ) {
    ThreadPoolExecutor pool( 2 );
    std::atomic<int>   counter{ 0 };

    constexpr int kJobs = 100;
    for ( int i = 0; i < kJobs; ++i ) {
        ASSERT_TRUE( pool.Submit( [&counter]() { counter.fetch_add( 1, std::memory_order_relaxed ); } ) );
    }

    EXPECT_TRUE( WaitUntil( [&] { return counter.load() == kJobs; } ) );
    EXPECT_EQ( counter.load(), kJobs );
}

TEST( ThreadPoolExecutorTest, RunsJobsConcurrentlyOnAllThreads ) {
    constexpr std::size_t kThreads = 4;
    ThreadPoolExecutor    pool( kThreads );

    std::mutex              mutex;
    std::condition_variable cv;
    int                     arrived = 0;

    for ( std::size_t i = 0; i < kThreads; ++i ) {
        ASSERT_TRUE( pool.Submit( [&]() {
            std::unique_lock<std::mutex> lock( mutex );
            ++arrived;
            cv.notify_all();
            cv.wait( lock, [&]() { return arrived == static_cast<int>( kThreads ); } );
        } ) );
    }

    std::unique_lock<std::mutex> lock( mutex );
    // 若线程池不能并发执行（如只有 1 个 worker），先到的任务会一直等到超时
    EXPECT_TRUE( cv.wait_for( lock, std::chrono::seconds( 3 ),
                              [&]() { return arrived == static_cast<int>( kThreads ); } ) );
}

TEST( ThreadPoolExecutorTest, RejectsWhenQueueIsFull ) {
    ThreadPoolExecutor pool( 1, 1 );  // 1 个 worker + 队列容量 1

    std::mutex              mutex;
    std::condition_variable cv;
    bool                    release = false;

    ASSERT_TRUE( pool.Submit( [&]() {
        std::unique_lock<std::mutex> lock( mutex );
        cv.wait( lock, [&]() { return release; } );
    } ) );
    ASSERT_TRUE( WaitUntil( [&] { return pool.ActiveCount() == 1; } ) );

    ASSERT_TRUE( pool.Submit( []() {} ) );   // 占住唯一的队列位
    EXPECT_FALSE( pool.Submit( []() {} ) );  // 队列满 → 背压拒绝
    EXPECT_EQ( pool.PendingCount(), 1u );

    {
        std::lock_guard<std::mutex> lock( mutex );
        release = true;
    }
    cv.notify_all();

    EXPECT_TRUE( WaitUntil( [&] { return pool.PendingCount() == 0 && pool.ActiveCount() == 0; } ) );
}

TEST( ThreadPoolExecutorTest, UnboundedQueueAcceptsBurst ) {
    ThreadPoolExecutor pool( 1, 0 );  // 无界队列

    std::mutex              mutex;
    std::condition_variable cv;
    bool                    release = false;

    ASSERT_TRUE( pool.Submit( [&]() {
        std::unique_lock<std::mutex> lock( mutex );
        cv.wait( lock, [&]() { return release; } );
    } ) );
    ASSERT_TRUE( WaitUntil( [&] { return pool.ActiveCount() == 1; } ) );

    std::atomic<int> counter{ 0 };
    constexpr int    kJobs = 500;
    for ( int i = 0; i < kJobs; ++i ) {
        ASSERT_TRUE( pool.Submit( [&counter]() { counter.fetch_add( 1, std::memory_order_relaxed ); } ) );
    }

    {
        std::lock_guard<std::mutex> lock( mutex );
        release = true;
    }
    cv.notify_all();

    EXPECT_TRUE( WaitUntil( [&] { return counter.load() == kJobs; } ) );
}

TEST( ThreadPoolExecutorTest, JobExceptionDoesNotKillWorker ) {
    ThreadPoolExecutor pool( 1 );
    std::atomic<int>   counter{ 0 };

    ASSERT_TRUE( pool.Submit( []() { throw std::runtime_error( "boom" ); } ) );
    ASSERT_TRUE( pool.Submit( [&counter]() { counter.fetch_add( 1, std::memory_order_relaxed ); } ) );

    EXPECT_TRUE( WaitUntil( [&] { return counter.load() == 1; } ) );
}

TEST( ThreadPoolExecutorTest, DestructorDrainsPendingJobs ) {
    std::atomic<int> counter{ 0 };

    constexpr int kJobs = 200;
    {
        ThreadPoolExecutor pool( 2 );
        for ( int i = 0; i < kJobs; ++i ) {
            ASSERT_TRUE( pool.Submit( [&counter]() { counter.fetch_add( 1, std::memory_order_relaxed ); } ) );
        }
    }  // 析构：排空队列后 join

    EXPECT_EQ( counter.load(), kJobs );
}

TEST( ThreadPoolExecutorTest, NullJobAcceptedAsNoOp ) {
    ThreadPoolExecutor pool( 1 );

    EXPECT_TRUE( pool.Submit( std::function<void()>() ) );
    EXPECT_EQ( pool.PendingCount(), 0u );
    EXPECT_EQ( pool.ActiveCount(), 0u );
}

TEST( ThreadPoolExecutorTest, AutoThreadCount ) {
    ThreadPoolExecutor pool;  // threads = 0 → 自动

    const unsigned    hc       = std::thread::hardware_concurrency();
    const std::size_t expected = hc == 0 ? 1 : static_cast<std::size_t>( hc );

    EXPECT_EQ( pool.ThreadCount(), expected );
    EXPECT_GE( pool.ThreadCount(), 1u );
}
