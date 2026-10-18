#pragma once

#include <functional>

namespace utils {

/**
 * @brief 任务执行器接口
 *
 * 调度器只负责"何时触发"，执行方式（同步/线程池/事件循环）由执行器决定。
 */
class TaskExecutor {
public:
    virtual ~TaskExecutor() = default;

    /**
     * @brief 提交一个待执行动作
     *
     * @param job 待执行动作
     * @return true 已受理（可能异步执行）；false 被拒绝（背压）
     */
    virtual bool Submit( std::function<void()> job ) = 0;
};

/**
 * @brief 同步执行器：Submit 内直接执行动作（默认）
 */
class InlineExecutor final : public TaskExecutor {
public:
    bool Submit( std::function<void()> job ) override {
        if ( job ) {
            job();
        }
        return true;
    }
};

}  // namespace utils
