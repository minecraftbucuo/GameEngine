//
// Created by MINEC on 2026/5/8.
//

#include "FireBall.h"

#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"

FireBall::FireBall(const unsigned int owner_id, const float x, const float y, const float speed_x) {
    this->owner_id = owner_id;
    this->position = sf::Vector2f(x, y);

#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame("fireball_frame"));
    explosionAnimation.setFrames(FrameManager::getInstance().getFrame("explosion_frame"));
    this->setSize(animation.getFrameWidth(), animation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize / 2, CONFIG.game.defaultBlockSize / 2);
#endif

    this->addComponent<Collision, BoxCollision, true>();
    this->addComponent<GravityComponent>();

    this->addComponent<MoveComponent>()->setSpeed(sf::Vector2f(speed_x, CONFIG.game.fireballSpeedY));

    // 设置为 10s 自动爆炸
    ttl_timer.start(CONFIG.game.fireBallTTL);
    ttl_timer.setCallback([this]() -> void { this->setExploded(); });

    this->tag = "fireball:" + std::to_string(this->id);
    className = "FireBall";
}

FireBall::~FireBall() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void FireBall::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );
}

#ifndef SERVER_BUILD
void FireBall::render(sf::RenderWindow* window) {
    GameObject::render(window);
    if (is_exploded) explosionAnimation.render(window, this->position);
    else animation.render(window, this->position);
}
#endif

#ifndef SERVER_BUILD
void FireBall::update(sf::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_exploded) {
        explosionAnimation.update(deltaTime);
        if (explosionAnimation.isOver()) {
            destroy();
        }
    }
    else {
        animation.update(deltaTime);
    }
    ttl_timer.update(deltaTime);
}
#else
void FireBall::update(sf::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_exploded) {
        destroy();
    }
    ttl_timer.update(deltaTime);
}
#endif

void FireBall::setExploded() {
    is_exploded = true;
    this->getComponent<GravityComponent>()->setActive(false);
    this->getComponent<MoveComponent>()->setActive(false);
    const float offset = CONFIG.game.defaultBlockSize / 4;
    this->getComponent<MoveComponent>()->addPosition(sf::Vector2f(-offset, -offset), false);
}

void FireBall::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    if (owner_id == other->getId()) return;

    // std::cout << this_->getTag() << ' ' << other->getTag() << std::endl;

    if (!this_->getMoveAble()) return;
    const std::shared_ptr<MoveComponent>& moveComponent = this_->getComponent<MoveComponent>();
    if (!moveComponent) return;

    // 计算 x 方向和 y 方向的重合度
    const float dx = std::min(event.a_position.x + this_->getSize().x,
                              event.b_position.x + other->getSize().x) - std::max(
        event.a_position.x, event.b_position.x);
    const float dy = std::min(event.a_position.y + this_->getSize().y,
                              event.b_position.y + other->getSize().y) - std::max(
        event.a_position.y, event.b_position.y);

    // 水平碰撞
    if (dx <= dy) {
        float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - this_->getSize().x);
        }
        else {
            moveComponent->moveCollisionXTo(event.b_position.x + other->getSize().x);
        }

        // 设置为爆炸
        setExploded();
    }
    else {
        // 处理垂直方向碰撞：当火球向上运动且重叠区域差异较小时忽略碰撞
        if (this_->getSpeed().y < 0 && dx - dy < 10.f) return;

        const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
        moveComponent->setSpeedY(relativeSpeedY * 0.5f);
        if (relativeSpeedY < 0) {
            moveComponent->setSpeedY(std::min(relativeSpeedY * 0.5f, CONFIG.game.fireballSpeedY));
        }

        // 计算火球顶部和底部与碰撞物体的距离，判断碰撞面并调整位置
        float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
        float bottom_y = std::abs(
            event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            moveComponent->moveCollisionYTo(event.b_position.y - this_->getSize().y);
        }
        else {
            moveComponent->moveCollisionYTo(event.b_position.y + other->getSize().y);
        }
    }
}

void FireBall::serialize(sf::Packet& packet, const NetworkMsg type) {
    // NetworkManager 检测到火球的产生后自动调用通知所有的客户端
    if (type == NetworkMsg::SpawnFireBall || type == NetworkMsg::SpawnObject) {
        // 交给 Scene 处理
        packet << NetworkMsg::SpawnFireBall;
        packet << this->id << ObjectType::FireBall << this->owner_id << this->position.x <<
            this->position.y << this->getSpeed().x << this->getSpeed().y;
    }
}

void FireBall::deserialize(sf::Packet& packet) {
}

unsigned int FireBall::getOwnerId() const {
    return this->owner_id;
}
