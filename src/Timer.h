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

    void reset();

    void stop();

private:
    int past_time = 0;
    int aim_time = 0;
    bool started = false;
    bool is_loop = false;
    std::function<void()> callback;
};
