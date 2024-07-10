#include "Timer.h"

namespace utils {

void ElapsedTimer::Start() {
    if ( !is_start_ ) {
        start_time_ = GetNow();
        is_start_   = true;
        is_pause_   = false;
    }
}

void ElapsedTimer::Stop( bool reset_time ) {
    if ( reset_time ) {
        start_time_ = GetReset();
    }
    is_start_ = false;
    is_pause_ = false;
}

void ElapsedTimer::Pause() {
    if ( is_start_ && !is_pause_ ) {
        pause_time_ = GetNow();
        is_pause_   = true;
    }
}

void ElapsedTimer::Resume() {
    if ( is_start_ && is_pause_ ) {
        start_time_ += GetNow() - pause_time_;
        is_pause_ = false;
    }
}

void ElapsedTimer::Restart() {
    Stop();
    Start();
}

std::chrono::steady_clock::time_point ElapsedTimer::GetNow() const {
    return std::chrono::steady_clock::now();
}

std::chrono::steady_clock::time_point ElapsedTimer::GetReset() const {
    return std::chrono::steady_clock::time_point( Nanoseconds( 0 ) );
}


}  // namespace utils
