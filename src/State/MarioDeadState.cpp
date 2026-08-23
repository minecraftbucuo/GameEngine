//
// Created by MINEC on 2026/6/2.
//

#include "MarioDeadState.h"
#ifndef SERVER_BUILD
#include "AssetManager.h"
#endif
#include "Collision.h"
#include "GravityComponent.h"
#include "MoveComponent.h"
#include "GameObject.h"
#include "Core/Types.h"

MarioDeadState::MarioDeadState() : BaseState("MarioDeadState") {
#ifndef SERVER_BUILD
    // 原 sf::Sprite 配置：mario_bros (160,32,16,16) 区域，4 倍放大
    texture = AssetManager::getInstance().getTextureHandle("mario_bros");
    texture_rect = eng::IntRect(160, 32, 16, 16);
#endif
}

void MarioDeadState::start() {
    if (const auto collision = owner->getComponent<Collision>()) collision->setActive(false);
    if (const auto gravity = owner->getComponent<GravityComponent>()) gravity->setActive(true);
    if (const auto move = owner->getComponent<MoveComponent>()) {
        move->setSpeed(eng::Vec2f(0.f, -500.f));
    }
    deathTimer.setCallback(
        [this]() -> void {
            owner->destroy();
            LOG_DEBUG("MarioDeadState destroy");
        }
    );
    deathTimer.start(600);
    LOG_DEBUG("MarioDeadState start");
}

void MarioDeadState::update(const eng::Time& deltaTime) {
    deathTimer.update(deltaTime);
}

#ifndef SERVER_BUILD
void MarioDeadState::render(eng::Renderer& renderer) {
    if (owner) {
        const eng::Vec2f size(scale.x * static_cast<float>(texture_rect.width),
                              scale.y * static_cast<float>(texture_rect.height));
        renderer.drawTexture(texture,
                             eng::FloatRect(static_cast<float>(texture_rect.left),
                                            static_cast<float>(texture_rect.top),
                                            static_cast<float>(texture_rect.width),
                                            static_cast<float>(texture_rect.height)),
                             eng::FloatRect(owner->getPosition(), size));
    }
}
#endif
