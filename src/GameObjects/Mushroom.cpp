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
#include "Koopa.h"
#include "MoveComponent.h"
#include "Scene.h"
#include "AssetManager.h"
#include "NetworkManager.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#endif

// 从方块里长出来的速度（像素/秒），升起一个方块位约 0.8 秒
constexpr float EMERGE_SPEED = 80.f;

// 方案 B：客户端预测交互结果后上报（仅客户端实际发送；服务端/单机为本地权威，直接静默）
static void reportMushroomEvent(const Mushroom* mushroom, const GameEventType type, const float blast_dir_x = 0.f) {
    auto* nm = mushroom->getScene() ? mushroom->getScene()->getNetworkManager() : nullptr;
    if (!nm || !nm->isClient()) return;
    eng::Packet packet;
    packet << NetworkMsg::ClientEvent << mushroom->getId() << type;
    if (type == GameEventType::MushroomKilled) packet << blast_dir_x;
    nm->getClientSocket().append(packet);
}

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

    // 升起阶段：关闭重力保证匀速上升；碰撞保持开启，马里奥在升起途中即可吃掉
    // （自身 handleCollision 在升起阶段直接 return，不会与承载方块误响应）
    this->addComponent<Collision, BoxCollision>();
    const auto gravity = this->addComponent<GravityComponent>();
    gravity->setActive(false);
    gravity->setSmartGravity(true);
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
    // 只有真正处于"刚从方块里长出来"的状态才播出生音效；加入服务器时还原的
    // 已长出/已被吃蘑菇（restoreNetworkState）不重放
    if (appear_track && is_emerging && !is_eaten) { MIX_StopTrack(appear_track, 0); MIX_PlayTrack(appear_track, 0); }
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
    // 炮弹与道具互不交互：穿行（FireBall 侧同样对蘑菇穿行，双向一致）——
    // 否则蘑菇会对着穿身而过的炮弹走通用解析（被弹得掉头）
    if (other->getClassName() == "FireBall") return;
    if (!this_->getMoveAble()) return;

    // 碰到敌人/BOSS/飞斧：朝远离敌人的方向弹飞，坠落穿出场景后销毁
    const std::string& other_class = other->getClassName();
    if (other_class == "Bowser" || other_class == "BowserAxe" || other_class == "BowserFire") {
        const float enemy_center = event.b_position.x + other->getSize().x * 0.5f;
        const float self_center = event.a_position.x + this_->getSize().x * 0.5f;
        setKilled(enemy_center < self_center ? 1.f : -1.f);
        return;
    }
    // 被滑动龟壳撞中：沿壳的滑动方向弹飞（结算在受害者侧；静止壳/行走乌龟穿行）
    if (other_class == "Koopa") {
        if (const auto koopa = std::dynamic_pointer_cast<Koopa>(other);
            koopa && koopa->isShellMoving()) {
            setKilled(event.b_speed.x > 0.f ? 1.f : -1.f);
        }
        return;
    }

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
    // 客户端预测吃掉后上报，服务端裁决并统一广播移除
    reportMushroomEvent(this, GameEventType::MushroomEaten);

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

void Mushroom::setKilled(const float blast_dir_x) {
    if (is_eaten) return;
    const auto collision = getComponent<Collision>();
    if (!collision || !collision->getActive()) return;

    // 客户端预测击杀后上报（附带弹飞方向，供服务端复现抛物线）
    reportMushroomEvent(this, GameEventType::MushroomKilled, blast_dir_x);

    // 关闭碰撞（清零碰撞盒尺寸），保留重力 → 弹飞后坠落穿出场景，由 update 兜底销毁
    collision->setActive(false);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);

    if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(true);
    if (const auto move = getComponent<MoveComponent>()) {
        move->setSpeed(eng::Vec2f(blast_dir_x * 180.f, -CONFIG.game.jumpForce * 0.55f));
        move->setActive(true);
    }
}

void Mushroom::restoreNetworkState(const float birth_y, const bool emerging, const bool eaten) {
    spawn_y = birth_y;
    if (eaten) {
        // 已被吃的蘑菇：静默进入被吃状态（不走 setEaten，避免客户端重复上报
        // ClientEvent，也不重播音效/延时器），等 RemoveObject 清理
        is_eaten = true;
        if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(false);
        if (const auto move = getComponent<MoveComponent>()) {
            move->setSpeed(eng::Vec2f(0.f, 0.f));
            move->setActive(false);
        }
        if (const auto collision = getComponent<Collision>()) collision->setActive(false);
        if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);
        return;
    }
    if (!emerging) {
        // 已长出的蘑菇：跳过升起流程，直接按快照位置开启重力与碰撞开始行走
        is_emerging = false;
        if (const auto move = getComponent<MoveComponent>()) {
            move->setPosition(this->position);
            move->setSpeed(eng::Vec2f(walk_speed, 0.f));
        }
        if (const auto collision = getComponent<Collision>()) collision->setActive(true);
        if (const auto gravity = getComponent<GravityComponent>()) gravity->setActive(true);
    }
    // 升起中途加入：保持升起状态，从快照位置以 EMERGE_SPEED 继续升到 spawn_y 目标位
}

void Mushroom::serialize(eng::Packet& packet, const NetworkMsg type) {
    if (type == NetworkMsg::SpawnObject) {   // 交给 Scene 处理
        // ID   对象类型   x   y   s_x   spawn_y   is_emerging   is_eaten
        packet << type << this->getId() << ObjectType::Mushroom
            << this->getPosition().x << this->getPosition().y << walk_speed
            << spawn_y << is_emerging << is_eaten;
    } else if (type == NetworkMsg::UpdateObject) {   // 交给自己处理
        // 第二个 type 供 deserialize 判别（与 Mario/Goomba 的线上格式一致）
        packet << type << this->getId() << type
            << this->getPosition().x << this->getPosition().y << walk_speed << is_eaten;
    }
    // RemoveObject 由 destroy() 触发 broadcastRemoveObject 统一广播，不走 serialize
}

void Mushroom::deserialize(eng::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;
    if (msg_type != NetworkMsg::UpdateObject) return;

    float x, y, s_x;
    bool remote_eaten;
    packet >> x >> y >> s_x >> remote_eaten;

    // 服务端判死兜底：本地尚未预测到被吃时补走完整流程（组件关闭/上报，服务端对重复上报幂等）
    if (remote_eaten && !is_eaten) {
        setEaten();
        return;
    }
    // 已被吃掉的对象（本地预测或补走流程）不再接受快照，等待 RemoveObject 清理
    if (is_eaten) return;
    // 升起阶段轨迹由出生点+速度决定（确定性），不同步快照，防止水平速度被污染
    if (is_emerging) return;

    if (const auto& move = getComponent<MoveComponent>()) {
        move->setPosition(eng::Vec2f(x, y));
        move->setSpeedX(s_x);
        walk_speed = s_x;
    }
}

void Mushroom::destroy() {
    // 服务端是移除的唯一权威：销毁时广播 RemoveObject（被吃延时销毁、弹飞坠落与
    // 掉出场景底部三种路径）；客户端本地销毁静默，由服务端消息兜底
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    if (nm && nm->isServer()) {
        nm->broadcastRemoveObject(this->getId());
    }
    NetworkGameObject::destroy();
}
