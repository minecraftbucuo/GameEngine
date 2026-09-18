//
// Created by MINEC on 2026/1/29.
//

#pragma once

#include "Animation.h"
#include "BaseState.h"
#include "Core/Types.h"

class MarioRunState : public BaseState {
public:
    explicit MarioRunState();
    ~MarioRunState() override = default;

    void start() override;

    void update(const eng::Time& deltaTime) override;

    void handleEvent(const eng::EngineEvent& event) override;

#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

    bool getIsLeft() const;

    void setIsLeft(const bool value) const;
#ifndef SERVER_BUILD
    Animation& getAnimation();
#endif
private:
#ifndef SERVER_BUILD
    Animation animation_right;
    Animation animation_left;

    // 按当前形态重新指向动画帧（吃蘑菇长大后切到大马里奥帧组）
    void applyForm();
#endif

    bool isBigForm() const;
};
