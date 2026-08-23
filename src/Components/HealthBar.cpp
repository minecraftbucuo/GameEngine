//
// Created by MINEC on 2026/5/14.
//

#include "HealthBar.h"
#include "GameObject.h"
#include "Core/Types.h"
#include "Render/Renderer.h"

HealthBar::HealthBar() {
    invulnerable_timer.setCallback([this]() { invulnerable = false; });
}

void HealthBar::update(const eng::Time& deltaTime) {
    Component::update(deltaTime);
    invulnerable_timer.update(deltaTime);
}

void HealthBar::takeDamage(const int damage) {
    if (dead) return;
    if (invulnerable) return;
    health -= damage;
    if (health <= 0) {
        health = 0;
        dead = true;
        return;
    }
    invulnerable = true;
    invulnerable_timer.start(1000);
}

bool HealthBar::isDead() const {
    return dead;
}

#ifndef SERVER_BUILD
// SDL3 迁移 6e：sf::RectangleShape ×2 → drawRect（bg 深灰带 1px 黑描边，fg 按血量变色）
void HealthBar::render(eng::Renderer& renderer) {
    Component::render(renderer);
    if (dead) return;
    // TODO: 根据 owner 改变血条形状
    const float barWidth = 48.f;
    const float barHeight = 5.f;
    const float barX = owner->getCenter().x - barWidth / 2;
    const float barY = owner->getPosition().y - 10.f;

    renderer.drawRect(eng::FloatRect(barX, barY, barWidth, barHeight),
                      eng::Color(60, 60, 60), true, 1.f, eng::Color(0, 0, 0));

    const float healthRatio = static_cast<float>(health) / static_cast<float>(max_health);
    eng::Color fgColor;
    if (healthRatio > 0.5f) {
        fgColor = eng::Color::Green;
    } else if (healthRatio > 0.25f) {
        fgColor = eng::Color::Yellow;
    } else {
        fgColor = eng::Color::Red;
    }
    if (invulnerable) {
        fgColor = eng::Color(150, 150, 150);
    }
    renderer.drawRect(eng::FloatRect(barX, barY, barWidth * healthRatio, barHeight), fgColor);
}
#endif
