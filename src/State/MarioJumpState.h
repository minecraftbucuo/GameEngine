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

    void update(const eng::Time& deltaTime) override {
        if (owner->getSpeed().x < 0) {
            setIsLeft(true);
        } else if (owner->getSpeed().x > 0) {
            setIsLeft(false);
        }
        const auto& box_collision = owner->getComponent<Collision, BoxCollision>();
        if (!getIsLeft()) {
            box_collision->setOffset(eng::Vec2f(16.f, 0.f));
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
        constexpr float scale_x = 4.f, scale_y = 4.f;
        const eng::Vec2f size(scale_x * static_cast<float>(texture_rect.width),
                              scale_y * static_cast<float>(texture_rect.height));
        renderer.drawTexture(texture,
                             eng::FloatRect(static_cast<float>(texture_rect.left),
                                            static_cast<float>(texture_rect.top),
                                            static_cast<float>(texture_rect.width),
                                            static_cast<float>(texture_rect.height)),
                             eng::FloatRect(owner->getPosition(), size),
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
};
