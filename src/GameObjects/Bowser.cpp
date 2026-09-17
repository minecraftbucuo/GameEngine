//
// Created by MINEC on 2026/9/17.
//

#include "Bowser.h"

#include <cmath>

#include "BowserFire.h"
#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "HealthBar.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "Scene.h"
#include "AssetManager.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#endif

namespace {
    constexpr int BOWSER_FIRE_COOLDOWN = 2800;  // 两次喷火之间的冷却（ms）
    constexpr int BOWSER_SHOOT_DELAY = 300;     // 张嘴后到吐出火焰弹的延时（ms）
    constexpr int BOWSER_BREATH_TIME = 700;     // 喷火动作总时长（ms）
    constexpr float BOWSER_FIRE_SPEED = 350.f;  // 火焰弹飞行速度（px/s）
}

Bowser::Bowser(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);
    facing_left = speed_x < 0.f;
    patrol_speed_x = std::abs(speed_x);

#ifndef SERVER_BUILD
    auto& fm = FrameManager::getInstance();
    walkAnimation.setFrames(fm.getFrame(facing_left ? "bowser_walk_left_frame" : "bowser_walk_right_frame"));
    breathAnimation.setFrames(fm.getFrame(facing_left ? "bowser_breath_left_frame" : "bowser_breath_right_frame"));
    anim_facing_left = facing_left;
    this->setSize(walkAnimation.getFrameWidth(), walkAnimation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize * 2.f, CONFIG.game.defaultBlockSize * 2.f);
#endif

    this->addComponent<Collision, BoxCollision>();
    this->addComponent<GravityComponent>();
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, 0.f));
    this->addComponent<HealthBar>();

    // 喷火节奏：冷却循环计时 → 站定张嘴 → 中段吐火焰弹 → 收嘴恢复巡逻
    fire_timer.setCallback([this]() -> void { this->startBreathing(); });
    fire_timer.start(BOWSER_FIRE_COOLDOWN, true);

    this->tag = "bowser:" + std::to_string(this->id);
    className = "Bowser";
}

Bowser::~Bowser() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
#ifndef SERVER_BUILD
    if (fire_track) MIX_DestroyTrack(fire_track);
    if (kick_track) MIX_DestroyTrack(kick_track);
#endif
}

void Bowser::start() {
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
    fire_track = MIX_CreateTrack(am.getMixer());
    if (fire_track) MIX_SetTrackAudio(fire_track, am.getSoundBuffer("fireball"));
    kick_track = MIX_CreateTrack(am.getMixer());
    if (kick_track) MIX_SetTrackAudio(kick_track, am.getSoundBuffer("kick"));
#endif
}

#ifndef SERVER_BUILD
void Bowser::render(eng::Renderer& renderer) {
    GameObject::render(renderer);
    if (is_killed) {
        // 被击败：180° 翻转贴图（渲染器无 flipY，绕中心旋转等效）
        const Animation::Frame& f = walkAnimation.getFrame();
        const float w = walkAnimation.getFrameWidth();
        const float h = walkAnimation.getFrameHeight();
        renderer.drawTexture(f.texture,
                             eng::FloatRect(static_cast<float>(f.textureRect.left),
                                            static_cast<float>(f.textureRect.top),
                                            static_cast<float>(f.textureRect.width),
                                            static_cast<float>(f.textureRect.height)),
                             eng::FloatRect(this->position.x, this->position.y, w, h),
                             180.f, eng::Vec2f(w * 0.5f, h * 0.5f), eng::Color::White, false);
    }
    else if (is_breathing) breathAnimation.render(renderer, this->position);
    else walkAnimation.render(renderer, this->position);
}
#endif

void Bowser::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);

    if (!is_killed) {
        // 朝向跟随巡逻方向（喷火站定期间速度为 0，保持当前朝向）
        const float sx = this->getSpeed().x;
        if (sx > 1.f) facing_left = false;
        else if (sx < -1.f) facing_left = true;

#ifndef SERVER_BUILD
        // 朝向变化时切换走路/喷火两组动画的帧集
        if (facing_left != anim_facing_left) {
            anim_facing_left = facing_left;
            auto& fm = FrameManager::getInstance();
            walkAnimation.setFrames(fm.getFrame(facing_left ? "bowser_walk_left_frame" : "bowser_walk_right_frame"));
            breathAnimation.setFrames(fm.getFrame(facing_left ? "bowser_breath_left_frame" : "bowser_breath_right_frame"));
        }

        if (is_breathing) {
            breathAnimation.update(deltaTime);
            shoot_timer.update(deltaTime);
            breath_timer.update(deltaTime);
        }
        else {
            walkAnimation.update(deltaTime);
            fire_timer.update(deltaTime);
        }
#endif
    }

    // 掉出场景底部直接销毁（被击败炸飞坠出场景同样由此销毁）
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
    }
}

void Bowser::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    // 已被击败则不再响应
    if (is_killed) return;
    // 与马里奥的交互（踩踏无效、接触即伤）由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;
    // 自家火焰弹：穿过（火焰弹侧同样忽略发射路径上的友军）
    if (other->getClassName() == "BowserFire") return;
    // 被炮弹击中：扣血；血量打空沿炮弹飞行方向炸飞坠落（炮弹爆炸由炮弹侧处理）
    if (other->getClassName() == "FireBall") {
        const auto& health_bar = getComponent<HealthBar>();
        health_bar->takeDamage(1);
        if (health_bar->isDead()) {
            // 炸飞方向：优先按炮弹飞行方向；炮弹近乎垂直落下时按相对位置向外炸
            const float dir = std::abs(event.b_speed.x) > 1.f
                ? (event.b_speed.x > 0.f ? 1.f : -1.f)
                : (event.a_position.x >= event.b_position.x ? 1.f : -1.f);
            setKilled(dir);
        }
        return;
    }
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

    if (dx <= dy) {
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
        moveComponent->setSpeedX(-this->getSpeed().x);
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

void Bowser::setKilled(const float blast_dir_x) {
    if (is_killed) return;
    is_killed = true;
    is_breathing = false;

#ifndef SERVER_BUILD
    if (kick_track) { MIX_StopTrack(kick_track, 0); MIX_PlayTrack(kick_track, 0); }
#endif

    fire_timer.stop();
    shoot_timer.stop();
    breath_timer.stop();

    // 只关闭碰撞（并清零碰撞盒尺寸，马里奥的地面几何探测不看 active 标志）；
    // 重力保留 → 被炸飞后坠落穿出场景，掉出底部由 update 销毁
    if (const auto collision = getComponent<Collision>()) collision->setActive(false);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);
    if (const auto move = getComponent<MoveComponent>()) {
        // 水平沿炮弹方向炸飞，垂直向上弹起（0.6 倍跳力），重力把轨迹拉成抛物线
        move->setSpeed(eng::Vec2f(blast_dir_x * CONFIG.game.playerSpeed * 0.6f,
                                  -CONFIG.game.jumpForce * 0.6f));
    }
}

void Bowser::startBreathing() {
    if (is_killed || is_breathing) return;
    is_breathing = true;

    // 原地喷火：站定（收嘴后按记录的巡逻速度恢复）
    if (const auto move = getComponent<MoveComponent>()) move->setSpeedX(0.f);

    shoot_timer.setCallback([this]() -> void { this->spawnFire(); });
    shoot_timer.start(BOWSER_SHOOT_DELAY);
    breath_timer.setCallback([this]() -> void { this->stopBreathing(); });
    breath_timer.start(BOWSER_BREATH_TIME);
}

void Bowser::stopBreathing() {
    if (!is_breathing) return;
    is_breathing = false;
    if (const auto move = getComponent<MoveComponent>())
        move->setSpeedX(facing_left ? -patrol_speed_x : patrol_speed_x);
    shoot_timer.stop();
}

void Bowser::spawnFire() {
    if (is_killed) return;
    Scene* scene = getScene();
    if (!scene) return;

#ifndef SERVER_BUILD
    if (fire_track) { MIX_StopTrack(fire_track, 0); MIX_PlayTrack(fire_track, 0); }
#endif

    // 火焰弹从嘴部生成：朝向前方一段距离、高度约在身体 2/5 处
    const float dir = facing_left ? -1.f : 1.f;
    const float fire_x = facing_left
        ? this->position.x - CONFIG.game.defaultBlockSize * 1.6f
        : this->position.x + this->getSize().x + CONFIG.game.defaultBlockSize * 0.1f;
    const float fire_y = this->position.y + this->getSize().y * 0.4f;
    scene->addObject(std::make_shared<BowserFire>(fire_x, fire_y, dir * BOWSER_FIRE_SPEED));
}
