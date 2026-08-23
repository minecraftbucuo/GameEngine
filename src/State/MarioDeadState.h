//
// Created by MINEC on 2026/5/14.
//

#pragma once
#include "BaseState.h"
#include "Timer.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Handles.h"
#endif

class MarioDeadState : public BaseState {
public:
    MarioDeadState();

    void start() override;

    void update(const eng::Time& deltaTime) override;

#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

private:
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：精灵数据化（原 sf::Sprite），死亡帧无方向翻转
    eng::TextureHandle texture;
    eng::IntRect texture_rect;
    eng::Vec2f scale {4.f, 4.f};
#endif
    Timer deathTimer;
};
