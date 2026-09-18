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
#include "Mario.h"
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
    // 按当前形态选择贴图区域：小马里奥站立 (178,32,12,16)，大马里奥站立 (176,0,16,32)
    if (const auto mario = dynamic_cast<Mario*>(owner); mario && mario->getIsBig()) {
        texture_rect = eng::IntRect(176, 0, 16, 32);
    } else {
        texture_rect = eng::IntRect(178, 32, 12, 16);
    }
    const float w = std::abs(scale.x) * static_cast<float>(texture_rect.width);
    const float h = std::abs(scale.y) * static_cast<float>(texture_rect.height);
    LOG_TRACE_FMT("MarioIdle sprite width:{}, height:{}", w, h);
#else
    float w = 48.f;
    float h = 64.f;
    if (const auto mario = dynamic_cast<Mario*>(owner); mario && mario->getIsBig()) {
        w = 64.f;
        h = 128.f;
    }
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
    // 变身闪烁（原版变身过程）：按相位在大小形态间交替展示
    bool show_big = false;
    if (const auto mario = dynamic_cast<Mario*>(owner)) {
        show_big = mario->getIsBig();
        if (mario->isTransforming())
            show_big = show_big != (mario->getTransformPhase() % 2 == 1);
    }
    const eng::IntRect rect = show_big ? eng::IntRect(176, 0, 16, 32) : eng::IntRect(178, 32, 12, 16);
    const eng::Vec2f size(std::abs(scale.x) * static_cast<float>(rect.width),
                          std::abs(scale.y) * static_cast<float>(rect.height));
    // 统一锚定脚底、水平居中：正常形态与碰撞盒重合（偏移为 0），
    // 闪烁显示另一形态时精灵贴着脚底缩放，不会陷进地面。
    const eng::Vec2f pos(owner->getPosition().x + (owner->getSize().x - size.x) * 0.5f,
                         owner->getPosition().y + owner->getSize().y - size.y);
    renderer.drawTexture(texture,
                         eng::FloatRect(static_cast<float>(rect.left),
                                        static_cast<float>(rect.top),
                                        static_cast<float>(rect.width),
                                        static_cast<float>(rect.height)),
                         eng::FloatRect(pos, size),
                         0.f, {}, eng::Color::White, getIsLeft());
}
#endif

bool MarioIdleState::getIsLeft() const {
    return owner->getComponent<StateMachine>()->getIsLeft();
}

void MarioIdleState::setIsLeft(const bool value) const {
    owner->getComponent<StateMachine>()->setIsLeft(value);
}
