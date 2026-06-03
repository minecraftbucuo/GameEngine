//
// Created by MINEC on 2026/5/8.
//

#include "Mario.h"
#include "MarioRunState.h"
#include "StateMachine.h"
#include "SceneContext.h"
#include "SceneManager.h"
#include "MarioCameraComponent.h"
#include "MarioController.h"
#include "MarioIdleState.h"
#include "CollisionSystem.h"
#include "HealthBar.h"
#include "MarioDeadState.h"
#include "Collision.h"
#include "GravityComponent.h"
#include "BoxCollision.h"
#include "AssetManager.h"
#include "MarioJumpState.h"
#include "EventBus.h"
#include "FireBall.h"


Mario::Mario(const float x, const float y, const bool isPlayer) {
    this->position = sf::Vector2f(x, y);
    this->isPlayer = isPlayer;
    const auto marioController = this->addComponent<MarioController, true>();
    if (!isPlayer) marioController->setIsPlayer(false);

    this->addComponent<Collision, BoxCollision, true>();
    // this->addComponent<CollisionHandle, BoxCollisionHandle>();
    this->addComponent<GravityComponent>();
    this->addComponent<HealthBar>();
#ifndef SERVER_BUILD
    if (isPlayer) this->addComponent<MarioCameraComponent>();
#endif
    const auto stateMachine = this->addComponent<StateMachine>();
    stateMachine->addState<MarioRunState>();
    stateMachine->addState<MarioIdleState>();
    stateMachine->addState<MarioJumpState>();
    stateMachine->addState<MarioDeadState>();
    stateMachine->setState("MarioIdleState");

    this->addComponent<MoveComponent>();

    this->tag = "mario:" + std::to_string(this->id);
    className = "Mario";
}

Mario::~Mario() {
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
    LOG_DEBUG_FMT("The object tagged {} is destroyed", this->getTag());
}

void Mario::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) -> void {
            handleCollision(collisionEvent);
        }
    );
}

void Mario::handleEvent(sf::Event& e) {
    if (isPlayer) GameObject::handleEvent(e);
}

void Mario::update(sf::Time deltaTime) {
    if (needGravity()) {
        this->getComponent<GravityComponent>()->setActive(true);
        if (this->getComponent<StateMachine>()->getCurrentStateName() != "MarioJumpState")
            this->getComponent<StateMachine>()->setState("MarioJumpState");
    }
    GameObject::update(deltaTime);
    if (this->getPosition().y > static_cast<float>(SceneContext::getInstance().getWindowHeight())) {
        if (this->getComponent<HealthBar>()->isDead()) return;
        this->getComponent<MoveComponent>()->setPositionY(-this->getSize().y);
    }
}

bool Mario::needGravity() {
    if (this->getComponent<HealthBar>()->isDead()) return false;
    auto collision = this->getComponent<Collision>();
    sf::Vector2f dy = sf::Vector2f(0.f, 1.f);
    collision->setCollisionPosition(collision->getCollisionPosition() + dy);

    const auto game_objects = *SceneContext::getInstance().
                               getSceneManager()->getCurrentScene()->getCollisionSystem()->getObjects();

    for (auto& game_object : game_objects) {
        if (game_object->getTag() == this->getTag()) continue;
        auto other_collision = game_object->getComponent<Collision>();
        if (!other_collision) continue;
        if (other_collision->checkCollision(*collision)) {
            collision->setCollisionPosition(collision->getCollisionPosition() - dy);
            return false;
        }
    }
    collision->setCollisionPosition(collision->getCollisionPosition() - dy);
    return true;
}

void Mario::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    // std::cout << this_->getTag() << ' ' << other->getTag() << std::endl;

    if (!this_->getMoveAble()) return;

    if (other->getClassName() == "FireBall") {
        if (const auto fireball = std::dynamic_pointer_cast<FireBall>(other);
            fireball && fireball->getOwnerId() != this_->getId()) {
            const auto& health_bar = getComponent<HealthBar>();
            health_bar->takeDamage(1);
            if (health_bar->isDead()) {
                this->getComponent<StateMachine>()->setState("MarioDeadState");
                return;
            }
        }
    }

    std::shared_ptr<MoveComponent> moveComponent = this_->getComponent<MoveComponent>();
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
        // const float relativeSpeedX = event.b_speed.x - event.a_speed.x;
        // moveComponent->setSpeedX(relativeSpeedX * 0.28f);
        // if (std::abs(this_->getSpeed().x) <= 2.f) {
        //     moveComponent->setSpeedX(0.f);
        // }
        float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - this_->getSize().x);
        }
        else {
            moveComponent->moveCollisionXTo(event.b_position.x + other->getSize().x);
        }
    }
    else {
        if (this_->getSpeed().y < 0 && dx - dy < 10.f) return;
        const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
        moveComponent->setSpeedY(relativeSpeedY * 0.28f);
        if (std::abs(this_->getSpeed().y) <= 2.f) {
            moveComponent->setSpeedY(0.f);
        }
        float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
        float bottom_y = std::abs(
            event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            moveComponent->moveCollisionYTo(event.b_position.y - this_->getSize().y);
            moveComponent->setSpeedY(0.f);
            this->getComponent<StateMachine>()->setState("MarioIdleState");
            this->getComponent<GravityComponent>()->setActive(false);
        }
        else {
            moveComponent->moveCollisionYTo(event.b_position.y + other->getSize().y);
        }
    }
}

bool Mario::getIsPlayer() const {
    return isPlayer;
}

void Mario::destroy() {
    NetworkGameObject::destroy();
    if (this->isPlayer) {
        EventBus::getInstance().publish("PlayerDied", true);
    }
}

sf::Vector2f Mario::getCenter() {
    return this->position + getComponent<Collision>()->getOffset() + this->size * 0.5f;
}

void Mario::serialize(sf::Packet& packet, const NetworkMsg type) {
    if (type == NetworkMsg::SpawnObject) {  // 交给Scene处理
        packet << type << this->getId();   // 给 NetworkManager 判断是哪种操作和定位对象的 ID

        // 通知客户端新建对象   ID   对象类型   x   y   s_x   s_y   is_jump   health
        packet << ObjectType::Mario;
        const bool is_jump = this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
        const int health = this->getComponent<HealthBar>()->getHealth();
        packet << this->getPosition().x << this->getPosition().y << this->getSpeed().x << this->getSpeed().y <<
            is_jump << health;
    } else if (type == NetworkMsg::UpdateObject) {  // 交给自己处理
        packet << type << this->getId();   // 给 NetworkManager 判断是哪种操作和定位对象的 ID

        // 通知客户端同步对象   ID   同步对象(这是在deserialize用来判断的)   对象类型   x   y   s_x   s_y   is_jump
        const bool is_jump = this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
        packet << type << this->getPosition().x << this->getPosition().y << this->getSpeed().x << this->getSpeed().y <<
            is_jump;
    } else if (type == NetworkMsg::SpawnPlayer) {  // 交给Scene处理
        packet << type << this->getId();   // 给 NetworkManager 判断是哪种操作和定位对象的 ID

        // 通知客户端创建玩家   ID   对象类型   x   y   s_x   s_y   is_jump   health
        packet << ObjectType::MarioPlayer;
        const bool is_jump = this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
        const int health = this->getComponent<HealthBar>()->getHealth();
        packet << this->getPosition().x << this->getPosition().y << this->getSpeed().x << this->getSpeed().y <<
            is_jump << health;
    }
    // else if (type == NetworkMsg::RemoveObject) {  // 交给外部处理
    //     通知客户端删除对象   ID
    // }
}

void Mario::deserialize(sf::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;

    if (msg_type == NetworkMsg::UpdateObject) {
        float x, y, s_x, s_y;
        bool is_jump;
        packet >> x >> y >> s_x >> s_y >> is_jump;
        const auto& move_component = this->getComponent<MoveComponent>();
        move_component->setPosition(x, y);
        move_component->setSpeed(s_x, s_y);
        if (is_jump) this->getComponent<StateMachine>()->setState("MarioJumpState");
    } else if (msg_type == NetworkMsg::ClientInput) {
        const auto& marioController = this->getComponent<MarioController>();
        InputType type;
        packet >> type;
        if (type == InputType::Jump) {
            marioController->jump(false);
        } else if (type == InputType::RunLeft) {
            marioController->runLeft();
        } else if (type == InputType::RunRight) {
            marioController->runRight();
        } else if (type == InputType::StopRun) {
            marioController->stopRun();
        } else if (type == InputType::JumpRelease) {
            // 处理长安跳跃键后松开的逻辑
            if (this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState") {
                marioController->setWisPressed(false);
            }
        } else if (type == InputType::Shoot) {
            marioController->shoot(false);
        }
    }
}
