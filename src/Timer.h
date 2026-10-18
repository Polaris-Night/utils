#pragma once

#include "Macros.h"
#include "DateTime.h"

namespace utils {

class ElapsedTimer {
public:
    ElapsedTimer() = default;
    DISABLE_COPY_MOVE( ElapsedTimer )

    /**
     * @brief 启动计时器，已启动（含暂停中）时再次调用无效
     *
     * 停止后再次 Start 会从已累计时长继续。
     */
    void Start();
    /**
     * @brief 停止计时器
     *
     * @param reset_time 停止时是否清零已计时的时长；false 表示冻结并保留累计时长
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
     * 未启动时返回 0；暂停或停止后返回冻结的累计时长。
     *
     * @tparam TimeUnit 支持日、小时、分钟、秒、毫秒、微秒、纳秒
     * @tparam ReturnType 支持数值类型；整型返回截断值，浮点型返回带小数的值
     * @return ReturnType
     */
    template <
        typename TimeUnit = Seconds, typename ReturnType = int64_t,
        std::enable_if_t<std::is_arithmetic_v<ReturnType>, bool> = true,
        std::enable_if_t<std::disjunction_v<std::is_same<TimeUnit, Days>, std::is_same<TimeUnit, Hours>,
                                            std::is_same<TimeUnit, Minutes>, std::is_same<TimeUnit, Seconds>,
                                            std::is_same<TimeUnit, Milliseconds>, std::is_same<TimeUnit, Microseconds>,
                                            std::is_same<TimeUnit, Nanoseconds>>,
                         bool>                                   = true>
    ReturnType GetElapsed() const {
        Nanoseconds total = accumulated_;
        if ( is_running_ && !is_pause_ ) {
            total += std::chrono::duration_cast<Nanoseconds>( GetNow() - start_time_ );
        }
        if constexpr ( std::is_integral_v<ReturnType> ) {
            return static_cast<ReturnType>( std::chrono::duration_cast<TimeUnit>( total ).count() );
        }
        else {
            return static_cast<ReturnType>( std::chrono::duration<double, typename TimeUnit::period>( total ).count() );
        }
    }

private:
    std::chrono::steady_clock::time_point GetNow() const;

    bool                                  is_running_{ false };
    bool                                  is_pause_{ false };
    std::chrono::steady_clock::time_point start_time_{};
    Nanoseconds                           accumulated_{ 0 };
};

class Timer {
public:
    Timer() = default;
    Timer( int msec );
    Timer( Milliseconds msec );
    DISABLE_COPY_MOVE( Timer )

    /**
     * @brief 启动计时器，已运行时调用无效
     *
     */
    void Start();
    /**
     * @brief 停止计时器，停止后 IsTimeout 返回 false
     *
     */
    void Stop();
    /**
     * @brief 重启计时器，重新开始计时
     *
     */
    void Restart();
    void SetInterval( int msec );
    void SetInterval( Milliseconds msec );

    /**
     * @brief 计时器是否正在运行
     *
     */
    bool IsRunning() const;
    /**
     * @brief 是否已达到设定的时间间隔
     *
     * 仅在运行状态下可能返回 true；超时后保持 true 直到 Stop/Restart。
     *
     */
    bool IsTimeout() const;

private:
    bool                                  is_running_{ false };
    Milliseconds                          interval_{};
    std::chrono::steady_clock::time_point start_time_{};
};

}  // namespace utils
