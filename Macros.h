#pragma once

#if defined( _WIN32 ) || defined( _WIN64 )
inline constexpr bool is_windows = true;
    #define PLATFORM_OS_WINDOWS 1
#else
static inline constexpr bool is_windows = false;
#endif

#if defined( __linux__ )
inline constexpr bool is_linux = true;
    #define PLATFORM_OS_LINUX 1
#else
inline constexpr bool        is_linux   = false;
#endif

#if defined( __APPLE__ )
inline constexpr bool is_osx = true;
    #define PLATFORM_OS_OSX 1
#else
inline constexpr bool        is_osx     = false;
#endif