//
// Created by MINEC on 2026/1/30.
//

#pragma once
#ifndef SERVER_BUILD
#include "AssetManager.h"
#include "Render/Renderer.h"
#endif
#include "BaseState.h"
#include "BoxCollision.h"
#include "Collision.h"
#include "GameObject.h"
#include "Mario.h"
#include "StateMachine.h"
#include "GravityComponent.h"
#include "Timer.h"
#include "MoveComponent.h"
#include "Core/Types.h"
#include <cmath>


class MarioJumpState : public BaseState {
public:
    MarioJumpState() : BaseState("MarioJumpState") {
#ifndef SERVER_BUILD
        // 原 sf::Sprite 配置：mario_bros (144,32,16,16) 区域，4 倍放大，左向为镜像
        texture = AssetManager::getInstance().getTextureHandle("mario_bros");
        texture_rect = eng::IntRect(144, 32, 16, 16);
#endif
    }
    ~MarioJumpState() override = default;

    void start() override {
        // 按当前形态选择跳跃贴图：小马里奥 (144,32,16,16)，大马里奥 (144,0,16,32)
        if (const auto mario = dynamic_cast<Mario*>(owner); mario && mario->getIsBig()) {
            texture_rect = eng::IntRect(144, 0, 16, 32);
        } else {
            texture_rect = eng::IntRect(144, 32, 16, 16);
        }
    }

    void update(const eng::Time& deltaTime) override {
        if (owner->getSpeed().x < 0) {
            setIsLeft(true);
        } else if (owner->getSpeed().x > 0) {
            setIsLeft(false);
        }
        const auto& box_collision = owner->getComponent<Collision, BoxCollision>();
        if (!getIsLeft()) {
            // 大马里奥碰撞盒与精灵同宽，不再向右偏移
            box_collision->setOffset(eng::Vec2f(isBigForm() ? 0.f : 16.f, 0.f));
        } else {
            box_collision->setOffset(eng::Vec2f(0.f, 0.f));
        }
    }
#ifndef SERVER_BUILD
    void handleEvent(const eng::EngineEvent& event) override {
        if (event.type == eng::EventType::KeyPress) {
            if (event.key == eng::Key::A) {
                setIsLeft(true);
            } else if (event.key == eng::Key::D) {
                setIsLeft(false);
            }
        }
    }

    void render(eng::Renderer& renderer) override {
        if (!owner) {
            LOG_ERROR("Owner is nullptr");
            return;
        }
        // 变身闪烁（原版变身过程）：按相位在大小形态间交替展示
        bool show_big = false;
        if (const auto mario = dynamic_cast<Mario*>(owner)) {
            show_big = mario->getIsBig();
            if (mario->isTransforming())
                show_big = show_big != (mario->getTransformPhase() % 2 == 1);
        }
        const eng::IntRect rect = show_big ? eng::IntRect(144, 0, 16, 32) : eng::IntRect(144, 32, 16, 16);
        constexpr float scale_x = 4.f, scale_y = 4.f;
        const eng::Vec2f size(scale_x * static_cast<float>(rect.width),
                              scale_y * static_cast<float>(rect.height));
        // 正常渲染保持原版画法：精灵从盒子左上角直接向下画。
        // 跳跃帧 64 宽、盒子 48 宽，若无条件水平居中会让精灵整体横移 8px，
        // 看起来碰撞接触点对不上。只有变身闪烁期间才贴脚底、水平居中——
        // 此时显示形态与盒子尺寸不一致，不锚定会伸到脚底之下。
        eng::Vec2f pos = owner->getPosition();
        if (const auto mario = dynamic_cast<Mario*>(owner); mario && mario->isTransforming()) {
            pos.x += (owner->getSize().x - size.x) * 0.5f;
            pos.y += owner->getSize().y - size.y;
        }
        renderer.drawTexture(texture,
                             eng::FloatRect(static_cast<float>(rect.left),
                                            static_cast<float>(rect.top),
                                            static_cast<float>(rect.width),
                                            static_cast<float>(rect.height)),
                             eng::FloatRect(pos, size),
                             0.f, {}, eng::Color::White, getIsLeft());
    }
#endif
    bool getIsLeft() const {
        return owner->getComponent<StateMachine>()->getIsLeft();
    }

    void setIsLeft(const bool value) const {
        owner->getComponent<StateMachine>()->setIsLeft(value);
    }

private:
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：精灵数据化（原 sf::Sprite ×2），方向由 render 的 flipX 表达
    eng::TextureHandle texture;
    eng::IntRect texture_rect;
#endif

    bool isBigForm() const {
        const auto* mario = dynamic_cast<const Mario*>(owner);
        return mario && mario->getIsBig();
    }
};
