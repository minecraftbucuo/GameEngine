//
// Created by MINEC on 2025/12/9.
//
#pragma once
#include <memory>
#include "SceneManager.h"

class GameEngine {
public:
    GameEngine() = default;
    ~GameEngine();

    void init();
#ifndef SERVER_BUILD
    void start() const;
#else
    [[noreturn]] void start() const;
#endif

private:
    std::shared_ptr<SceneManager> scene_manager;
    sf::RenderWindow* window{};
};
