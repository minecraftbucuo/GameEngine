//
// Created by MINEC on 2026/3/14.
//

#pragma once

#include "BoxGameObject.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Render/Handles.h"
#endif

class Brick : public BoxGameObject {
public:
    Brick(float x, float y, const std::string& tag = "brick");

    void setPosition(float posX, float posY) override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override {
        BoxGameObject::render(renderer);
        // 原砖块贴图：tile_set (16,0,16,16) 区域，4 倍放大
        renderer.drawTexture(texture,
                             eng::FloatRect(16.f, 0.f, 16.f, 16.f),
                             eng::FloatRect(getPosition(), eng::Vec2f(64.f, 64.f)));
    }
#endif
private:
#ifndef SERVER_BUILD
    // SDL3 迁移 6c：精灵数据化（原 sf::Sprite）
    eng::TextureHandle texture;
#endif
};
