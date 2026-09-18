//
// Created by MINEC on 2026/6/2.
//

#include "MarioRunState.h"

#include "FrameManager.h"
#include "StateMachine.h"
#include "GameObject.h"
#include "Mario.h"
#include "Collision.h"
#include "BoxCollision.h"
#include "Core/Types.h"
#include <cmath>

MarioRunState::MarioRunState() : BaseState("MarioRunState") {
#ifndef SERVER_BUILD
    animation_right.setFrames(FrameManager::getInstance().getFrame("right_small_normal"));
    animation_left.setFrames(FrameManager::getInstance().getFrame("left_small_normal"));
#endif
}

void MarioRunState::start() {
#ifndef SERVER_BUILD
    // 每次进入跑步状态都按当前形态重指帧组（吃蘑菇长大后切到大马里奥帧）
    applyForm(isBigForm());
    applied_form = static_cast<int>(isBigForm());
#endif
}

bool MarioRunState::isBigForm() const {
    const auto* mario = dynamic_cast<const Mario*>(owner);
    return mario && mario->getIsBig();
}

#ifndef SERVER_BUILD
void MarioRunState::applyForm(const bool big) {
    auto& frame_manager = FrameManager::getInstance();
    if (big) {
        animation_right.setFrames(frame_manager.getFrame("right_big_normal"));
        animation_left.setFrames(frame_manager.getFrame("left_big_normal"));
    } else {
        animation_right.setFrames(frame_manager.getFrame("right_small_normal"));
        animation_left.setFrames(frame_manager.getFrame("left_small_normal"));
    }
}
#endif

void MarioRunState::update(const eng::Time& deltaTime) {
    // 防止错误更新
    if (owner->getComponent<StateMachine>()->getCurrentStateName() != this->getName()) return;
    if (owner->getSpeed().x == 0.f) {
        owner->getComponent<StateMachine>()->setState("MarioIdleState");
        return;
    }

    const float speedX = owner->getSpeed().x;
    if (speedX > 0.f) {
        setIsLeft(false);
    } else if (speedX < 0.f) {
        setIsLeft(true);
    }
#ifndef SERVER_BUILD
    this->getAnimation().update(deltaTime);
    // 变身闪烁（原版变身过程）：按相位在大小形态帧组间交替；
    // 非变身期回退到实际形态（闪烁可能在另一形态的相位中结束）。
    if (const auto* mario = dynamic_cast<const Mario*>(owner)) {
        const bool show_big = mario->isTransforming()
            ? isBigForm() != (mario->getTransformPhase() % 2 == 1)
            : isBigForm();
        if (applied_form != static_cast<int>(show_big)) {
            applyForm(show_big);
            applied_form = static_cast<int>(show_big);
        }
    }
#endif
    // sf::Sprite* sprite;
    // if (getIsLeft()) {
    //     sprite = &animation_left.getSprite();
    // } else {
    //     sprite = &animation_right.getSprite();
    // }
    // const auto box_collision = owner->getComponent<Collision, BoxCollision>();
    // const float w = sprite->getGlobalBounds().width;
    // const float h = sprite->getGlobalBounds().height;
    // box_collision->setSize(w, h);
    // owner->setSize(w, h);

    const auto box_collision = owner->getComponent<Collision, BoxCollision>();
    if (!getIsLeft()) {
        // 大马里奥碰撞盒与精灵同宽，不再向右偏移
        box_collision->setOffset(eng::Vec2f(isBigForm() ? 0.f : 12.f, 0.f));
    } else {
        box_collision->setOffset(eng::Vec2f(0.f, 0.f));
    }
}

void MarioRunState::handleEvent(const eng::EngineEvent& event) {
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::A) {
            setIsLeft(true);
        } else if (event.key == eng::Key::D) {
            setIsLeft(false);
        }
    }
}

#ifndef SERVER_BUILD
void MarioRunState::render(eng::Renderer& renderer) {
    if (!owner) {
        LOG_ERROR("owner is nullptr");
        return;
    }
    eng::Vec2f draw_pos = owner->getPosition();
    // 变身闪烁显示另一形态时，精灵锚定脚底、水平居中
    //（帧默认从盒子左上角向下画，直接画会伸到脚底之下）。
    if (const auto* mario = dynamic_cast<const Mario*>(owner); mario && mario->isTransforming()) {
        const auto& frame = this->getAnimation().getFrame();
        const float shown_w = std::abs(frame.scale.x) * static_cast<float>(frame.textureRect.width);
        const float shown_h = std::abs(frame.scale.y) * static_cast<float>(frame.textureRect.height);
        draw_pos.x += (owner->getSize().x - shown_w) * 0.5f;
        draw_pos.y += owner->getSize().y - shown_h;
    }
    this->getAnimation().render(renderer, draw_pos);
}
#endif

bool MarioRunState::getIsLeft() const {
    return owner->getComponent<StateMachine>()->getIsLeft();
}

void MarioRunState::setIsLeft(const bool value) const {
    owner->getComponent<StateMachine>()->setIsLeft(value);
}

#ifndef SERVER_BUILD
Animation& MarioRunState::getAnimation() {
    if (getIsLeft()) return animation_left;
    return animation_right;
}
#endif


