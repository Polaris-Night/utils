#pragma once

#include <iostream>
#include <mutex>
#include <optional>
#include <utility>

namespace utils {

template <typename T>
class ConsumableOptional {
public:
    ConsumableOptional( const T &value, bool thread_safe = false ) : opt( value ), thread_safe_( thread_safe ) {}
    template <typename U>
    ConsumableOptional( U &&value, bool thread_safe = false ) :
        opt( std::forward<U>( value ) ), thread_safe_( thread_safe ) {}
    ConsumableOptional( bool thread_safe = false ) : thread_safe_( thread_safe ) {}
    ConsumableOptional( ConsumableOptional && )            = delete;
    ConsumableOptional &operator=( ConsumableOptional && ) = delete;

    void set( const T &value ) {
        std::unique_lock<std::mutex> lock( mutex, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        opt = value;
    }
    template <typename U>
    void set( U &&value ) {
        std::unique_lock<std::mutex> lock( mutex, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        opt = std::forward<U>( value );
    }

    std::optional<T> comsume() {
        std::unique_lock<std::mutex> lock( mutex, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        if ( opt.has_value() ) {
            T value = std::move( opt.value() );
            opt.reset();
            return value;
        }
        return std::nullopt;
    }

    bool has_value() const noexcept {
        std::unique_lock<std::mutex> lock( mutex, std::defer_lock );
        if ( thread_safe_ ) {
            lock.lock();
        }
        return opt.has_value();
    }

    constexpr explicit operator bool() const noexcept { return has_value(); }

private:
    mutable std::mutex mutex;
    std::optional<T>   opt;
    bool               thread_safe_;
};

}  // namespace utils
