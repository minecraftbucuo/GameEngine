//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <SFML/Graphics.hpp>
#include <memory>
#include "World.h"
#include "Player.h"

#include "Ground.h"
#include "Collision.h"
#include "BoxCollision.h"


class GameEngine {
public:
    GameEngine(World* world, sf::RenderWindow* window) {
        this->world = world;
        this->window = window;
    }
    GameEngine() = default;

    void init() {
        window = new sf::RenderWindow(sf::VideoMode(1200, 960), "GameEngine");
        world = new World(1200, 960);
        world->setWorldContext();
        std::shared_ptr<Player> player = std::make_shared<Player>(100, 100, 20);
        player->addComponent<Controller>();
        world->addObject(player);

        // for (int i = 1; i <= 9; i += 2) {
        //     std::shared_ptr<Player> player = std::make_shared<Player>(i * 100, 0, 20);
        //     if (i == 3) {
        //         player->addComponent<Controller>();
        //     }
        //     world->addObject(player);
        // }

        // std::shared_ptr<Ground> ground = std::make_shared<Ground>(0, 960, 1200, 1200, "ground");
        std::shared_ptr<Ground> ground = std::make_shared<Ground>(20, 600, 600, 20);
        ground->setTag("111");
        world->addObject(ground);
    }

    void start() {
        window->setFramerateLimit(165);
        sf::Clock clock;
        while (window->isOpen()) {
            const sf::Time deltaTime = clock.restart();
            sf::Event event{};
            while (window->pollEvent(event)) {
                if (event.type == sf::Event::Closed) {
                    window->close();
                }
                if (event.type == sf::Event::Resized) {
                    // 处理窗口大小改变引起的缩放问题
                    window->setView(sf::View(sf::FloatRect(0, 0,
                        static_cast<float>(event.size.width), static_cast<float>(event.size.height))));
                    world->setWorldSize(static_cast<float>(event.size.width), static_cast<float>(event.size.height));
                    world->setWorldContext();
                }
                world->handleEvent(event);
            }

            world->update(deltaTime);
            window->clear();
            world->render(window);
            window->display();
        }
    }

private:
    World* world;
    sf::RenderWindow* window;
};

