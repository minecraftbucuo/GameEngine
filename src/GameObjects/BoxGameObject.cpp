//
// Created by MINEC on 2026/6/2.
//

#include "BoxGameObject.h"
#include "Collision.h"
#include "CollisionHandle.h"
#include "BoxCollisionHandle.h"
#include "BoxCollision.h"

BoxGameObject::BoxGameObject() {
    className = "BoxGameObject";
}

BoxGameObject::BoxGameObject(const float posX, const float posY, const float width, const float height,
    const std::string& tag) : GameObject(posX, posY, width, height) {
    this->tag = tag + ":" + std::to_string(id);
    this->addComponent<Collision, BoxCollision>();
    this->addComponent<CollisionHandle, BoxCollisionHandle>();
    className = "BoxGameObject";
}

BoxGameObject::~BoxGameObject() {
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void BoxGameObject::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
        [this](const CollisionEvent& event) {
            if (const auto &handler = this->getComponent<CollisionHandle>()) {
                handler->handleCollision(event);
            }
        }
    );
}
