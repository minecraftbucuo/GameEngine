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

MarioIdleState::MarioIdleState() : BaseState("MarioIdleState") {
#ifndef SERVER_BUILD
    const sf::Texture& mario_texture = AssetManager::getInstance().getTexture("mario_bros");
    right_sprite.setTexture(mario_texture);
    right_sprite.setTextureRect(eng::IntRect(178, 32, 12, 16));
    right_sprite.setScale(4.f, 4.f);
    left_sprite.setTexture(mario_texture);
    left_sprite.setTextureRect(eng::IntRect(178, 32, 12, 16));
    left_sprite.setScale(-4.f, 4.f);
    left_sprite.setOrigin(static_cast<float>(right_sprite.getTextureRect().width), 0.f);
#endif
}

void MarioIdleState::start() {
    const auto box_collision = owner->getComponent<Collision, BoxCollision>();
#ifndef SERVER_BUILD
    const float w = left_sprite.getGlobalBounds().width;
    const float h = left_sprite.getGlobalBounds().height;
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
void MarioIdleState::render(sf::RenderWindow* window) {
    if (getIsLeft()) {
        if (owner) left_sprite.setPosition(owner->getPosition());
        else
            LOG_ERROR("Owner is nullptr");
        window->draw(left_sprite);
    }
    else {
        if (owner) right_sprite.setPosition(owner->getPosition());
        else
            LOG_ERROR("Owner is nullptr");
        window->draw(right_sprite);
    }
}
#endif

bool MarioIdleState::getIsLeft() const {
    return owner->getComponent<StateMachine>()->getIsLeft();
}

void MarioIdleState::setIsLeft(const bool value) const {
    owner->getComponent<StateMachine>()->setIsLeft(value);
}
