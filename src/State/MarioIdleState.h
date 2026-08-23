//
// Created by MINEC on 2026/1/30.
//

#pragma once
#include "BaseState.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Handles.h"
#endif

class MarioIdleState : public BaseState {
public:
    explicit MarioIdleState();
    ~MarioIdleState() override = default;

    void start() override;

    void update(const eng::Time& deltaTime) override;

    void handleEvent(const eng::EngineEvent& event) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    bool getIsLeft() const;

    void setIsLeft(bool value) const;

private:
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：精灵数据化（原 sf::Sprite ×2），方向由 render 的 flipX 表达
    eng::TextureHandle texture;
    eng::IntRect texture_rect;
    eng::Vec2f scale {4.f, 4.f};
#endif
};
