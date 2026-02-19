//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Button.h"
#include "Scene.h"

class MenuScene : public Scene {
public:
    explicit MenuScene(sf::RenderWindow* _window) : Scene(_window, "MenuScene") {}
    ~MenuScene() override = default;

    void init() override {
        Scene::init();
        initScene();
    }

    void initScene() {
        std::shared_ptr<Button> button = std::make_shared<Button>(100, 100, 200, 50, L"开始游戏");
        button->setOnClick([&]() -> void {
           SceneContext::getInstance().getSceneManager()->loadScene("SuperMarioScene");
        });
        this->addObject(button);
    }
};