//
// Created by MINEC on 2026/9/17.
//

#include "Bowser.h"

#include <cmath>
#include <random>

#include "BowserAxe.h"
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
    constexpr int BOWSER_ACTIVATE_INTERVAL = 200;   // 延迟激活检查间隔（ms）
    constexpr float BOWSER_ACTIVATE_RANGE = 800.f;  // 马里奥进入该水平距离才现身
    constexpr int BOWSER_DECISION_INTERVAL = 1000;  // AI 决策心跳间隔（ms）
    constexpr int BOWSER_AIR_DECISION_INTERVAL = 450; // 腾空掷斧心跳间隔（ms）
    constexpr int BOWSER_SHOOT_DELAY = 300;         // 张嘴后到吐出火焰弹的延时（ms）
    constexpr int BOWSER_BREATH_TIME = 700;         // 喷火动作总时长（ms）
    constexpr float BOWSER_FIRE_SPEED = 350.f;      // 火焰弹飞行速度（px/s）
    constexpr float BOWSER_AXE_SPEED = 420.f;       // 飞斧水平初速（px/s）
    constexpr int BOWSER_AXE_BURST_INTERVAL = 140;  // 飞斧连发间隔（ms）
    constexpr float BOWSER_JUMP_FORCE = 1.1f;       // 跳扑力度（相对马里奥跳力）
    constexpr float BOWSER_JUMP_CHASE = 1.4f;       // 跳扑水平前压速度（相对巡逻速度）
    constexpr float BOWSER_HITBOX_W = 80.f;         // 碰撞盒宽（小于渲染贴图，贴合躯干）
    constexpr float BOWSER_HITBOX_H = 100.f;         // 碰撞盒高（底部与贴图对齐）

    // AI 决策随机源
    std::mt19937& decisionRng() {
        static std::mt19937 rng{std::random_device{}()};
        return rng;
    }
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
    // 沉睡中不巡逻：激活后才按巡逻速度开始走动
    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(0.f, 0.f));
    auto healthBar = this->addComponent<HealthBar>();
    healthBar->setMaxHealth(5);
    healthBar->setHealth(5);

    // 延迟激活检查：马里奥接近才现身（远处的行动不会被察觉）
    activate_timer.setCallback([this]() -> void { this->checkActivation(); });
    activate_timer.start(BOWSER_ACTIVATE_INTERVAL, true);

    // 飞斧连发计时：每到间隔发一把（回调只注册一次）
    axe_timer.setCallback([this]() -> void { this->throwAxeOne(); });

    this->tag = "bowser:" + std::to_string(this->id);
    className = "Bowser";
}

Bowser::~Bowser() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
#ifndef SERVER_BUILD
    if (kick_track) MIX_DestroyTrack(kick_track);
#endif
}

void Bowser::start() {
    GameObject::start();

    // BoxCollision::start() 会用渲染贴图尺寸覆盖碰撞盒，必须在组件初始化后重新缩小：
    // 水平居中、底部对齐（头冠/四肢留白不参与碰撞）
    if (const auto box = getComponent<Collision, BoxCollision>()) {
        box->setSize(BOWSER_HITBOX_W, BOWSER_HITBOX_H);
        box->setOffset(eng::Vec2f((this->getSize().x - BOWSER_HITBOX_W) * 0.5f,
                                  this->getSize().y - BOWSER_HITBOX_H));
    }

    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );

#ifndef SERVER_BUILD
    // 常驻 track 绑定预解码音频，播放时 restart（一次性音效）
    auto& am = AssetManager::getInstance();
    kick_track = MIX_CreateTrack(am.getMixer());
    if (kick_track) MIX_SetTrackAudio(kick_track, am.getSoundBuffer("kick"));
#endif
}

#ifndef SERVER_BUILD
void Bowser::render(eng::Renderer& renderer) {
    // 沉睡中保持隐身（马里奥接近才现身）
    if (!is_activated) return;

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
    GameObject::render(renderer);
}
#endif

void Bowser::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);

    // 掉出场景底部直接销毁（被击败炸飞坠出场景同样由此销毁）
    if (getScene() && this->position.y > static_cast<float>(getScene()->getWindowSize().y)) {
        destroy();
        return;
    }

    // 延迟激活检查：未激活时唯一运转的逻辑（驱动 checkActivation 回调）
    activate_timer.update(deltaTime);

    if (!is_activated || is_killed) return;

    // 朝向跟随移动方向（喷火站定期间速度为 0，保持当前朝向）
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

    if (is_breathing) breathAnimation.update(deltaTime);
    else walkAnimation.update(deltaTime);
#endif

    if (is_breathing) {
        shoot_timer.update(deltaTime);
        breath_timer.update(deltaTime);
    }
    else {
        // AI 决策心跳（喷火中不决策，动作收尾后恢复心跳）
        decision_timer.update(deltaTime);
    }

    // 飞斧连发计时独立于喷火/决策状态（空中连掷期间同样运转）
    axe_timer.update(deltaTime);
}

void Bowser::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    // 已被击败则不再响应
    if (is_killed) return;
    // 沉睡中只处理地形贴地（隐身期间不与任何单位交互）
    if (!is_activated) {
        const std::string& name = other->getClassName();
        if (name != "Ground" && name != "Brick" && name != "Box") return;
    }
    // 与马里奥的交互（踩踏无效、接触即伤）由马里奥侧的 handleCollision 处理
    if (other->getClassName() == "Mario") return;
    // 自家投射物：穿过（投射物侧同样忽略发射路径上的友军）
    if (other->getClassName() == "BowserFire") return;
    if (other->getClassName() == "BowserAxe") return;
    // 小怪与 BOSS 不做实体交互：互相穿行（小怪侧同步忽略 BOSS，
    // 否则小怪会按贴图全高解析本类带偏移的碰撞盒位置而被压进地面）
    if (other->getClassName() == "Goomba") return;
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

    // 计算碰撞盒（小于渲染贴图，a_position 即含 offset 的碰撞盒位置）与对方的重合度
    const float dx = std::min(event.a_position.x + BOWSER_HITBOX_W,
                              event.b_position.x + other->getSize().x) - std::max(
        event.a_position.x, event.b_position.x);
    const float dy = std::min(event.a_position.y + BOWSER_HITBOX_H,
                              event.b_position.y + other->getSize().y) - std::max(
        event.a_position.y, event.b_position.y);

    if (dx <= dy) {
        // 水平碰撞：贴合碰撞面并掉头继续走
        const float right_x = std::abs(
            event.a_position.x + BOWSER_HITBOX_W - (event.b_position.x + other->getSize().x * 0.5f));
        const float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - BOWSER_HITBOX_W);
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
            event.a_position.y + BOWSER_HITBOX_H - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            // 起跳保护：脚下残余重叠且正在上升（jumpAction 刚设置跳跃速度），
            // 不贴合不清速度，否则跳跃速度会在同帧被地面碰撞清零、永远跳不起来
            if (this->getSpeed().y < 0.f) return;
            moveComponent->moveCollisionYTo(event.b_position.y - BOWSER_HITBOX_H);
            moveComponent->setSpeedY(0.f);
            // 跳扑落地：清除腾空标志并恢复巡逻（喷火站定期间保持速度 0）
            if (is_in_air) {
                is_in_air = false;
                if (!is_breathing)
                    moveComponent->setSpeedX(facing_left ? -patrol_speed_x : patrol_speed_x);
                // 决策心跳恢复地面节奏（腾空期间切的是掷斧短间隔）
                decision_timer.start(BOWSER_DECISION_INTERVAL, true);
            }
        }
        else {
            // 上升撞头：贴合物体下方并停止垂直运动
            moveComponent->moveCollisionYTo(event.b_position.y + other->getSize().y);
            moveComponent->setSpeedY(0.f);
        }
    }
}

void Bowser::setKilled(const float blast_dir_x) {
    if (is_killed) return;
    is_killed = true;
    is_breathing = false;

#ifndef SERVER_BUILD
    if (kick_track) { MIX_StopTrack(kick_track, 0); MIX_PlayTrack(kick_track, 0); }
#endif

    activate_timer.stop();
    decision_timer.stop();
    axe_timer.stop();
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

void Bowser::checkActivation() {
    if (is_activated || is_killed) return;
    Scene* scene = getScene();
    if (!scene) return;

    for (const auto& obj : scene->getGameObjects()) {
        if (!obj || obj->getClassName() != "Mario") continue;
        // 马里奥水平距离进入阈值：现身并开始行动
        if (std::abs(obj->getPosition().x - this->position.x) <= BOWSER_ACTIVATE_RANGE) {
            is_activated = true;
            activate_timer.stop();
            if (const auto move = getComponent<MoveComponent>())
                move->setSpeedX(facing_left ? -patrol_speed_x : patrol_speed_x);
            decision_timer.setCallback([this]() -> void { this->makeDecision(); });
            decision_timer.start(BOWSER_DECISION_INTERVAL, true);
        }
        break;
    }
}

void Bowser::makeDecision() {
    if (is_killed || is_breathing) return;
    // 腾空时不做落地决策：跳扑方向即马里奥方向，直接连续掷斧
    if (is_in_air) {
        throwAxe();
        return;
    }
    Scene* scene = getScene();
    if (!scene) return;

    // 找到马里奥，按水平距离选择动作
    GameObject* mario = nullptr;
    for (const auto& obj : scene->getGameObjects()) {
        if (obj && obj->getClassName() == "Mario") {
            mario = obj.get();
            break;
        }
    }
    if (!mario) return;

    // 面向马里奥（喷火/掷斧/跳扑方向以此为准）
    const float to_mario = mario->getPosition().x + mario->getSize().x * 0.5f
        - (this->position.x + this->getSize().x * 0.5f);
    facing_left = to_mario < 0.f;

    // 按距离加权随机：近身以喷火为主，中距以跳扑为主，远距以掷斧为主
    const float dist = std::abs(to_mario);
    float w_breath, w_jump, w_axe;
    if (dist < 200.f) {
        w_breath = 7.f; w_jump = 3.f; w_axe = 0.f;
    }
    else if (dist < 600.f) {
        w_breath = 3.f; w_jump = 5.f; w_axe = 2.f;
    }
    else {
        w_breath = 0.f; w_jump = 4.f; w_axe = 6.f;
    }

    std::uniform_real_distribution<float> roll(0.f, w_breath + w_jump + w_axe);
    const float pick = roll(decisionRng());
    if (pick < w_breath) startBreathing();
    else if (pick < w_breath + w_jump) jumpAction();
    else throwAxe();
}

void Bowser::jumpAction() {
    if (is_killed || is_in_air) return;
    const auto move = getComponent<MoveComponent>();
    if (!move) return;
    // 跳起并朝马里奥方向前压（落地由垂直碰撞恢复巡逻）
    move->setSpeedY(-CONFIG.game.jumpForce * BOWSER_JUMP_FORCE);
    move->setSpeedX((facing_left ? -1.f : 1.f) * patrol_speed_x * BOWSER_JUMP_CHASE);
    is_in_air = true;
    // 腾空期间决策心跳切到掷斧节奏：空中连续掷斧，落地后恢复
    decision_timer.start(BOWSER_AIR_DECISION_INTERVAL, true);
}

void Bowser::throwAxe() {
    if (is_killed) return;

    // 一次掷斧动作 = 连发 2~3 把（首把立即出手，后续按连发间隔逐发）
    std::uniform_int_distribution<int> burst(2, 3);
    axe_burst_left = burst(decisionRng());
    throwAxeOne();
}

void Bowser::throwAxeOne() {
    if (is_killed) return;
    Scene* scene = getScene();
    if (!scene) return;

    // 从头顶向马里奥方向抛出（抛物线+旋转，无视地形穿行，见 BowserAxe）
    const float dir = facing_left ? -1.f : 1.f;
    const float axe_x = this->position.x + this->getSize().x * 0.5f - CONFIG.game.defaultBlockSize * 0.5f;
    const float axe_y = this->position.y - CONFIG.game.defaultBlockSize * 0.3f;
    scene->addObject(std::make_shared<BowserAxe>(axe_x, axe_y, dir * BOWSER_AXE_SPEED));

    if (--axe_burst_left > 0) axe_timer.start(BOWSER_AXE_BURST_INTERVAL);
    else axe_timer.stop();
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
    // 跳扑腾空时不恢复速度（落地由垂直碰撞恢复）
    if (!is_in_air) {
        if (const auto move = getComponent<MoveComponent>())
            move->setSpeedX(facing_left ? -patrol_speed_x : patrol_speed_x);
    }
    shoot_timer.stop();
}

void Bowser::spawnFire() {
    if (is_killed) return;
    Scene* scene = getScene();
    if (!scene) return;

    // 火焰弹从嘴部生成：朝向前方一段距离、高度在身位 35%~70% 间随机（整体偏低，覆盖低空与跳跃之间的空档）
    const float dir = facing_left ? -1.f : 1.f;
    const float fire_x = facing_left
        ? this->position.x - CONFIG.game.defaultBlockSize * 1.6f
        : this->position.x + this->getSize().x + CONFIG.game.defaultBlockSize * 0.1f;
    std::uniform_real_distribution<float> fire_height(0.35f, 0.70f);
    const float fire_y = this->position.y + this->getSize().y * fire_height(decisionRng());
    scene->addObject(std::make_shared<BowserFire>(fire_x, fire_y, dir * BOWSER_FIRE_SPEED));
}
