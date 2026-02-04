//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "GameScene.h"
#include "Scene.h"
#include "GameScene3D.h"
#include "SuperMarioScene.h"


class GameEngine {
public:
    GameEngine() = default;
    ~GameEngine() {
        delete window;
    }

    void init() {
        window = new sf::RenderWindow(sf::VideoMode(1200, 960), "GameEngine");
        loadScene<GameScene>(window);
        // loadScene<GameScene3D>(window);
        // loadScene<SuperMarioScene>(window);
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
                currentScene->handleEvent(event);
            }

            currentScene->update(deltaTime);
            window->clear();
            currentScene->render(window);
            window->display();
        }
    }

    template<typename T, typename... Args>
    void loadScene(Args&&... args) {
        loadScene(std::make_shared<T>(std::forward<Args>(args)...));
    }

    void loadScene(std::shared_ptr<Scene> scene) {
        currentScene = std::move(scene);
        currentScene->init();
    }

private:
    std::shared_ptr<Scene> currentScene;
    sf::RenderWindow* window{};
};

