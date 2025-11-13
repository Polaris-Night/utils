#pragma once
#include <memory>

/**
 * Modern PIMPL macros with dual initialization support.
 *
 * Features:
 *   - PIMPL_DECLARE: declare d_ptr and d_func()
 *   - PIMPL_MAKE_IMPL: create Impl object (for init list or ctor body)
 *   - PIMPL_D: access d pointer in methods
 *
 * Usage:
 *
 *   // In header
 *   class MyClass {
 *       PIMPL_DECLARE(MyClass)
 *   public:
 *       explicit MyClass(std::string name);
 *       ~MyClass(); // must be defined in .cpp
 *   };
 *
 *   // In .cpp
 *   class MyClass::Impl { ... };
 *
 *   // Style 1: Initializer list
 *   MyClass::MyClass(std::string name)
 *       : PIMPL_MAKE_IMPL(MyClass, std::move(name))
 *   {}
 *
 *   // Style 2: Constructor body (e.g., conditional creation)
 *   MyClass::MyClass(bool lazy, std::string name) {
 *       if (!lazy) {
 *           PIMPL_CONSTRUCT_IMPL(MyClass, std::move(name));
 *       }
 *   }
 */

#define PIMPL_DECLARE( Class )               \
private:                                     \
    class Impl;                              \
    std::unique_ptr<Impl> d_ptr;             \
                                             \
protected:                                   \
    /** Access implementation (non-const) */ \
    Impl *d_func() noexcept {                \
        return d_ptr.get();                  \
    }                                        \
    /** Access implementation (const) */     \
    const Impl *d_func() const noexcept {    \
        return d_ptr.get();                  \
    }

/**
 * @brief Use in constructor initializer list to construct Impl with arguments.
 * Example: : PIMPL_MAKE_IMPL(MyClass, arg1, arg2)
 */
#define PIMPL_MAKE_IMPL( Class, ... ) d_ptr( std::make_unique<Class::Impl>( __VA_ARGS__ ) )

/**
 * @brief Use in constructor/function body to assign new Impl.
 * Example: PIMPL_CONSTRUCT_IMPL(MyClass, arg1, arg2);
 */
#define PIMPL_CONSTRUCT_IMPL( Class, ... ) d_ptr = std::make_unique<Class::Impl>( __VA_ARGS__ )

/**
 * @brief Shortcut to get a local const pointer to Impl (like Qt's Q_D).
 * Example: PIMPL_D(MyClass); d->method();
 */
#define PIMPL_D( Class ) auto *const d = d_func()