//
// Created by MINEC on 2026/9/15.
//

#include "Goomba.h"

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

Goomba::Goomba(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);

#ifndef SERVER_BUILD
    walkAnimation.setFrames(FrameManager::getInstance().getFrame("goomba_walk_frame"));
    squashAnimation.setFrames(FrameManager::getInstance().getFrame("goomba_squash_frame"));
    this->setSize(walkAnimation.getFrameWidth(), walkAnimation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif

    this->addComponent<Collision, BoxCollision>();
    this->addComponent<GravityComponent>();
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, 0.f));

    this->tag = "goomba:" + std::to_string(this->id);
    className = "Goomba";
}

Goomba::~Goomba() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
#ifndef SERVER_BUILD
    if (stomp_track) MIX_DestroyTrack(stomp_track);
    if (kick_track) MIX_DestroyTrack(kick_track);
#endif
}

void Goomba::start() {
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
    stomp_track = MIX_CreateTrack(am.getMixer());
    if (stomp_track) MIX_SetTrackAudio(stomp_track, am.getSoundBuffer("stomp"));
    kick_track = MIX_CreateTrack(am.getMixer());
    if (kick_track) MIX_SetTrackAudio(kick_track, am.getSoundBuffer("kick"));
#endif
}

#ifndef SERVER_BUILD
void Goomba::render(eng::Renderer& renderer) {
    GameObject::render(renderer);
    if (killed_by_fireball) {
        // 被炮弹击毙：180° 翻转贴图（渲染器无 flipY，绕中心旋转等效）
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
    else if (is_squashed) squashAnimation.render(renderer, this->position);
    else walkAnimation.render(renderer, this->position);
}
#endif

void Goomba::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_squashed && !killed_by_fireball) {
        squash_timer.update(deltaTime);
    }
#ifndef SERVER_BUILD
    else {
        walkAnimation.update(deltaTime);
    }
#endif
    // 掉出场景底部直接销毁
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
    }
}

void Goomba::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    // 已被踩扁则不再响应
    if (is_squashed) return;
    // 与马里奥的交互（踩踏/受伤）由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;
    // 被炮弹击中：沿炮弹飞行方向炸飞并翻身坠落（炮弹爆炸由炮弹侧处理）
    if (other->getClassName() == "FireBall") {
        // 炸飞方向：优先按炮弹飞行方向；炮弹近乎垂直落下时按相对位置向外炸
        const float dir = std::abs(event.b_speed.x) > 1.f
            ? (event.b_speed.x > 0.f ? 1.f : -1.f)
            : (event.a_position.x >= event.b_position.x ? 1.f : -1.f);
        setKilledByFireball(dir);
        return;
    }
    // 被 BOSS 火焰弹点燃：同样炸飞坠落（火焰弹侧不与小怪交互）
    if (other->getClassName() == "BowserFire") {
        const float dir = event.b_speed.x > 0.f ? 1.f : -1.f;
        setKilledByFireball(dir);
        return;
    }
    // BOSS 的旋转飞斧对小怪无效果：穿行
    if (other->getClassName() == "BowserAxe") return;
    // 与 BOSS 不做实体交互：互相穿行。BOSS 碰撞盒（80×100）小于贴图且向下对齐脚底，
    // 通用垂直解析按"碰撞盒位置 + 对方贴图全高"贴面，会把小怪压进地面一个偏移量
    if (other->getClassName() == "Bowser") return;
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

void Goomba::setSquashed() {
    if (is_squashed) return;
    is_squashed = true;

#ifndef SERVER_BUILD
    if (stomp_track) { MIX_StopTrack(stomp_track, 0); MIX_PlayTrack(stomp_track, 0); }
#endif

    // 停止一切运动
    if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(false);
    if (const auto move = getComponent<MoveComponent>()) {
        // 踩扁贴图只有一半高，下移半个方块贴地
        move->addPosition(eng::Vec2f(0.f, CONFIG.game.defaultBlockSize / 2.f), false);
        move->setSpeed(eng::Vec2f(0.f, 0.f));
        move->setActive(false);
    }
    // 关闭碰撞；同时清零碰撞盒尺寸（马里奥 needGravity 的几何探测不看 active 标志）
    if (const auto collision = getComponent<Collision>()) collision->setActive(false);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);

    // 延时销毁
    squash_timer.setCallback([this]() -> void { this->destroy(); });
    squash_timer.start(500);
}

void Goomba::setKilledByFireball(const float blast_dir_x) {
    if (is_squashed) return;
    is_squashed = true;
    killed_by_fireball = true;

#ifndef SERVER_BUILD
    if (kick_track) { MIX_StopTrack(kick_track, 0); MIX_PlayTrack(kick_track, 0); }
#endif

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
