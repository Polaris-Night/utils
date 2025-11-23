#pragma once

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace utils {

template <typename T, typename E>
class Result;

template <typename E>
class Result<void, E>;

namespace detail {
struct ok_tag {};
struct err_tag {};
}  // namespace detail

// --- 通用 Result<T, E> ---
template <typename T, typename E>
class Result {
    static_assert( !std::is_same_v<T, void>, "Use Result<void, E> for void type" );
    static_assert( !std::is_reference_v<T>, "T must not be a reference" );
    static_assert( !std::is_reference_v<E>, "E must not be a reference" );

public:
    using ValueType = T;
    using ErrorType = E;

    /// @brief 构造 Ok 值
    template <typename U = T, std::enable_if_t<std::is_constructible_v<T, U &&>, int> = 0>
    constexpr Result( U &&val ) noexcept( std::is_nothrow_constructible_v<T, U &&> ) :
        data_( std::in_place_index<ok_index_>, std::forward<U>( val ) ) {}

    /// @brief 构造 Err 值
    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, G &&>, int> = 0>
    constexpr Result( detail::err_tag, G &&err ) noexcept( std::is_nothrow_constructible_v<E, G &&> ) :
        data_( std::in_place_index<err_index_>, std::forward<G>( err ) ) {}

    /// @brief 检查是否为 Ok
    [[nodiscard]] constexpr bool IsOk() const noexcept { return data_.index() == 0; }

    /// @brief 检查是否为 Err
    [[nodiscard]] constexpr bool IsErr() const noexcept { return data_.index() == 1; }

    /// @brief 获取 Ok 值（调用前需确保 IsOk()）
    [[nodiscard]] constexpr T &Value() & {
        if ( IsErr() ) {
            throw std::bad_variant_access{};
        }
        return std::get<ok_index_>( data_ );
    }

    [[nodiscard]] constexpr const T &Value() const & {
        if ( IsErr() ) {
            throw std::bad_variant_access{};
        }
        return std::get<ok_index_>( data_ );
    }

    [[nodiscard]] constexpr T &&Value() && {
        if ( IsErr() ) {
            throw std::bad_variant_access{};
        }
        return std::move( std::get<ok_index_>( data_ ) );
    }

    /// @brief 获取错误（调用前需确保 IsErr()）
    [[nodiscard]] constexpr E &Error() & {
        if ( IsOk() ) {
            throw std::bad_variant_access{};
        }
        return std::get<err_index_>( data_ );
    }

    [[nodiscard]] constexpr const E &Error() const & {
        if ( IsOk() ) {
            throw std::bad_variant_access{};
        }
        return std::get<err_index_>( data_ );
    }

    /// @brief 安全获取值，出错时返回默认值
    template <typename U>
    [[nodiscard]] constexpr T ValueOr( U &&default_val ) const & {
        return IsOk() ? Value() : static_cast<T>( std::forward<U>( default_val ) );
    }

    /// @brief 安全获取错误，成功时返回默认错误
    template <typename U>
    [[nodiscard]] constexpr E ErrorOr( U &&default_err ) const & {
        return IsErr() ? Error() : static_cast<E>( std::forward<U>( default_err ) );
    }

    /// @brief map: 仅对 Ok 值进行变换
    template <typename F>
    [[nodiscard]] auto Map( F &&f ) const -> Result<std::decay_t<std::invoke_result_t<F, T>>, E> {
        using R = std::decay_t<std::invoke_result_t<F, T>>;
        if ( IsOk() ) {
            return Result<R, E>( f( Value() ) );
        }
        else {
            return Result<R, E>( detail::err_tag{}, Error() );
        }
    }

    /// @brief and_then: 对 Ok 值执行返回 Result 的函数
    template <typename F>
    [[nodiscard]] auto AndThen( F &&f ) const -> std::invoke_result_t<F, T> {
        using Ret = std::invoke_result_t<F, T>;
        static_assert( std::is_same_v<typename Ret::ErrorType, E>, "Error type must match in and_then" );
        if ( IsOk() ) {
            return f( Value() );
        }
        else {
            return Ret( detail::err_tag{}, Error() );
        }
    }

private:
    std::variant<T, E>   data_;
    static constexpr int ok_index_  = 0;
    static constexpr int err_index_ = 1;
};

// --- Result<void, E> 特化 ---
template <typename E>
class Result<void, E> {
    static_assert( !std::is_reference_v<E>, "E must not be a reference" );

public:
    using ValueType = void;
    using ErrorType = E;

    /// @brief 构造 Ok（成功，无值）
    constexpr Result() noexcept : data_( std::in_place_index<ok_index_> ) {}

    /// @brief 构造 Err
    template <typename G = E, std::enable_if_t<std::is_constructible_v<E, G &&>, int> = 0>
    constexpr Result( detail::err_tag, G &&err ) noexcept( std::is_nothrow_constructible_v<E, G &&> ) :
        data_( std::in_place_index<err_index_>, std::forward<G>( err ) ) {}

    [[nodiscard]] constexpr bool IsOk() const noexcept { return data_.index() == 0; }
    [[nodiscard]] constexpr bool IsErr() const noexcept { return data_.index() == 1; }

    /// @brief 获取错误（调用前需确保 IsErr()）
    [[nodiscard]] constexpr E &Error() & {
        if ( IsOk() ) {
            throw std::bad_variant_access{};
        }
        return std::get<err_index_>( data_ );
    }

    [[nodiscard]] constexpr const E &Error() const & {
        if ( IsOk() ) {
            throw std::bad_variant_access{};
        }
        return std::get<err_index_>( data_ );
    }

    template <typename U>
    [[nodiscard]] constexpr E ErrorOr( U &&default_err ) const & {
        return IsErr() ? Error() : static_cast<E>( std::forward<U>( default_err ) );
    }

    /// @brief Map：对 void Ok 无操作，仅传递
    template <typename F>
    [[nodiscard]] Result<void, E> Map( F && ) const {
        if ( IsOk() ) {
            return Result<void, E>{};
        }
        else {
            return Result<void, E>( detail::err_tag{}, Error() );
        }
    }

    /// @brief AndThen：执行返回 Result 的函数
    template <typename F>
    [[nodiscard]] auto AndThen( F &&f ) const -> std::invoke_result_t<F> {
        using Ret = std::invoke_result_t<F>;
        static_assert( std::is_same_v<typename Ret::ErrorType, E>, "Error type must match in and_then" );
        if ( IsOk() ) {
            return f();
        }
        else {
            return Ret( detail::err_tag{}, Error() );
        }
    }

private:
    std::variant<std::monostate, E> data_;
    static constexpr int            ok_index_  = 0;
    static constexpr int            err_index_ = 1;
};

// --- 辅助函数：构造 Result ---
template <typename T, typename E = std::string>
[[nodiscard]] constexpr auto MakeOk( T &&val ) -> Result<std::decay_t<T>, std::decay_t<E>> {
    return Result<std::decay_t<T>, std::decay_t<E>>( std::forward<T>( val ) );
}

template <typename E = std::string>
[[nodiscard]] constexpr auto MakeOk() -> Result<void, std::decay_t<E>> {
    return Result<void, std::decay_t<E>>{};
}

template <typename E>
[[nodiscard]] constexpr auto MakeErr( E &&err ) -> Result<void, std::decay_t<E>> {
    return Result<void, std::decay_t<E>>( detail::err_tag{}, std::forward<E>( err ) );
}

template <typename T, typename E>
[[nodiscard]] constexpr auto MakeErr( E &&err ) -> Result<T, std::decay_t<E>> {
    return Result<T, std::decay_t<E>>( detail::err_tag{}, std::forward<E>( err ) );
}

}  // namespace utils