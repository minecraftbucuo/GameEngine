//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "MenuScene.h"
#include "AssetManager.h"
#include "SuperMarioScene.h"
#include "Button.h"
#include "Render/Renderer.h"
#include "SceneManager.h"
#include <algorithm>
#include <random>

MenuScene::MenuScene(eng::Renderer* _renderer) : Scene(_renderer, "MenuScene") {
    font = AssetManager::getInstance().getFontHandle();
    const eng::Vec2f titleSize = _renderer->measureText(font, titleText, TITLE_FONT_SIZE);
    titlePos = eng::Vec2f(_renderer->getSize().x * 0.5f - titleSize.x * 0.5f,
                          static_cast<float>(_renderer->getSize().y) * 0.18f);
}

void MenuScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;
    initScene();
}

void MenuScene::initScene() {
    const float winW = static_cast<float>(renderer->getSize().x);
    const float winH = static_cast<float>(renderer->getSize().y);
    const float btnW = 280.f;
    const float btnH = 55.f;
    const float startY = winH * 0.45f;
    const float spacing = 35.f;

    auto makeButton = [&](const std::string& label, int index, auto&& callback) {
        auto btn = std::make_shared<Button>(0, 0, btnW, btnH, label);
        btn->setOnClick(std::forward<decltype(callback)>(callback));
        btn->setToRectCenter(0, startY + index * (btnH + spacing), winW, btnH);
        this->addObject(btn);
    };

    makeButton("超级玛丽 Client", 0, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->connectToServer(
            CONFIG.network.serverIp);
    });

    makeButton("超级玛丽 Server", 1, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->startServer();
    });

    makeButton("3D 渲染", 2, [&]() -> void {
        getSceneManager()->loadScene("GameScene3D");
    });

    makeButton("Demo", 3, [&]() -> void {
        getSceneManager()->loadScene("GameScene");
    });

    makeButton("设置", 4, [&]() -> void {
        getSceneManager()->loadScene("SettingsScene");
    });

    makeButton("物理测试", 5, [&]() -> void {
        getSceneManager()->loadScene("PhysicsTestScene");
    });

    // 初始化背景粒子
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(0.f, winW);
    std::uniform_real_distribution<float> distY(0.f, winH);
    std::uniform_real_distribution<float> distV(-20.f, 20.f);
    std::uniform_real_distribution<float> distR(3.f, 10.f);
    std::uniform_real_distribution<float> distAlpha(60.f, 180.f);
    std::uniform_real_distribution<float> distAlphaSpeed(10.f, 30.f);

    for (int i = 0; i < 80; ++i) {
        Particle p;
        p.radius = distR(rng);
        p.pos = {distX(rng), distY(rng)};
        p.velocity = {distV(rng), distV(rng)};
        p.alpha = distAlpha(rng);
        p.alphaSpeed = distAlphaSpeed(rng);
        particles.push_back(p);
    }
}

void MenuScene::update(eng::Time deltaTime) {
    Scene::update(deltaTime);

    const float winW = static_cast<float>(renderer->getSize().x);
    const float winH = static_cast<float>(renderer->getSize().y);
    const float dt = deltaTime.asSeconds();

    for (auto& p : particles) {
        p.pos += p.velocity * dt;

        // 呼吸效果：alpha 缓慢变化
        p.alpha += p.alphaSpeed * dt;
        if (p.alpha > 120.f || p.alpha < 20.f) p.alphaSpeed = -p.alphaSpeed;
        p.alpha = std::clamp(p.alpha, 0.f, 255.f);

        // 超出边界则从另一侧进入
        if (p.pos.x < -10.f) p.pos.x = winW;
        else if (p.pos.x > winW + 10.f) p.pos.x = -10.f;
        if (p.pos.y < -10.f) p.pos.y = winH;
        else if (p.pos.y > winH + 10.f) p.pos.y = -10.f;
    }
}

void MenuScene::render(eng::Renderer& _renderer) {
    // 背景粒子（呼吸 alpha，颜色与迁移前一致）
    for (const auto& p : particles) {
        _renderer.drawCircle(p.pos, p.radius, eng::Color(130, 200, 255,
                                static_cast<eng::Uint8>(p.alpha)));
    }
    renderObjects(_renderer);
    _renderer.drawText(font, titleText, titlePos, TITLE_FONT_SIZE, eng::Color::Yellow);
}
#endif
