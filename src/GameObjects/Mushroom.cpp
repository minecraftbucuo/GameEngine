//
// Created by MINEC on 2026/9/18.
//

#include "Mushroom.h"

#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "Scene.h"
#include "AssetManager.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#endif

// 从方块里长出来的速度（像素/秒），升起一个方块位约 0.8 秒
constexpr float EMERGE_SPEED = 80.f;

Mushroom::Mushroom(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);
    spawn_y = y;
    walk_speed = speed_x;

#ifndef SERVER_BUILD
    idleAnimation.setFrames(FrameManager::getInstance().getFrame("mushroom_frame"));
    this->setSize(idleAnimation.getFrameWidth(), idleAnimation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif

    // 升起阶段：碰撞与重力都先关闭，避免与承载方块/马里奥误碰撞
    this->addComponent<Collision, BoxCollision>()->setActive(false);
    this->addComponent<GravityComponent>()->setActive(false);
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(0.f, -EMERGE_SPEED));

    this->tag = "mushroom:" + std::to_string(this->id);
    className = "Mushroom";
}

Mushroom::~Mushroom() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
#ifndef SERVER_BUILD
    if (appear_track) MIX_DestroyTrack(appear_track);
    if (eaten_track) MIX_DestroyTrack(eaten_track);
#endif
}

void Mushroom::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );

#ifndef SERVER_BUILD
    // 常驻 track 绑定预解码音频，播放时 restart（一次性音效）
    auto& am = AssetManager::getInstance();
    appear_track = MIX_CreateTrack(am.getMixer());
    if (appear_track) MIX_SetTrackAudio(appear_track, am.getSoundBuffer("powerup_appears"));
    eaten_track = MIX_CreateTrack(am.getMixer());
    if (eaten_track) MIX_SetTrackAudio(eaten_track, am.getSoundBuffer("powerup"));
    if (appear_track) { MIX_StopTrack(appear_track, 0); MIX_PlayTrack(appear_track, 0); }
#endif
}

#ifndef SERVER_BUILD
void Mushroom::render(eng::Renderer& renderer) {
    if (is_eaten) return;
    idleAnimation.render(renderer, this->position);
    GameObject::render(renderer);
}
#endif

void Mushroom::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_eaten) {
        eaten_timer.update(deltaTime);
        return;
    }
#ifndef SERVER_BUILD
    idleAnimation.update(deltaTime);
#endif

    if (is_emerging) {
        // 升到方块顶部正上方即完成：开启碰撞与重力，开始行走
        const float target_y = spawn_y - CONFIG.game.defaultBlockSize + 20.f;
        if (this->position.y <= target_y) {
            if (const auto move = getComponent<MoveComponent>()) {
                move->setPositionY(target_y);
                move->setSpeed(eng::Vec2f(walk_speed, 0.f));
            }
            if (const auto collision = getComponent<Collision>()) collision->setActive(true);
            if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(true);
            is_emerging = false;
        }
    }

    // 掉出场景底部直接销毁
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
    }
}

void Mushroom::handleCollision(const CollisionEvent& event) {
    // 升起阶段碰撞是关闭的，理论上收不到事件；被吃掉后不再响应
    if (is_emerging || is_eaten) return;

    auto& this_ = event.a;
    auto& other = event.b;

    // 与马里奥的交互（吃掉回复）由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;
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

    // 底边在对方中线之上 = 站在其上：在方块接缝处，重力每帧下陷产生的 dy 会大于刚
    // 跨过接缝的 dx 细条，仅凭 dx<=dy 会把"踩在相邻方块上"误判成撞墙而原地掉头
    const bool standing_on_other =
        event.a_position.y + this_->getSize().y <= event.b_position.y + other->getSize().y * 0.5f;
    if (dx <= dy && !standing_on_other) {
        // 水平碰撞：贴合碰撞面并掉头继续走
        const float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        const float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - this_->getSize().x);
        }
        else {
            moveComponent->moveCollisionXTo(event.b_position.x + other->getSize().x);
        }
        moveComponent->setSpeedX(-walk_speed);
        walk_speed = -walk_speed;
    }
    else {
        // 垂直碰撞：贴合地面/物体表面并停止垂直运动
        const float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
        const float bottom_y = std::abs(
            event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            moveComponent->moveCollisionYTo(event.b_position.y - this_->getSize().y);
        }
        else {
            moveComponent->moveCollisionYTo(event.b_position.y + other->getSize().y);
        }
        moveComponent->setSpeedY(0.f);
    }
}

void Mushroom::setEaten() {
    if (is_eaten) return;
    is_eaten = true;

#ifndef SERVER_BUILD
    if (eaten_track) { MIX_StopTrack(eaten_track, 0); MIX_PlayTrack(eaten_track, 0); }
#endif

    // 停止一切运动与碰撞（碰撞盒尺寸清零，马里奥 needGravity 的几何探测不看 active 标志）
    if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(false);
    if (const auto move = getComponent<MoveComponent>()) {
        move->setSpeed(eng::Vec2f(0.f, 0.f));
        move->setActive(false);
    }
    if (const auto collision = getComponent<Collision>()) collision->setActive(false);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);

    // 音效挂在本对象 track 上，等播放一段再销毁对象
    eaten_timer.setCallback([this]() -> void { this->destroy(); });
    eaten_timer.start(1000);
}
