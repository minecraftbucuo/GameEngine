//
// Created by MINEC on 2026/5/8.
//

#include "MarioController.h"

#include "GameObject.h"
#include "MoveComponent.h"
#include "StateMachine.h"
#include "ConfigManager.h"
#include "AssetManager.h"
#include "FireBall.h"
#include "Mario.h"
#include "MarioJumpState.h"
#include "NetworkManager.h"
#include "Scene.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#endif

namespace {
    // 按住跳跃时额外施加的上升加速度（可变跳跃高度）
    constexpr float HOLD_JUMP_ACCEL = 1815.f;
    // 按住跳跃的有效时长：超过之后按不按住都不再增加跳跃高度
    constexpr int HOLD_JUMP_WINDOW_MS = 500;
    // 起跳缓冲窗口（毫秒）：网络输入到达时若只差一点落地，允许延后这么久执行
    constexpr int JUMP_BUFFER_MS = 150;
}

MarioController::~MarioController() {
#ifndef SERVER_BUILD
    if (jump_track) MIX_DestroyTrack(jump_track);
    if (big_jump_track) MIX_DestroyTrack(big_jump_track);
    if (shoot_track) MIX_DestroyTrack(shoot_track);
#endif
}

void MarioController::start() {
#ifndef SERVER_BUILD
    // 常驻 track 绑定预解码音频（原 sf::Sound::setBuffer 的等价物）
    auto& am = AssetManager::getInstance();
    jump_track = MIX_CreateTrack(am.getMixer());
    if (jump_track) MIX_SetTrackAudio(jump_track, am.getSoundBuffer("small_jump"));
    big_jump_track = MIX_CreateTrack(am.getMixer());
    if (big_jump_track) MIX_SetTrackAudio(big_jump_track, am.getSoundBuffer("big_jump"));
    shoot_track = MIX_CreateTrack(am.getMixer());
    if (shoot_track) MIX_SetTrackAudio(shoot_track, am.getSoundBuffer("fireball"));
#endif
    jump_timer.setCallback([this]() -> void { this->w_is_pressed = false; });
    shoot_timer.setCallback([&]() -> void { could_shoot = true; });
}

void MarioController::handleEvent(const eng::EngineEvent& event) {
    if (!is_player) return;
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::A) {
            a_held = true;
            runLeft();
        }
        if (event.key == eng::Key::D) {
            d_held = true;
            runRight();
        }
        if (event.key == eng::Key::W || event.key == eng::Key::Space) {
            jump();
        }
        if (event.key == eng::Key::J) {
            shoot();
        }
    } else if (event.type == eng::EventType::KeyRelease) {
        if (event.key == eng::Key::A) {
            a_held = false;
            // 另一方向键仍按住时不立即停车，保持当前移动
            if (!d_held) stopRun();
        }
        if (event.key == eng::Key::D) {
            d_held = false;
            if (!a_held) stopRun();
        }
        if (event.key == eng::Key::W || event.key == eng::Key::Space) {
            w_is_pressed = false;
            auto* nm = owner->getScene()->getNetworkManager();
            if (nm && nm->isClient()) {
                eng::Packet packet;
                packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
                packet << InputType::JumpRelease;
                nm->getClientSocket().append(packet);
            }
        }
    }
}

void MarioController::update(const eng::Time& deltaTime) {
    // 起跳缓冲：网络输入在"还差一两帧才落地"时先存着，落地立刻执行。
    // 不做这个缓冲，服务端会把这次起跳丢掉，而客户端本地已经跳了，
    // 随后到达的服务端快照会把角色强行拉回地面。
    if (jump_buffered) {
        jump_buffer_timer.update(deltaTime);
        if (!jump_buffer_timer.isActive()) {
            jump_buffered = false;
        } else if (canJumpNow()) {
            jump_buffered = false;
            jump(jump_buffer_play_sound);
        }
    }

    // 按住时长从起跳开始算，且不依赖当前状态：落地后状态会切回 Idle，
    // 若只在跳跃状态里计时，计时器会永远停住，w_is_pressed 就一直挂着。
    if (w_is_pressed) {
        jump_timer.update(deltaTime);
    }

    // 踩在地面上时不施加按住推力。落地那一刻碰撞已经把重力关掉了，
    // 这点残余推力没有任何东西抵消，会把马里奥顶离地面 1px 以上；
    // needGravity() 随即返回 true、状态机被切回 MarioJumpState，
    // 服务端于是认为玩家还在空中，接下来收到的跳跃输入全被状态闸门丢掉
    //（表现为：本地跳成功，服务端不跳，快照把角色拉回地面）。
    const auto* mario = dynamic_cast<const Mario*>(owner);
    const bool grounded = mario && mario->isOnGround();
    if (w_is_pressed && !grounded) {
        if (const auto& move_component = owner->getComponent<MoveComponent>()) {
            move_component->addSpeed(eng::Vec2f(0.f, -HOLD_JUMP_ACCEL * deltaTime.asSeconds()));
        }
    }

    shoot_timer.update(deltaTime);
}

// 此刻能不能起跳：只看物理（踩在实体上、且没有在上升），不看状态机名字。
// 状态名是渲染/动画的产物：服务端刚落地那一帧、或被残余推力顶起来的瞬间，
// 名字都可能还是 MarioJumpState，用它当闸门会吞掉合法的起跳输入。
bool MarioController::canJumpNow() {
    const auto state = owner->getComponent<StateMachine>();
    if (!state) return false;
    if (state->getCurrentStateName() == "MarioDeadState") return false;
    // 正在上升说明已经在跳跃中，禁止二段跳
    if (owner->getSpeed().y < 0.f) return false;
    if (auto* mario = dynamic_cast<Mario*>(owner)) return !mario->needGravity();
    return state->getCurrentStateName() != "MarioJumpState";
}

// play_sound 只在非 SERVER_BUILD 分支（音效播放）里使用，服务端构建下靠
// jump_buffer_play_sound 的赋值保住引用，不会触发未使用告警。
void MarioController::jump(const bool play_sound, const bool allow_buffer) {
    if (!canJumpNow()) {
        // 网络输入：此刻还差一点落地，先缓存，落地后立刻补上（见 update）
        if (allow_buffer) {
            jump_buffered = true;
            jump_buffer_play_sound = play_sound;
            jump_buffer_timer.start(JUMP_BUFFER_MS);
        }
        return;
    }
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedY(-CONFIG.game.jumpForce);
#ifndef SERVER_BUILD
    if (play_sound) {
        // 大小马里奥跳跃音效不同：按当前形态选 track
        MIX_Track* track = jump_track;
        if (const auto mario = dynamic_cast<Mario*>(owner); mario && mario->getIsBig())
            track = big_jump_track;
        if (track) { MIX_StopTrack(track, 0); MIX_PlayTrack(track, 0); }
        LOG_TRACE("jump sound play!");
    }
#endif
    owner->getComponent<StateMachine>()->setState("MarioJumpState");
    w_is_pressed = true;
    jump_timer.start(HOLD_JUMP_WINDOW_MS);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        eng::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::Jump;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::runLeft() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(-CONFIG.game.playerSpeed);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        eng::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunLeft;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::runRight() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(CONFIG.game.playerSpeed);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        eng::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::RunRight;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::stopRun() const {
    std::shared_ptr<MoveComponent> moveComponent = owner->getComponent<MoveComponent>();
    if (!moveComponent) {
        moveComponent = owner->addComponent<MoveComponent>();
    }
    moveComponent->setSpeedX(0.f);

    auto* nm = owner->getScene()->getNetworkManager();
    if (nm && nm->isClient()) {
        eng::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::StopRun;
        nm->getClientSocket().append(packet);
    }
}

void MarioController::setIsPlayer(const bool flag) {
    is_player = flag;
}

void MarioController::setWisPressed(const bool flag) {
    w_is_pressed = flag;
}

// 同 jump()：play_sound 仅非 SERVER_BUILD 分支使用
void MarioController::shoot([[maybe_unused]] const bool play_sound) {
    // 小时候不能发射炮弹：只有吃蘑菇长大后才能射击（本地按键和服务端转发输入都会经过这里）
    if (const auto mario = dynamic_cast<Mario*>(owner); mario && !mario->getIsBig()) return;
    if (!could_shoot) return;
    could_shoot = false;
    shoot_timer.start(CONFIG.game.shootDelay);
#ifndef SERVER_BUILD
    if (play_sound) {
        if (shoot_track) { MIX_StopTrack(shoot_track, 0); MIX_PlayTrack(shoot_track, 0); }
        LOG_TRACE("shoot sound play!");
    }
#endif
    auto* network_manager = owner->getScene()->getNetworkManager();
    const auto current_scene = owner->getScene();
    if (network_manager && network_manager->isClient()) {
        eng::Packet packet;
        packet << NetworkMsg::ClientInput << NetworkMsg::ClientInput;
        packet << InputType::Shoot;
        network_manager->getClientSocket().append(packet);
        return;
    }
    if (owner->getComponent<StateMachine>()->getIsLeft()) {
        current_scene->addObjectWithNetwork(std::make_shared<FireBall>(owner->getId(),
            owner->getPosition().x - 32.f, owner->getPosition().y, -600.f));
    }
    else {
        current_scene->addObjectWithNetwork(std::make_shared<FireBall>(owner->getId(), owner->getPosition().x +
            owner->getComponent<Collision>()->getOffset().x + owner->getSize().x, owner->getPosition().y, 600.f));
    }
}