//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <functional>
#include "Core/Types.h"

class Timer {
public:
    Timer() = default;
    ~Timer() = default;

    // _aim_time: 毫秒
    void start(const int _aim_time, const bool _is_loop = false);

    void update(const eng::Time& deltaTime);

    void setCallback(const std::function<void()>& _callback);

    // 已累计时间（毫秒）：供外部读取计时相位（如变身闪烁的形态交替）
    [[nodiscard]] int getPastTime() const {
        return this->past_time;
    }

    // 是否处于计时中（start 后未到期未 stop）：区分"计时中"与"从未启动"
    //（后者 past_time 恒 0，直接比对会误判为计时刚开始）
    [[nodiscard]] bool isActive() const {
        return this->started;
    }

    void reset();

    void stop();

private:
    int past_time = 0;
    int aim_time = 0;
    bool started = false;
    bool is_loop = false;
    std::function<void()> callback;
};
