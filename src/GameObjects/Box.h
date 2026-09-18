//
// Created by MINEC on 2026/3/18.
//

#pragma once
#include "Animation.h"
#include "BoxGameObject.h"
#include "Core/Types.h"

class Box : public BoxGameObject {
public:
    Box(float x, float y, const std::string& tag = "box");

    void start() override;

    void update(eng::Time deltaTime) override;

    void setPosition(float posX, float posY) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif

private:
#ifndef SERVER_BUILD
    Animation animation;
#endif
    float last_y;
    // 首次被顶：变暗并生成蘑菇（只发生一次；联机模式不做）
    bool has_spawned = false;
#ifndef SERVER_BUILD
    // 碰撞回调里只记标记，真正改场景延迟到 update（碰撞派发是同步的）
    bool pending_spawn = false;
#endif
};
