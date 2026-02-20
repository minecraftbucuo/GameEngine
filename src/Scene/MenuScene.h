//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Button.h"
#include "Scene.h"
#include "SuperMarioScene.h"

class MenuScene : public Scene {
public:
    explicit MenuScene(sf::RenderWindow* _window) : Scene(_window, "MenuScene") {
        title.setString(L"Menu");
        title.setFillColor(sf::Color::Yellow);
        title.setFont(AssetManager::getInstance().getFont());
        title.setScale(2.f, 2.f);
        // 让标题居中
        title.setPosition(_window->getSize().x * 0.5f - title.getGlobalBounds().width * 0.5f,
            window->getSize().y * 0.15f - title.getGlobalBounds().height * 0.5f);
    }
    ~MenuScene() override = default;

    void init() override {
        Scene::init();
        initScene();
    }

    void initScene() {
        std::shared_ptr<Button> button1 = std::make_shared<Button>(100, 100, 250, 50, L"超级玛丽Client");
        button1->setOnClick([&]() -> void {
            SceneContext::getInstance().getSceneManager()->loadScene("SuperMarioScene");
            std::dynamic_pointer_cast<SuperMarioScene>(SceneContext::getInstance().getSceneManager()->getCurrentScene())->connectToServer("127.0.0.1");
        });
        std::shared_ptr<Button> button1_ = std::make_shared<Button>(100, 100, 250, 50, L"超级玛丽Server");
        button1_->setOnClick([&]() -> void {
            SceneContext::getInstance().getSceneManager()->loadScene("SuperMarioScene");
            std::dynamic_pointer_cast<SuperMarioScene>(SceneContext::getInstance().getSceneManager()->getCurrentScene())->startServer();
        });
        auto render_window = SceneContext::getInstance().getWindow();
        button1->setToRectCenter(0, render_window->getSize().y * 0.25f, render_window->getSize().x * 0.5f, render_window->getSize().y * 0.25f);
        this->addObject(button1);
        button1_->setToRectCenter(render_window->getSize().x * 0.5f, render_window->getSize().y * 0.25f, render_window->getSize().x * 0.5f, render_window->getSize().y * 0.25f);
        this->addObject(button1_);

        std::shared_ptr<Button> button2 = std::make_shared<Button>(100, 100, 200, 50, L"3D渲染");
        button2->setOnClick([&]() -> void {
            SceneContext::getInstance().getSceneManager()->loadScene("GameScene3D");
        });
        button2->setToRectCenter(0, render_window->getSize().y * 0.5f, render_window->getSize().x, render_window->getSize().y * 0.25f);
        this->addObject(button2);

        std::shared_ptr<Button> button3 = std::make_shared<Button>(100, 100, 200, 50, L"Demo");
        button3->setOnClick([&]() -> void {
            SceneContext::getInstance().getSceneManager()->loadScene("GameScene");
        });
        button3->setToRectCenter(0, render_window->getSize().y * 0.75f, render_window->getSize().x, render_window->getSize().y * 0.25f);
        this->addObject(button3);
    }

    void render(sf::RenderWindow* _window) override {
        Scene::render(_window);
        _window->draw(title);
    }

private:
    sf::Text title;
};
