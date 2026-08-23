//
// Created by MINEC on 2026/6/2.
//

#include "Timer.h"
#include "Core/Types.h"

void Timer::start(const int _aim_time, const bool _is_loop) {
    this->aim_time = _aim_time;
    this->started = true;
    this->is_loop = _is_loop;
    this->reset();
}

void Timer::update(const eng::Time& deltaTime) {
    if (started) {
        past_time += deltaTime.asMilliseconds();
        if (past_time >= aim_time) {
            if (callback) callback();
            if (is_loop) {
                reset();
            } else {
                stop();
            }
        }
    }
}

void Timer::setCallback(const std::function<void()>& _callback) {
    this->callback = _callback;
}

void Timer::reset() {
    past_time = 0;
}

void Timer::stop() {
    started = false;
}
