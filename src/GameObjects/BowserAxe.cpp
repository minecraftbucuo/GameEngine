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
    // 抛物线轨迹：水平直飞 + 先上抛，由重力拉出弧线
    this->addComponent<GravityComponent>();
    this->addComponent<MoveComponent>()->setSpeed(
        eng::Vec2f(speed_x, -CONFIG.game.jumpForce * 0.8f));

    ttl_timer.setCallback([this]() -> void { this->setDestroyed(); });
    ttl_timer.start(BOWSER_AXE_TTL);

    this->tag = "bowser_axe:" + std::to_string(this->id);
    className = "BowserAxe";
}

BowserAxe::~BowserAxe() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void BowserAxe::start() {
    GameObject::start();
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
    // 命中马里奥：伤害与销毁由马里奥侧的 handleCollision 处理
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
