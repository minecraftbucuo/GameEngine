//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "GameScene.h"
#include "Scene.h"
#include "GameScene3D.h"
#include "MenuScene.h"
#include "SceneManager.h"
#include "SuperMarioScene.h"


class GameEngine {
public:
    GameEngine() = default;
    ~GameEngine() {
        delete window;
    }

    void init() {
        window = new sf::RenderWindow(sf::VideoMode(1200, 960), "GameEngine");
        scene_manager = std::make_shared<SceneManager>();
        scene_manager->addScene<GameScene>(window);
        scene_manager->addScene<GameScene3D>(window);
        scene_manager->addScene<SuperMarioScene>(window);
        scene_manager->addScene<MenuScene>(window);
        scene_manager->loadScene("MenuScene");
    }

    void start() const {
        window->setFramerateLimit(165);
        sf::Clock clock;
        while (window->isOpen()) {
            const sf::Time deltaTime = clock.restart();
            sf::Event event{};
            while (window->pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window->close();
                }
                scene_manager->handleEvent(event);
            }

            scene_manager->update(deltaTime);
            window->clear();
            scene_manager->render(window);
            window->display();
        }
    }

private:
    std::shared_ptr<SceneManager> scene_manager;
    sf::RenderWindow* window{};
};

