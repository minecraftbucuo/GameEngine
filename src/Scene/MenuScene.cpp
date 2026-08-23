//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "MenuScene.h"
#include "AssetManager.h"
#include "SuperMarioScene.h"
#include "Button.h"
#include "SceneManager.h"
#include <random>

MenuScene::MenuScene(eng::Renderer* _renderer) : Scene(_renderer, "MenuScene") {
    title.setString(L"GameEngine");
    title.setFillColor(eng::Color::Yellow);
    title.setFont(AssetManager::getInstance().getFont());
    title.setScale(3.f, 3.f);
    title.setPosition(_renderer->getSize().x * 0.5f - title.getGlobalBounds().width * 0.5f,
                      _renderer->getSize().y * 0.18f);
}

void MenuScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;
    initScene();
}

void MenuScene::initScene() {
    const float winW = static_cast<float>(window->getSize().x);
    const float winH = static_cast<float>(window->getSize().y);
    const float btnW = 280.f;
    const float btnH = 55.f;
    const float startY = winH * 0.45f;
    const float spacing = 35.f;

    auto makeButton = [&](const sf::String& label, int index, auto&& callback) {
        auto btn = std::make_shared<Button>(0, 0, btnW, btnH, label);
        btn->setOnClick(std::forward<decltype(callback)>(callback));
        btn->setToRectCenter(0, startY + index * (btnH + spacing), winW, btnH);
        this->addObject(btn);
    };

    makeButton(L"超级玛丽 Client", 0, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->connectToServer(
            CONFIG.network.serverIp);
    });

    makeButton(L"超级玛丽 Server", 1, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->startServer();
    });

    makeButton(L"3D 渲染", 2, [&]() -> void {
        getSceneManager()->loadScene("GameScene3D");
    });

    makeButton(L"Demo", 3, [&]() -> void {
        getSceneManager()->loadScene("GameScene");
    });

    makeButton(L"设置", 4, [&]() -> void {
        getSceneManager()->loadScene("SettingsScene");
    });

    makeButton(L"物理测试", 5, [&]() -> void {
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
        float r = distR(rng);
        p.shape.setRadius(r);
        p.shape.setPosition(distX(rng), distY(rng));
        p.shape.setFillColor(eng::Color(130, 200, 255, static_cast<sf::Uint8>(distAlpha(rng))));
        p.velocity = {distV(rng), distV(rng)};
        p.alpha = distAlpha(rng);
        p.alphaSpeed = distAlphaSpeed(rng);
        particles.push_back(std::move(p));
    }
}

void MenuScene::update(eng::Time deltaTime) {
    Scene::update(deltaTime);

    const float winW = static_cast<float>(window->getSize().x);
    const float winH = static_cast<float>(window->getSize().y);
    const float dt = deltaTime.asSeconds();

    for (auto& p : particles) {
        p.shape.move(p.velocity * dt);

        // 呼吸效果：alpha 缓慢变化
        p.alpha += p.alphaSpeed * dt;
        if (p.alpha > 120.f || p.alpha < 20.f) p.alphaSpeed = -p.alphaSpeed;
        eng::Color c = p.shape.getFillColor();
        c.a = static_cast<sf::Uint8>(std::clamp(p.alpha, 0.f, 255.f));
        p.shape.setFillColor(c);

        // 超出边界则从另一侧进入
        eng::Vec2f pos = p.shape.getPosition();
        if (pos.x < -10.f) pos.x = winW;
        else if (pos.x > winW + 10.f) pos.x = -10.f;
        if (pos.y < -10.f) pos.y = winH;
        else if (pos.y > winH + 10.f) pos.y = -10.f;
        p.shape.setPosition(pos);
    }
}

void MenuScene::render(sf::RenderWindow* _window) {
    for (const auto& p : particles) {
        _window->draw(p.shape);
    }
    Scene::render(_window);
    _window->draw(title);
}
#endif