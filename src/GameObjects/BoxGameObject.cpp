//
// Created by MINEC on 2026/6/2.
//

#include "BoxGameObject.h"
#include "Collision.h"
#include "BoxCollision.h"
#include "Events.h"

BoxGameObject::BoxGameObject() {
    className = "BoxGameObject";
}

BoxGameObject::BoxGameObject(const float posX, const float posY, const float width, const float height,
    const std::string& tag) : GameObject(posX, posY, width, height) {
    this->tag = tag + ":" + std::to_string(id);
    // 物理响应由 CollisionSystem 求解器统一处理，不再添加 CollisionHandle 组件（B1/B6 清理）
    this->addComponent<Collision, BoxCollision>();
    className = "BoxGameObject";
}

BoxGameObject::~BoxGameObject() {
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void BoxGameObject::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
        [this](const CollisionEvent&) {
            // B1：物理响应已由 CollisionSystem 的迭代求解器统一处理，事件仅保留给游戏逻辑。
        }
    );
}
