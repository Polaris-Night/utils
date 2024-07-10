#pragma once

#include "Macros.h"
#include "TimeUtil.h"

namespace utils {

class ElapsedTimer {
public:
    ElapsedTimer() = default;
    DISABLE_COPY_MOVE( ElapsedTimer )

    /**
     * @brief 启动计时器，已启动后再次调用无效
     *
     */
    void Start();
    /**
     * @brief 停止计时器，并将计时清零
     *
     * @param reset_time 停止时是否清除计时
     */
    void Stop( bool reset_time = true );
    /**
     * @brief 暂停计时器，未启动或已暂停时调用无效
     *
     */
    void Pause();
    /**
     * @brief 从暂停中恢复计时器，未启动或正在运行时调用无效
     *
     */
    void Resume();
    /**
     * @brief 重启计时器，计时清零并重启
     *
     */
    void Restart();

    /**
     * @brief 获取计时时长
     *
     * @tparam TimeUnit 支持日、小时、分钟、秒、毫秒、微秒
     * @tparam ReturnType 支持数值类型
     * @return ReturnType
     */
    template <
        typename TimeUnit = Seconds, typename ReturnType = int64_t,
        std::enable_if_t<std::is_arithmetic_v<ReturnType>, bool> = true,
        std::enable_if_t<std::disjunction_v<std::is_same<TimeUnit, Days>, std::is_same<TimeUnit, Hours>,
                                            std::is_same<TimeUnit, Minutes>, std::is_same<TimeUnit, Seconds>,
                                            std::is_same<TimeUnit, Milliseconds>, std::is_same<TimeUnit, Microseconds>>,
                         bool>                                   = true>
    ReturnType GetElapsed() const {
        if ( is_pause_ ) {
            return std::chrono::duration_cast<TimeUnit>( pause_time_ - start_time_ ).count();
        }
        auto now = GetNow();
        if constexpr ( std::is_integral_v<ReturnType> ) {
            return std::chrono::duration_cast<TimeUnit>( now - start_time_ ).count();
        }

        auto duration = std::chrono::duration_cast<Microseconds>( now - start_time_ ).count();
        if constexpr ( std::is_same_v<TimeUnit, Days> ) {
            return duration / 1e6 / 86400;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Hours> ) {
            return duration / 1e6 / 3600;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Minutes> ) {
            return duration / 1e6 / 60;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Seconds> ) {
            return duration / 1e6;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Milliseconds> ) {
            return duration / 1e3;
        }
        else if constexpr ( std::is_same_v<TimeUnit, Microseconds> ) {
            return duration;
        }
    }

private:
    std::chrono::steady_clock::time_point GetNow() const;
    std::chrono::steady_clock::time_point GetReset() const;

    bool                                  is_start_{ false };
    bool                                  is_pause_{ false };
    std::chrono::steady_clock::time_point start_time_{};
    std::chrono::steady_clock::time_point pause_time_{};
};

}  // namespace utils
