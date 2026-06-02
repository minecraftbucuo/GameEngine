//
// Created by MINEC on 2026/5/8.
//
#include "Box.h"

#include "Collision.h"
#include "CollisionHandle.h"
#include "FrameManager.h"
#include "GravityComponent.h"
#include "MoveComponent.h"
#include "BoxCollision.h"

Box::Box(const float x, const float y, const std::string& tag) : BoxGameObject(x, y, 0, 0) {
    this->tag = tag + ":" + std::to_string(id);

    last_y = y;
#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame("box_frame"));
    animation.setBack(true);
#endif

#ifndef SERVER_BUILD
    const auto w = animation.getSprite().getGlobalBounds().width;
    const auto h = animation.getSprite().getGlobalBounds().height;
#else
    const auto w = CONFIG.game.defaultBlockSize;
    const auto h = CONFIG.game.defaultBlockSize;
#endif
    this->setSize(w, h);
    this->getComponent<Collision, BoxCollision>()->setSize(w, h);

    this->removeComponent<CollisionHandle>();

    this->addComponent<MoveComponent>();
    this->addComponent<GravityComponent>()->setActive(false);
    className = "Box";
}

Box::~Box() {
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void Box::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
        [this](const CollisionEvent& event) {
            const auto& this_pos = event.a_position;
            const auto& other_pos = event.b_position;
            if (other_pos.y < this_pos.y) return;

            // 水平重合量
            const auto w = std::min(this_pos.x + this->getSize().x, other_pos.x + event.b->getSize().x) -
                std::max(this_pos.x, other_pos.x);

            if (w < 0.4 * this->getSize().x) return;

            const auto& move_component = this->getComponent<MoveComponent>();
            move_component->setSpeedY(-300.f);
            this->getComponent<GravityComponent>()->setActive(true);
        }
    );
}

void Box::update(sf::Time deltaTime) {
    BoxGameObject::update(deltaTime);
#ifndef SERVER_BUILD
    animation.update(deltaTime);
#endif
    if (this->getPosition().y > last_y) {
        const auto& move_component = this->getComponent<MoveComponent>();
        move_component->setPositionY(last_y);
        move_component->setSpeedY(0.f);

        this->getComponent<GravityComponent>()->setActive(false);
    }
}

void Box::setPosition(const float posX, const float posY) {
    this->position = sf::Vector2f(posX, posY);
    const auto boxCollision = this->getComponent<Collision, BoxCollision>();
    boxCollision->setPosition(posX, posY);
}

#ifndef SERVER_BUILD
void Box::render(sf::RenderWindow* window) {
    animation.render(window, this->getPosition());
    BoxGameObject::render(window);
}
#endif
