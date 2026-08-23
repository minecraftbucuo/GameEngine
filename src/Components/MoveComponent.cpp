//
// Created by MINEC on 2026/5/8.
//

#include "MoveComponent.h"

#include "GameObject.h"
#include "Collision.h"
#include <cmath>
#include "ConfigManager.h"
#include "Core/Types.h"

void MoveComponent::update(const eng::Time& deltaTime) {
    owner->position += owner->speed * deltaTime.asSeconds();
    setPosition(owner->position.x, owner->position.y);
}
#ifndef SERVER_BUILD
void MoveComponent::render(sf::RenderWindow* window) {
    if (!CONFIG.game.debug) return;
    if (owner->speed.x == 0.f && owner->speed.y == 0.f) return;
    const auto center = owner->getCenter();
    drawArrow(window, center.x, center.y, center.x + owner->speed.x / 10.f, center.y + owner->speed.y / 10.f);
}
#endif
void MoveComponent::setPosition(const eng::Vec2f& pos, const bool move_collision) const {
    owner->setPosition(pos.x, pos.y);
    if (move_collision) setCollisionPosition(pos);
}

void MoveComponent::moveCollisionTo(const eng::Vec2f& pos) const {
    if (const auto& collision = owner->getComponent<Collision>()) {
        const auto delta_pos = pos - collision->getCollisionPosition();
        collision->setCollisionPosition(pos);
        this->addPosition(delta_pos, false);
    }
}

void MoveComponent::moveCollisionXTo(const float posX) const {
    const auto& collision = owner->getComponent<Collision>();
    moveCollisionTo(eng::Vec2f(posX, collision->getCollisionPosition().y));
}

void MoveComponent::moveCollisionYTo(const float posY) const {
    const auto& collision = owner->getComponent<Collision>();
    moveCollisionTo(eng::Vec2f(collision->getCollisionPosition().x, posY));
}

void MoveComponent::setPosition(const float posX, const float posY, const bool move_collision) const {
    setPosition(eng::Vec2f(posX, posY), move_collision);
}

void MoveComponent::setPositionX(const float posX, const bool move_collision) const {
    setPosition(eng::Vec2f(posX, owner->position.y), move_collision);
}

void MoveComponent::setPositionY(const float posY, const bool move_collision) const {
    setPosition(eng::Vec2f(owner->position.x, posY), move_collision);
}

void MoveComponent::addPosition(const eng::Vec2f& pos, const bool move_collision) const {
    setPosition(owner->position + pos, move_collision);
}

void MoveComponent::setSpeed(const eng::Vec2f& speed) const {
    owner->speed = speed;
}

void MoveComponent::setSpeedX(const float speedX) const {
    owner->speed.x = speedX;
}

void MoveComponent::setSpeedY(const float speedY) const {
    owner->speed.y = speedY;
}

void MoveComponent::setSpeed(const float speedX, const float speedY) const {
    owner->speed = eng::Vec2f(speedX, speedY);
}

void MoveComponent::addSpeed(const eng::Vec2f& speed) const {
    owner->speed += speed;
}
#ifndef SERVER_BUILD
void MoveComponent::drawArrow(sf::RenderWindow* window, const float x1, const float y1, const float x2, const float y2,
                              const float arrowSize, const eng::Color color) {
    // 绘制箭杆
    const sf::Vertex line[] = {
        sf::Vertex(eng::Vec2f(x1, y1), color),
        sf::Vertex(eng::Vec2f(x2, y2), color)
    };
    window->draw(line, 2, sf::Lines);

    // 计算箭头方向角度
    const float angle = std::atan2(y2 - y1, x2 - x1);

    // 计算箭头头部的两个翼点
    constexpr auto pi_6 = static_cast<float>(M_PI / 6);
    const float x3 = x2 - arrowSize * cosf(angle + pi_6);
    const float y3 = y2 - arrowSize * sinf(angle + pi_6);
    const float x4 = x2 - arrowSize * cosf(angle - pi_6);
    const float y4 = y2 - arrowSize * sinf(angle - pi_6);

    // 绘制箭头头部（三角形）
    const sf::Vertex arrowHead[] = {
        sf::Vertex(eng::Vec2f(x2, y2), color),
        sf::Vertex(eng::Vec2f(x3, y3), color),
        sf::Vertex(eng::Vec2f(x4, y4), color)
    };
    window->draw(arrowHead, 3, sf::Triangles);
}
#endif
void MoveComponent::setCollisionPosition(const eng::Vec2f& pos) const {
    if (const auto& collision = owner->getComponent<Collision>()) {
        collision->setPosition(pos);
    }
}
