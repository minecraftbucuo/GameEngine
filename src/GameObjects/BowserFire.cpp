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
#include "Scene.h"
#include "Core/Types.h"

namespace {
    // 火焰弹存活上限（ms）：防止沿空旷地形一直飞
    constexpr int BOWSER_FIRE_TTL = 5000;
}

BowserFire::BowserFire(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);

#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame(
        speed_x < 0.f ? "bowser_fire_left_frame" : "bowser_fire_right_frame"));
    this->setSize(animation.getFrameWidth(), animation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize * 1.5f, CONFIG.game.defaultBlockSize * 0.5f);
#endif

    this->addComponent<Collision, BoxCollision>();
    // 直线飞行：不加重力组件
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, 0.f));

    ttl_timer.setCallback([this]() -> void { this->setExtinguished(); });
    ttl_timer.start(BOWSER_FIRE_TTL);

    this->tag = "bowser_fire:" + std::to_string(this->id);
    className = "BowserFire";
}

BowserFire::~BowserFire() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void BowserFire::start() {
    GameObject::start();
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
    // 只被地形/可顶物（地面、砖块、箱子）阻挡熄灭，穿过其余对象（含友军）
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
