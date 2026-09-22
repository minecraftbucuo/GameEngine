//
// Created by MINEC on 2026/9/17.
//

#include "BowserFire.h"

#include "FrameManager.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "NetworkManager.h"
#include "Scene.h"
#include "Core/Types.h"

namespace {
    // 火焰弹存活上限（ms）：防止沿空旷地形一直飞
    constexpr int BOWSER_FIRE_TTL = 5000;
}

BowserFire::BowserFire(const float x, const float y, const float speed_x) {
#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame(
        speed_x < 0.f ? "bowser_fire_left_frame" : "bowser_fire_right_frame"));
    this->setSize(animation.getFrameWidth(), animation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize * 1.5f, CONFIG.game.defaultBlockSize * 0.5f);
#endif
    // y 为火焰垂直中心点：上移半高，让火焰居中于调用点（嘴部高度）而非从该点向下展开。
    // 客户端经网络重建时由 spawnEntityWithNetwork 按快照位置重置，避免二次偏移
    this->position = eng::Vec2f(x, y - this->getSize().y * 0.5f);

    this->addComponent<Collision, BoxCollision>();
    // 直线飞行：不加重力组件
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, 0.f));

    ttl_timer.setCallback([this]() -> void { this->setExtinguished(); });
    // TTL 仅权威端启动（构造时 scene 未设置无法判定网络身份）；
    // 客户端生命周期完全由服务端 RemoveObject 驱动，不做本地 TTL 预判

    this->tag = "bowser_fire:" + std::to_string(this->id);
    className = "BowserFire";
}

BowserFire::~BowserFire() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

bool BowserFire::isAuthority() const {
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    return !nm || !nm->isClient();
}

void BowserFire::start() {
    GameObject::start();

    if (isAuthority()) {
        ttl_timer.start(BOWSER_FIRE_TTL);
    }

    // BoxCollision::start() 会用渲染贴图尺寸覆盖碰撞盒，必须在组件初始化后重新缩小：
    // 宽 75%、高 35% 的居中条带（火焰边缘渐隐部分不参与碰撞，也避免出生就蹭到地面）
    if (const auto box = getComponent<Collision, BoxCollision>()) {
        const float hit_w = this->getSize().x * 0.75f;
        const float hit_h = this->getSize().y * 0.35f;
        box->setSize(hit_w, hit_h);
        box->setOffset(eng::Vec2f((this->getSize().x - hit_w) * 0.5f,
                                  (this->getSize().y - hit_h) * 0.5f));
    }

    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );
}

#ifndef SERVER_BUILD
void BowserFire::render(eng::Renderer& renderer) {
    GameObject::render(renderer);
    animation.render(renderer, this->position);
}
#endif

void BowserFire::update(eng::Time deltaTime) {
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

void BowserFire::handleCollision(const CollisionEvent& event) {
    auto& other = event.b;

    if (is_extinguished) return;
    // 命中马里奥：伤害与熄灭由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;
    // 只被地形/可顶物（地面、砖块、箱子）阻挡熄灭，穿过其余对象（含友军）。
    // 客户端同样本地熄灭：火焰位置是快照硬同步的，几何判定与服务端一致
    const std::string& name = other->getClassName();
    if (name == "Ground" || name == "Brick" || name == "Box") {
        setExtinguished();
    }
}

void BowserFire::setExtinguished() {
    if (is_extinguished) return;
    is_extinguished = true;
    destroy();
}

void BowserFire::serialize(eng::Packet& packet, const NetworkMsg type) {
    if (type == NetworkMsg::SpawnObject) {   // 交给 Scene 处理
        // ID   对象类型   x   y   s_x
        packet << type << this->getId() << ObjectType::BowserFire
            << this->getPosition().x << this->getPosition().y << this->getSpeed().x;
    } else if (type == NetworkMsg::UpdateObject) {   // 交给自己处理
        // 第二个 type 供 deserialize 判别（与 Mario/Goomba 的线上格式一致）
        packet << type << this->getId() << type
            << this->getPosition().x << this->getPosition().y
            << this->getSpeed().x << is_extinguished;
    }
    // RemoveObject 由 destroy() 触发 broadcastRemoveObject 统一广播，不走 serialize
}

void BowserFire::deserialize(eng::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;
    if (msg_type != NetworkMsg::UpdateObject) return;

    float x, y, s_x;
    bool remote_extinguished;
    packet >> x >> y >> s_x >> remote_extinguished;

    // 服务端判灭兜底：本地尚未预测到熄灭时补走完整流程（与服务端随后的
    // RemoveObject 幂等，FireBall 的 exploded 快照同款）
    if (remote_extinguished && !is_extinguished) {
        setExtinguished();
        return;
    }
    // 已熄灭的对象不再接受快照，等待 RemoveObject 清理
    if (is_extinguished) return;

    if (const auto move = getComponent<MoveComponent>()) {
        move->setPosition(eng::Vec2f(x, y));
        move->setSpeedX(s_x);
    }
}

void BowserFire::destroy() {
    // 服务端是移除的唯一权威：销毁时广播 RemoveObject（撞墙/超时/飞出场景等路径）；
    // 客户端本地熄灭销毁静默，由服务端消息兜底
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    if (nm && nm->isServer()) {
        nm->broadcastRemoveObject(this->getId());
    }
    NetworkGameObject::destroy();
}
