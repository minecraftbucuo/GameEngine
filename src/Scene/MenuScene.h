//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <string>
#include <vector>
#include "Scene.h"
#include "Render/Handles.h"

class Button;

class MenuScene : public Scene {
public:
    explicit MenuScene(eng::Renderer* _renderer);
    ~MenuScene() override = default;

    void init() override;

    void initScene();

    void handleEvent(const eng::EngineEvent& event) override;

    void update(eng::Time deltaTime) override;

    void render(eng::Renderer& _renderer) override;

private:
    // 标题（SDL3 迁移 6d：sf::Text 数据化，渲染时经 measureText 居中）
    eng::FontHandle font;
    std::string titleText = "GameEngine";
    static constexpr unsigned TITLE_FONT_SIZE = 90;   // = 迁移前默认 30 号字 × setScale(3)
    eng::Vec2f titlePos;

    // 背景粒子（SDL3 迁移 6d：sf::CircleShape 数据化）
    struct Particle {
        eng::Vec2f pos;
        float radius;
        eng::Vec2f velocity;
        float alpha;
        float alphaSpeed;
    };
    std::vector<Particle> particles;

    // 按钮引用：自适应视口宽度变化后需按新视口重新居中
    std::vector<std::shared_ptr<Button>> buttons;

    void relayout();
};
#endif
