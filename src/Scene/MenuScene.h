//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Scene.h"

class MenuScene : public Scene {
public:
    explicit MenuScene(sf::RenderWindow* _window);
    ~MenuScene() override = default;

    void init() override;

    void initScene();

    void update(eng::Time deltaTime) override;

    void render(sf::RenderWindow* _window) override;

private:
    sf::Text title;

    struct Particle {
        sf::CircleShape shape;
        eng::Vec2f velocity;
        float alpha;
        float alphaSpeed;
    };
    std::vector<Particle> particles;
};
#endif
