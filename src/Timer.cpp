#include "Timer.h"

namespace utils {

void ElapsedTimer::Start() {
    if ( !is_running_ ) {
        start_time_ = GetNow();
        is_running_ = true;
        is_pause_   = false;
    }
}

void ElapsedTimer::Stop( bool reset_time ) {
    if ( is_running_ && !is_pause_ ) {
        accumulated_ += std::chrono::duration_cast<Nanoseconds>( GetNow() - start_time_ );
    }
    is_running_ = false;
    is_pause_   = false;
    if ( reset_time ) {
        accumulated_ = Nanoseconds( 0 );
    }
}

void ElapsedTimer::Pause() {
    if ( is_running_ && !is_pause_ ) {
        accumulated_ += std::chrono::duration_cast<Nanoseconds>( GetNow() - start_time_ );
        is_pause_ = true;
    }
}

void ElapsedTimer::Resume() {
    if ( is_running_ && is_pause_ ) {
        start_time_ = GetNow();
        is_pause_   = false;
    }
}

void ElapsedTimer::Restart() {
    accumulated_ = Nanoseconds( 0 );
    start_time_  = GetNow();
    is_running_  = true;
    is_pause_    = false;
}

std::chrono::steady_clock::time_point ElapsedTimer::GetNow() const {
    return std::chrono::steady_clock::now();
}

Timer::Timer( int msec ) : interval_( Milliseconds( msec ) ) {}

Timer::Timer( Milliseconds msec ) : interval_( msec ) {}

void Timer::Start() {
    if ( !is_running_ ) {
        start_time_ = std::chrono::steady_clock::now();
        is_running_ = true;
    }
}

void Timer::Stop() {
    is_running_ = false;
}

void Timer::Restart() {
    start_time_ = std::chrono::steady_clock::now();
    is_running_ = true;
}

void Timer::SetInterval( int msec ) {
    interval_ = Milliseconds( msec );
}

void Timer::SetInterval( Milliseconds msec ) {
    interval_ = msec;
}

bool Timer::IsRunning() const {
    return is_running_;
}

bool Timer::IsTimeout() const {
    if ( !is_running_ ) {
        return false;
    }
    return std::chrono::steady_clock::now() - start_time_ >= interval_;
}

}  // namespace utils
