//
// Created by MINEC on 2026/5/8.
//

#include "Mario.h"

#include <algorithm>

#include "MarioRunState.h"
#include "StateMachine.h"
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
#include "Core/Types.h"

namespace {
    // 本地玩家已经在客户端预测移动，服务端快照的小误差不要硬拉回去，否则会抖动。
    constexpr float LOCAL_POSITION_TOLERANCE = 6.f;
    // 超过这个距离说明客户端和服务端已经明显不同步，需要直接使用服务端权威位置。
    constexpr float LOCAL_POSITION_SNAP_DISTANCE = 120.f;
    // 中等误差每次只修正一部分，让校正过程看起来更平滑。
    constexpr float LOCAL_CORRECTION_RATIO = 0.18f;

    // 这里只比较距离大小，不需要开方，避免每个网络同步包都做 sqrt。
    float lengthSquared(const eng::Vec2f& value) {
        return value.x * value.x + value.y * value.y;
    }

    float clamp01(const float value) {
        return std::max(0.f, std::min(1.f, value));
    }
}

Mario::Mario(const float x, const float y, const bool isPlayer) {
    this->position = eng::Vec2f(x, y);
    this->isPlayer = isPlayer;
    const auto marioController = this->addComponent<MarioController>();
    if (!isPlayer) marioController->setIsPlayer(false);

    this->addComponent<Collision, BoxCollision>();
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

void Mario::update(eng::Time deltaTime) {
    if (needGravity()) {
        this->getComponent<GravityComponent>()->setActive(true);
        if (this->getComponent<StateMachine>()->getCurrentStateName() != "MarioJumpState")
            this->getComponent<StateMachine>()->setState("MarioJumpState");
    } else {
        if (this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState"
            && this->getSpeed().y == 0.f) {
            this->getComponent<StateMachine>()->setState("MarioIdleState");
        }
    }
    GameObject::update(deltaTime);
    if (this->getPosition().y > static_cast<float>(getScene()->getWindowSize().y)) {
        if (this->getComponent<HealthBar>()->isDead()) return;
        this->getComponent<MoveComponent>()->setPositionY(-this->getSize().y);
    }
}

bool Mario::needGravity() {
    if (this->getComponent<HealthBar>()->isDead()) return false;
    auto collision = this->getComponent<Collision>();
    eng::Vec2f dy = eng::Vec2f(0.f, 1.f);
    collision->setCollisionPosition(collision->getCollisionPosition() + dy);

    const auto game_objects = *getScene()->getCollisionSystem()->getObjects();

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
            if (std::abs(this_->getSpeed().x) > 0.f) {
                this->getComponent<StateMachine>()->setState("MarioRunState");
            } else {
                this->getComponent<StateMachine>()->setState("MarioIdleState");
            }
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

eng::Vec2f Mario::getCenter() {
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
        const eng::Vec2f serverPosition{x, y};
        const eng::Vec2f serverSpeed{s_x, s_y};
        const auto* nm = getScene()->getNetworkManager();
        if (isPlayer && nm && nm->isClient()) {
            // 本地玩家：保留客户端预测手感，只用服务端状态做温和纠偏。
            reconcileLocalPlayer(serverPosition, serverSpeed, is_jump);
        } else if (!isPlayer && nm && nm->isClient()) {
            setAuthoritativeState(serverPosition, serverSpeed, is_jump);
        }
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

// 只用于客户端自己的玩家。
// 本地玩家已经根据输入预测移动，这里只负责用服务端快照纠偏，避免旧快照反复拉扯角色。
void Mario::reconcileLocalPlayer(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, const bool isJump) {
    const auto& move_component = this->getComponent<MoveComponent>();
    if (!move_component) return;

    const eng::Vec2f delta = serverPosition - this->getPosition();
    const float distance_squared = lengthSquared(delta);
    // 大偏差通常来自碰撞分歧、掉线重连或长时间丢包，此时平滑会显得拖泥带水，直接校正。
    if (distance_squared >= LOCAL_POSITION_SNAP_DISTANCE * LOCAL_POSITION_SNAP_DISTANCE) {
        setAuthoritativeState(serverPosition, serverSpeed, isJump);
        return;
    }

    // 小误差直接忽略，中等误差慢慢补回来，避免服务端旧快照和本地预测来回抢位置。
    if (distance_squared > LOCAL_POSITION_TOLERANCE * LOCAL_POSITION_TOLERANCE) {
        move_component->addPosition(delta * LOCAL_CORRECTION_RATIO);
    }

    if (isJump) this->getComponent<StateMachine>()->setState("MarioJumpState");
}

// 无平滑地应用服务端权威位置和速度。
// 本地的远端玩家直接同步服务端状态。
void Mario::setAuthoritativeState(const eng::Vec2f& serverPosition, const eng::Vec2f& serverSpeed, const bool isJump) {
    const auto& move_component = this->getComponent<MoveComponent>();
    if (!move_component) return;

    // 权威同步路径：用于服务端对象或客户端严重偏离服务端时。
    move_component->setPosition(serverPosition);
    // 客户端玩家正在跳跃时，服务器可能还没处理跳跃，不应覆盖本地 speed.y
    const auto* nm = getScene()->getNetworkManager();
    if (isPlayer && nm && nm->isClient()
        && this->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState") {
        move_component->setSpeed(serverSpeed.x, this->getSpeed().y);
    } else {
        move_component->setSpeed(serverSpeed);
    }
    if (isJump) this->getComponent<StateMachine>()->setState("MarioJumpState");
}