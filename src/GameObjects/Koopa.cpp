//
// Created by MINEC on 2026/9/24.
//

#include "Koopa.h"

#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "Scene.h"
#include "AssetManager.h"
#include "NetworkManager.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#endif

namespace {
    // 静止壳静置复活时长（毫秒，权威端计时）
    constexpr int KOOPA_REVIVE_MS = 5000;
    // 踢壳豁免时长（毫秒，双端确定性）：壳刚被踢出时马里奥仍与其重叠，
    // 豁免期内滑动壳侧碰马里奥不结算受伤
    constexpr int KICK_GRACE_MS = 250;
    // 变壳豁免时长（毫秒，双端确定性）：刚缩成/踩停成静止壳时，马里奥的接触
    // 不触发踢出（马里奥还压在壳上，需要等他弹开）
    constexpr int STOMP_GRACE_MS = 250;
}

// 方案 B：客户端预测交互结果后上报（仅客户端实际发送；服务端/单机为本地权威，直接静默）
static void reportKoopaEvent(const Koopa* koopa, const GameEventType type, const float dir_x = 0.f) {
    auto* nm = koopa->getScene() ? koopa->getScene()->getNetworkManager() : nullptr;
    if (!nm || !nm->isClient()) return;
    eng::Packet packet;
    packet << NetworkMsg::ClientEvent << koopa->getId() << type;
    if (type == GameEventType::KoopaKicked || type == GameEventType::KoopaKilledByFireball)
        packet << dir_x;
    nm->getClientSocket().append(packet);
}

Koopa::Koopa(const float x, const float y, const float speed_x) {
    this->position = eng::Vec2f(x, y);
    patrol_speed_x = speed_x;

#ifndef SERVER_BUILD
    walkAnimation.setFrames(FrameManager::getInstance().getFrame("koopa_walk_left_frame"));
    shellAnimation.setFrames(FrameManager::getInstance().getFrame("koopa_shell_frame"));
    this->setSize(walkAnimation.getFrameWidth(), walkAnimation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize * 1.5f);
#endif

    this->addComponent<Collision, BoxCollision>();
    this->addComponent<GravityComponent>()->setSmartGravity(true);
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, 0.f));

    this->tag = "koopa:" + std::to_string(this->id);
    className = "Koopa";
}

Koopa::~Koopa() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
#ifndef SERVER_BUILD
    if (stomp_track) MIX_DestroyTrack(stomp_track);
    if (kick_track) MIX_DestroyTrack(kick_track);
#endif
}

void Koopa::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );
    // BoxCollision::start() 会用贴图尺寸覆盖碰撞盒：按当前状态重设（行走 64×96 / 壳 64×64）
    applyStateSize(state);
    revive_timer.setCallback([this]() -> void { this->revive(); });

#ifndef SERVER_BUILD
    // 常驻 track 绑定预解码音频，播放时 restart（一次性音效，与 Goomba 同款）
    auto& am = AssetManager::getInstance();
    stomp_track = MIX_CreateTrack(am.getMixer());
    if (stomp_track) MIX_SetTrackAudio(stomp_track, am.getSoundBuffer("stomp"));
    kick_track = MIX_CreateTrack(am.getMixer());
    if (kick_track) MIX_SetTrackAudio(kick_track, am.getSoundBuffer("kick"));
#endif
}

#ifndef SERVER_BUILD
void Koopa::switchFacing() {
    if (anim_facing_left == facing_left) return;
    walkAnimation.setFrames(FrameManager::getInstance().getFrame(
        facing_left ? "koopa_walk_left_frame" : "koopa_walk_right_frame"));
    anim_facing_left = facing_left;
}

void Koopa::render(eng::Renderer& renderer) {
    if (is_killed) {
        // 被火系击毙：180° 翻转贴图坠落（渲染器无 flipY，绕中心旋转等效）
        Animation& anim = state == KoopaState::Walking ? walkAnimation : shellAnimation;
        const Animation::Frame& f = anim.getFrame();
        const float w = anim.getFrameWidth();
        const float h = anim.getFrameHeight();
        renderer.drawTexture(f.texture,
                             eng::FloatRect(static_cast<float>(f.textureRect.left),
                                            static_cast<float>(f.textureRect.top),
                                            static_cast<float>(f.textureRect.width),
                                            static_cast<float>(f.textureRect.height)),
                             eng::FloatRect(this->position.x, this->position.y, w, h),
                             180.f, eng::Vec2f(w * 0.5f, h * 0.5f), eng::Color::White, false);
    }
    else if (state == KoopaState::Walking) {
        switchFacing();
        walkAnimation.render(renderer, this->position);
    }
    else {
        shellAnimation.render(renderer, this->position);
    }
    GameObject::render(renderer);
}
#endif

void Koopa::update(const eng::Time deltaTime) {
    GameObject::update(deltaTime);
    // 被击毙：只保留重力坠落轨迹，停掉一切计时与动画，掉出场景底部销毁
    if (is_killed) {
        if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
            destroy();
        }
        return;
    }
    // 朝向跟随速度符号（行走动画翻转与复活恢复方向都依赖它）
    if (state == KoopaState::Walking) {
        facing_left = this->getSpeed().x < 0.f;
#ifndef SERVER_BUILD
        walkAnimation.update(deltaTime);
#endif
    }
#ifndef SERVER_BUILD
    else {
        shellAnimation.update(deltaTime);
    }
#endif
    // 踢壳/变壳豁免倒计时（双端确定性运转）
    kick_grace_timer.update(deltaTime);
    stomp_grace_timer.update(deltaTime);
    // 复活计时仅权威端运转：客户端等快照 state 变化走幂等补流程
    if (isAuthority() && state == KoopaState::ShellIdle) {
        revive_timer.update(deltaTime);
    }
    // 掉出场景底部直接销毁
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
    }
}

void Koopa::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    // 被击毙坠落中：不再参与任何交互
    if (is_killed) return;

    // 与马里奥的交互（踩缩壳/踢壳/踩停/受伤）由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;

    // 被炮弹击毙：沿炮弹飞行方向炸飞并翻身坠落（炮弹爆炸由炮弹侧处理）
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
        setKilledByFireball(event.b_speed.x > 0.f ? 1.f : -1.f);
        return;
    }
    // 壳撞死小怪由受害者侧结算（Goomba/Mushroom 自己认滑动壳），这里一律穿行
    if (other->getClassName() == "Goomba") return;
    if (other->getClassName() == "Mushroom") return;
    // 同类穿行（壳 vs 壳、壳与行走乌龟互不结算）
    if (other->getClassName() == "Koopa") return;
    // BOSS 的飞斧对乌龟无效果：穿行
    if (other->getClassName() == "BowserAxe") return;
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
        // 水平碰撞：贴合碰撞面；仅相向运动（相对速度朝向对方）才掉头——
        // 行走乌龟撞墙掉头巡逻、滑动壳撞墙反向继续滑都依赖它；同向追尾
        // 或被同向对象贴上时不翻转（对方速度 0 的静态地形必然相向，照常反弹）
        const float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        const float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        const float rel_x = this->getSpeed().x - event.b_speed.x;
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - this_->getSize().x);
            if (rel_x > 0.f) moveComponent->setSpeedX(-this->getSpeed().x);
        }
        else {
            moveComponent->moveCollisionXTo(event.b_position.x + other->getSize().x);
            if (rel_x < 0.f) moveComponent->setSpeedX(-this->getSpeed().x);
        }
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

void Koopa::setShellIdle() {
    if (state == KoopaState::ShellIdle || is_killed) return;   // 幂等
    state = KoopaState::ShellIdle;
    // 客户端预测踩缩壳/踩停成功后上报（两种踩合并为同一事件，服务端幂等裁决）
    reportKoopaEvent(this, GameEventType::KoopaStomped);

#ifndef SERVER_BUILD
    if (stomp_track) { MIX_StopTrack(stomp_track, 0); MIX_PlayTrack(stomp_track, 0); }
#endif

    // 停住水平运动，尺寸缩为壳并保持底边对齐（贴图只有壳高，下移贴地）
    if (const auto move = getComponent<MoveComponent>()) move->setSpeedX(0.f);
    applyStateSize(KoopaState::ShellIdle);
    // 变壳豁免：马里奥还压在壳上，短暂忽略他的接触（等他反弹离开），
    // 否则踩中变壳的下一帧就会把壳踢走，壳无法保持静止
    stomp_grace_timer.start(STOMP_GRACE_MS);

    // 权威端静置 5s 复活（客户端等快照 state 变化补流程）
    if (isAuthority()) revive_timer.start(KOOPA_REVIVE_MS);
}

void Koopa::kicked(const float dir_x) {
    if (state != KoopaState::ShellIdle || is_killed) return;   // 幂等：只有静止壳能被踢出
    state = KoopaState::ShellMoving;
    revive_timer.stop();
    stomp_grace_timer.stop();
    // 客户端预测踢壳成功后上报（附带踢出方向，供服务端复现滑动方向）
    reportKoopaEvent(this, GameEventType::KoopaKicked, dir_x);

#ifndef SERVER_BUILD
    if (kick_track) { MIX_StopTrack(kick_track, 0); MIX_PlayTrack(kick_track, 0); }
#endif

    // 滑动速度与炮弹炸飞同速（0.6 倍玩家速度）
    if (const auto move = getComponent<MoveComponent>()) {
        move->setSpeedX(dir_x * CONFIG.game.playerSpeed * 0.6f);
    }
    // 踢壳豁免（双端确定性倒计时，update 中运转）
    kick_grace_timer.start(KICK_GRACE_MS);
}

void Koopa::setKilledByFireball(const float blast_dir_x) {
    if (is_killed) return;
    is_killed = true;
    // 客户端预测击毙后上报（附带炸飞方向，供服务端复现抛物线）
    reportKoopaEvent(this, GameEventType::KoopaKilledByFireball, blast_dir_x);

#ifndef SERVER_BUILD
    if (kick_track) { MIX_StopTrack(kick_track, 0); MIX_PlayTrack(kick_track, 0); }
#endif

    // 停掉状态计时（复活/豁免），只关碰撞保留重力 → 被炸飞后坠落穿出场景，
    // 掉出底部由 update 销毁（碰撞盒同步清零，马里奥的地面几何探测不看 active 标志）
    revive_timer.stop();
    kick_grace_timer.stop();
    stomp_grace_timer.stop();
    if (const auto collision = getComponent<Collision>()) collision->setActive(false);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(0.f, 0.f);
    if (const auto& move = getComponent<MoveComponent>()) {
        // 水平沿炮弹方向炸飞，垂直向上弹起（0.6 倍跳力），重力把轨迹拉成抛物线
        move->setSpeed(eng::Vec2f(blast_dir_x * CONFIG.game.playerSpeed * 0.6f,
                                  -CONFIG.game.jumpForce * 0.6f));
    }
}

void Koopa::revive() {
    if (state != KoopaState::ShellIdle || is_killed) return;   // 幂等
    state = KoopaState::Walking;
    revive_timer.stop();
    // 尺寸还原为行走态并保持底边对齐（左上角上移）
    applyStateSize(KoopaState::Walking);
    // 恢复巡逻（带符号初始方向，朝向随后由速度符号回写）
    if (const auto move = getComponent<MoveComponent>()) move->setSpeedX(patrol_speed_x);
}

void Koopa::reverse() {
    if (state != KoopaState::Walking) return;
    if (const auto move = getComponent<MoveComponent>()) move->setSpeedX(-this->getSpeed().x);
}

bool Koopa::isKickGraceActive() const {
    return state == KoopaState::ShellMoving && kick_grace_timer.getPastTime() < KICK_GRACE_MS;
}

bool Koopa::isShellGraceActive() const {
    return state == KoopaState::ShellIdle && stomp_grace_timer.getPastTime() < STOMP_GRACE_MS;
}

bool Koopa::isAuthority() const {
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    return !nm || !nm->isClient();
}

void Koopa::applyStateSize(const KoopaState new_state) {
    // 行走 64×96、壳 64×64；左上角锚点平移保持底边对齐（缩壳下移、复活上移）
    const float block = CONFIG.game.defaultBlockSize;
    const float new_w = block;
    const float new_h = new_state == KoopaState::Walking ? block * 1.5f : block;
    const float bottom = this->position.y + this->getSize().y;
    this->setSize(new_w, new_h);
    // 同时重设碰撞盒尺寸（马里奥 needGravity 的几何探测不看 active 标志）
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(new_w, new_h);
    if (const auto move = getComponent<MoveComponent>()) {
        move->addPosition(eng::Vec2f(0.f, bottom - new_h - this->position.y), false);
    }
}

void Koopa::serialize(eng::Packet& packet, const NetworkMsg type) {
    if (type == NetworkMsg::SpawnObject) {   // 交给 Scene 处理
        // ID   对象类型   x   y   巡逻速度(带符号)   状态   朝向（状态为枚举，底层 uint8 编码）
        packet << type << this->getId() << ObjectType::Koopa
            << this->getPosition().x << this->getPosition().y << patrol_speed_x
            << state << facing_left;
    } else if (type == NetworkMsg::UpdateObject) {   // 交给自己处理
        // 第二个 type 供 deserialize 判别（与 Goomba/Mario 的线上格式一致）
        packet << type << this->getId() << type
            << this->getPosition().x << this->getPosition().y << this->getSpeed().x
            << state << facing_left;
    }
    // RemoveObject 由 destroy() 触发 broadcastRemoveObject 统一广播，不走 serialize
}

void Koopa::deserialize(eng::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;
    if (msg_type != NetworkMsg::UpdateObject) return;

    float x, y, s_x;
    KoopaState remote_state;
    bool remote_facing;
    packet >> x >> y >> s_x >> remote_state >> remote_facing;

    // 被击毙坠落中不再接受快照（服务端随后的 RemoveObject 幂等清理）
    if (is_killed) return;

    // 状态差异走幂等补流程（补走的方法会再次上报 ClientEvent，服务端幂等，
    // 与 Goomba 现状一致）；补流程含状态转换的底边对齐平移，return 跳过本次
    // 位移同步，防止平移量与快照位置叠加造成二次偏移
    if (remote_state != state) {
        if (remote_state == KoopaState::ShellIdle) {
            setShellIdle();
        }
        else if (remote_state == KoopaState::ShellMoving) {
            kicked(s_x > 0.f ? 1.f : -1.f);
        }
        else {
            // remote Walking：ShellIdle 走正常复活；ShellMoving 属极端失序
            //（TCP 不丢事件，仅兜底）强制回行走
            revive();
            if (state != KoopaState::Walking) {
                state = KoopaState::Walking;
                applyStateSize(KoopaState::Walking);
                if (const auto move = getComponent<MoveComponent>()) move->setSpeedX(patrol_speed_x);
            }
        }
        return;
    }
    // 同态：硬同步位置/速度/朝向（静止壳速度恒 0，只收位置）
    facing_left = remote_facing;
    if (const auto& move = getComponent<MoveComponent>()) {
        move->setPosition(eng::Vec2f(x, y));
        move->setSpeedX(s_x);
    }
}

void Koopa::restoreNetworkState(const KoopaState state_, const bool facing_left_, const float speed_x) {
    facing_left = facing_left_;
    state = state_;
#ifndef SERVER_BUILD
    // 静默还原：快照 y 即当前态权威左上角，不做底边对齐平移、不重放音效/计时
    walkAnimation.setFrames(FrameManager::getInstance().getFrame(
        facing_left ? "koopa_walk_left_frame" : "koopa_walk_right_frame"));
    anim_facing_left = facing_left;
#endif
    // 按目标态重设尺寸（不平移：位置由快照直接给出）
    const float block = CONFIG.game.defaultBlockSize;
    this->setSize(block, state == KoopaState::Walking ? block * 1.5f : block);
    if (const auto box = getComponent<Collision, BoxCollision>()) box->setSize(block, this->getSize().y);
    if (const auto& move = getComponent<MoveComponent>()) {
        move->setSpeedX(state == KoopaState::Walking ? patrol_speed_x
                        : state == KoopaState::ShellMoving ? speed_x : 0.f);
    }
}

void Koopa::destroy() {
    // 服务端是移除的唯一权威：销毁时广播 RemoveObject（掉出场景底部一条路径）；
    // 客户端本地销毁静默，由服务端消息兜底
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    if (nm && nm->isServer()) {
        nm->broadcastRemoveObject(this->getId());
    }
    NetworkGameObject::destroy();
}
