//
// Created by MINEC on 2026/6/2.
//

#include "MarioRunState.h"

#include "FrameManager.h"
#include "StateMachine.h"
#include "GameObject.h"
#include "Collision.h"
#include "BoxCollision.h"
#include "Core/Types.h"

MarioRunState::MarioRunState() : BaseState("MarioRunState") {
#ifndef SERVER_BUILD
    animation_right.setFrames(FrameManager::getInstance().getFrame("right_small_normal"));
    animation_left.setFrames(FrameManager::getInstance().getFrame("left_small_normal"));
#endif
}

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
        box_collision->setOffset(eng::Vec2f(12.f, 0.f));
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
    if (owner) this->getAnimation().render(renderer, owner->getPosition());
    else LOG_ERROR("owner is nullptr");
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


