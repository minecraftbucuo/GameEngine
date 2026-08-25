//
// Created by MINEC on 2026/5/14.
//

#pragma once
#include "Component.h"
#include "Timer.h"
#include "Core/Types.h"

class HealthBar : public Component {
public:
    HealthBar();

    void update(const eng::Time& deltaTime) override;

#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

    void takeDamage(int damage);

    [[nodiscard]] int getHealth() const {
        return this->health;
    }

    void setHealth(const int _health) {
        this->health = _health;
    }

    // 服务端权威血量同步：归零时一并置死亡标志（用于服务端判死、客户端尚未预测到的兜底）
    void syncHealth(const int _health) {
        this->health = _health;
        if (_health <= 0) this->dead = true;
    }

    [[nodiscard]] bool isDead() const;

private:
    bool dead = false;
    bool invulnerable = false;
    int health = 3;
    int max_health = 3;
    Timer invulnerable_timer;
};
