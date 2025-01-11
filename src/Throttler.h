#pragma once

#include <chrono>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace utils {

template <typename Rep, typename Period>
class Throttler final {
public:
    using IntervalType = std::chrono::duration<Rep, Period>;

    Throttler( const IntervalType &interval ) : interval_( interval ) {}
    Throttler( const Throttler & )            = delete;
    Throttler &operator=( const Throttler & ) = delete;
    Throttler( Throttler && )                 = delete;
    Throttler &operator=( Throttler && )      = delete;

    template <typename Func, typename... Args,
              typename R = std::conditional_t<std::is_void_v<std::invoke_result_t<Func, Args...>>, bool,
                                              std::pair<bool, std::optional<std::invoke_result_t<Func, Args...>>>>>
    auto Run( Func &&func, Args &&...args ) -> R {
        auto now = std::chrono::steady_clock::now();
        if ( now - last_time_ >= interval_ ) {
            last_time_ = now;
            if constexpr ( std::is_same_v<R, bool> ) {
                std::invoke( std::forward<Func>( func ), std::forward<Args>( args )... );
                return true;
            }
            else {
                auto result = std::invoke( std::forward<Func>( func ), std::forward<Args>( args )... );
                return std::make_pair( true, result );
            }
        }
        if constexpr ( std::is_same_v<R, bool> ) {
            return false;
        }
        else {
            return std::make_pair( false, std::nullopt );
        }
    }

private:
    IntervalType                          interval_;
    std::chrono::steady_clock::time_point last_time_;
};

}  // namespace utils