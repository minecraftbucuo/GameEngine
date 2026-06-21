//
// Created by MINEC on 2026/6/2.
//
#ifndef SERVER_BUILD
#include "MenuScene.h"
#include "AssetManager.h"
#include "SuperMarioScene.h"
#include "Button.h"
#include "SceneManager.h"

MenuScene::MenuScene(sf::RenderWindow* _window) : Scene(_window, "MenuScene") {
    title.setString(L"GameEngine");
    title.setFillColor(sf::Color::Yellow);
    title.setFont(AssetManager::getInstance().getFont());
    title.setScale(3.f, 3.f);
    title.setPosition(_window->getSize().x * 0.5f - title.getGlobalBounds().width * 0.5f,
                      _window->getSize().y * 0.18f);
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
        // TODO: 进入设置界面
    });
}

void MenuScene::render(sf::RenderWindow* _window) {
    Scene::render(_window);
    _window->draw(title);
}
#endif