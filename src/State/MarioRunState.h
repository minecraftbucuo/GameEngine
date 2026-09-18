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

    // 按给定形态重新指向动画帧（长大切大帧组；变身闪烁按相位在两套帧组间切换）
    void applyForm(bool big);

    // 当前帧组形态：-1 未设置，0 小，1 大。只在形态变化时才 setFrames，
    // 避免每帧重置动画播放进度。
    int applied_form = -1;
#endif

    bool isBigForm() const;
};
