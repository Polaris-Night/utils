#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace utils {

class SingleTrigger {
public:
    using Ptr  = std::shared_ptr<SingleTrigger>;
    using Weak = std::weak_ptr<SingleTrigger>;

    SingleTrigger( bool trigger );

    void Trigger();

    bool Consume();

    template <typename Func, typename... Args>
    void ExecuteIfTriggered( Func &&func, Args &&...args ) {
        bool expected = true;
        if ( trigger_.compare_exchange_weak( expected, false ) ) {
            std::invoke( std::forward<Func>( func ), std::forward<Args>( args )... );
        }
    }

    static Ptr Create( bool trigger = false );

    static void Trigger( Ptr &ptr );
    static void Trigger( Weak &weak );

private:
    std::atomic_bool trigger_;
};

}  // namespace utils
