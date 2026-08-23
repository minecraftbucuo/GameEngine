//
// Created by MINEC on 2026/6/2.
//

#include "MarioIdleState.h"

#ifndef SERVER_BUILD
#include "AssetManager.h"
#endif

#include "Collision.h"
#include "BoxCollision.h"
#include "GameObject.h"
#include "MarioJumpState.h"
#include "StateMachine.h"
#include "Core/Types.h"
#include <cmath>

MarioIdleState::MarioIdleState() : BaseState("MarioIdleState") {
#ifndef SERVER_BUILD
    // 原 sf::Sprite 配置：mario_bros 纹理 (178,32,12,16) 区域，4 倍放大，左右镜像
    texture = AssetManager::getInstance().getTextureHandle("mario_bros");
    texture_rect = eng::IntRect(178, 32, 12, 16);
#endif
}

void MarioIdleState::start() {
    const auto box_collision = owner->getComponent<Collision, BoxCollision>();
#ifndef SERVER_BUILD
    const float w = std::abs(scale.x) * static_cast<float>(texture_rect.width);
    const float h = std::abs(scale.y) * static_cast<float>(texture_rect.height);
    LOG_TRACE_FMT("MarioIdle sprite width:{}, height:{}", w, h);
#else
    const float w = 48.f;
    const float h = 64.f;
#endif
    box_collision->setSize(w, h);
    owner->setSize(w, h);
    box_collision->setOffset(eng::Vec2f(0.f, 0.f));
}

void MarioIdleState::update(const eng::Time& deltaTime) {
    if (owner->getSpeed().x != 0.f) {
        owner->getComponent<StateMachine>()->setState("MarioRunState");
    }
}

void MarioIdleState::handleEvent(const eng::EngineEvent& event) {
    // 防止错误更新
    if (owner->getComponent<StateMachine>()->getCurrentStateName() != this->getName()) return;
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::A) {
            setIsLeft(true);
        }
        else if (event.key == eng::Key::D) {
            setIsLeft(false);
        }
    }
}

#ifndef SERVER_BUILD
void MarioIdleState::render(eng::Renderer& renderer) {
    if (!owner) {
        LOG_ERROR("Owner is nullptr");
        return;
    }
    const eng::Vec2f size(std::abs(scale.x) * static_cast<float>(texture_rect.width),
                          std::abs(scale.y) * static_cast<float>(texture_rect.height));
    renderer.drawTexture(texture,
                         eng::FloatRect(static_cast<float>(texture_rect.left),
                                        static_cast<float>(texture_rect.top),
                                        static_cast<float>(texture_rect.width),
                                        static_cast<float>(texture_rect.height)),
                         eng::FloatRect(owner->getPosition(), size),
                         0.f, {}, eng::Color::White, getIsLeft());
}
#endif

bool MarioIdleState::getIsLeft() const {
    return owner->getComponent<StateMachine>()->getIsLeft();
}

void MarioIdleState::setIsLeft(const bool value) const {
    owner->getComponent<StateMachine>()->setIsLeft(value);
}
