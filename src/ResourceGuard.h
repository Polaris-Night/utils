#pragma once

#include <functional>
#include <stdexcept>
#include <utility>

namespace utils {

template <typename T>
class ResourceGuard {
public:
    using Deleter = std::function<void( T * )>;
    explicit ResourceGuard( T *resource, Deleter &&deleter, bool check = true ) :
        resource_( resource ), deleter_( std::move( deleter ) ) {
        if ( !check ) {
            return;
        }
        if ( resource_ == nullptr ) {
            throw std::runtime_error( "ResourceGuard: resource is nullptr" );
        }
        if ( !deleter_ ) {
            throw std::runtime_error( "ResourceGuard: deleter is not a callable" );
        }
    }
    ResourceGuard( const ResourceGuard & ) = delete;
    ResourceGuard( ResourceGuard &&other ) noexcept { *this = std::move( other ); }
    ResourceGuard &operator=( const ResourceGuard & ) = delete;
    ResourceGuard &operator=( ResourceGuard &&other ) noexcept {
        resource_ = std::exchange( other.resource_, nullptr );
        deleter_  = std::move( other.deleter_ );
        return *this;
    }
    ~ResourceGuard() {
        if ( resource_ && deleter_ ) {
            deleter_( resource_ );
        }
    }

    T *Get() const noexcept { return resource_; }

    T *Release() noexcept {
        deleter_ = nullptr;
        return std::exchange( resource_, nullptr );
    }

    void Reset() noexcept {
        if ( resource_ && deleter_ ) {
            deleter_( resource_ );
        }
        resource_ = nullptr;
        deleter_  = nullptr;
    }

private:
    T      *resource_ = nullptr;
    Deleter deleter_;
};

}  // namespace utils
