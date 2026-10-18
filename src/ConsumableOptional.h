#pragma once

#include <mutex>
#include <optional>
#include <utility>

namespace utils {

/**
 * @brief 一次性消费的可选值容器，支持可选的线程安全访问
 *
 * @tparam T 值类型
 */
template <typename T>
class ConsumableOptional {
public:
    ConsumableOptional( const T &value, bool thread_safe = false ) : opt_( value ), thread_safe_( thread_safe ) {}
    template <typename U>
    ConsumableOptional( U &&value, bool thread_safe = false ) :
        opt_( std::forward<U>( value ) ), thread_safe_( thread_safe ) {}
    ConsumableOptional( bool thread_safe = false ) : thread_safe_( thread_safe ) {}
    ConsumableOptional( ConsumableOptional && )            = delete;
    ConsumableOptional &operator=( ConsumableOptional && ) = delete;

    void set( const T &value ) {
        std::unique_lock<std::mutex> lock( mutex_, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        opt_ = value;
    }
    template <typename U>
    void set( U &&value ) {
        std::unique_lock<std::mutex> lock( mutex_, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        opt_ = std::forward<U>( value );
    }

    /**
     * @brief 消费当前值
     *
     * @return std::optional<T> 存在值时返回该值并清空容器；否则返回 std::nullopt
     */
    std::optional<T> consume() {
        std::unique_lock<std::mutex> lock( mutex_, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        if ( opt_.has_value() ) {
            T value = std::move( opt_.value() );
            opt_.reset();
            return value;
        }
        return std::nullopt;
    }

    void reset() {
        std::unique_lock<std::mutex> lock( mutex_, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        opt_.reset();
    }

    /**
     * @brief 当前是否持有值
     *
     * 注意：thread_safe=true 时会加锁，锁操作可能抛出 std::system_error
     */
    bool has_value() const {
        std::unique_lock<std::mutex> lock( mutex_, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        return opt_.has_value();
    }

    explicit operator bool() const { return has_value(); }

private:
    mutable std::mutex mutex_;
    std::optional<T>   opt_;
    const bool         thread_safe_;
};

}  // namespace utils
