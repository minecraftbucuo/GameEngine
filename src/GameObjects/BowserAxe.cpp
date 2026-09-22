//
// Created by MINEC on 2026/9/17.
//

#include "BowserAxe.h"

#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "NetworkManager.h"
#include "Scene.h"
#include "Core/Types.h"

namespace {
    // 飞斧存活上限（ms）
    constexpr int BOWSER_AXE_TTL = 6000;
}

BowserAxe::BowserAxe(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);

#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame("bowser_axe_frame"));
    this->setSize(animation.getFrameWidth(), animation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif

    this->addComponent<Collision, BoxCollision>();
    // 抛物线轨迹：水平直飞 + 先上抛，由重力拉出弧线（垂直初速由构造统一复现，
    // 无需随快照传输）
    this->addComponent<GravityComponent>();
    this->addComponent<MoveComponent>()->setSpeed(
        eng::Vec2f(speed_x, -CONFIG.game.jumpForce * 0.8f));

    ttl_timer.setCallback([this]() -> void { this->setDestroyed(); });
    // TTL 仅权威端启动（构造时 scene 未设置无法判定网络身份）；
    // 客户端生命周期完全由服务端 RemoveObject 驱动，不做本地 TTL 预判

    this->tag = "bowser_axe:" + std::to_string(this->id);
    className = "BowserAxe";
}

BowserAxe::~BowserAxe() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

bool BowserAxe::isAuthority() const {
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    return !nm || !nm->isClient();
}

void BowserAxe::start() {
    GameObject::start();

    if (isAuthority()) {
        ttl_timer.start(BOWSER_AXE_TTL);
    }

    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );
}

#ifndef SERVER_BUILD
void BowserAxe::render(eng::Renderer& renderer) {
    GameObject::render(renderer);
    animation.render(renderer, this->position);
}
#endif

void BowserAxe::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
#ifndef SERVER_BUILD
    animation.update(deltaTime);
#endif
    ttl_timer.update(deltaTime);
    // 兜底：飞出场景底部直接销毁
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
    }
}

void BowserAxe::handleCollision(const CollisionEvent& event) {
    auto& other = event.b;

    if (is_destroyed) return;
    // 命中马里奥：伤害与销毁由马里奥侧的 handleCollision 处理（客户端预测马里奥
    // 命中后本地静默销毁，与服务端随后的 RemoveObject 幂等）
    if (other->getClassName() == "Mario") return;
    // 对友军（BOSS 本体/火焰弹）与小怪穿行：BOSS 的武器不与自家阵营交互
    const std::string& name = other->getClassName();
    if (name == "Bowser" || name == "BowserFire" || name == "Goomba") return;
    // 无视地面和方块：飞斧按抛物线穿行所有地形（销毁只由 TTL 与飞出场景兜底）
    if (name == "Ground" || name == "Brick" || name == "Box") return;
}

void BowserAxe::setDestroyed() {
    if (is_destroyed) return;
    is_destroyed = true;
    destroy();
}

void BowserAxe::serialize(eng::Packet& packet, const NetworkMsg type) {
    if (type == NetworkMsg::SpawnObject) {   // 交给 Scene 处理
        // ID   对象类型   x   y   s_x
        packet << type << this->getId() << ObjectType::BowserAxe
            << this->getPosition().x << this->getPosition().y << this->getSpeed().x;
    } else if (type == NetworkMsg::UpdateObject) {   // 交给自己处理
        // 第二个 type 供 deserialize 判别（与 Mario/Goomba 的线上格式一致）；
        // 不传旋转角度——客户端本地动画自转，亚帧误差无感知意义
        packet << type << this->getId() << type
            << this->getPosition().x << this->getPosition().y << this->getSpeed().x;
    }
    // RemoveObject 由 destroy() 触发 broadcastRemoveObject 统一广播，不走 serialize
}

void BowserAxe::deserialize(eng::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;
    if (msg_type != NetworkMsg::UpdateObject) return;

    float x, y, s_x;
    packet >> x >> y >> s_x;

    // 已销毁的对象不再接受快照，等待 RemoveObject 清理
    if (is_destroyed) return;

    if (const auto move = getComponent<MoveComponent>()) {
        move->setPosition(eng::Vec2f(x, y));
        move->setSpeedX(s_x);
    }
}

void BowserAxe::destroy() {
    // 服务端是移除的唯一权威：销毁时广播 RemoveObject（超时/飞出场景等路径）；
    // 客户端本地销毁静默，由服务端消息兜底
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    if (nm && nm->isServer()) {
        nm->broadcastRemoveObject(this->getId());
    }
    NetworkGameObject::destroy();
}
