//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <SFML/Graphics.hpp>
#include "NetworkGameObject.h"
#include "Events.h"
#include <SFML/Audio.hpp>

class Mario : public NetworkGameObject {
public:
    Mario(float x, float y, bool isPlayer = true);

    ~Mario() override;

    void start() override;

    void handleEvent(sf::Event& e) override;

    void update(sf::Time deltaTime) override;

    bool needGravity();

    void handleCollision(const CollisionEvent& event);

    bool getIsPlayer() const;

    void destroy() override;

    sf::Vector2f getCenter() override;

    void serialize(sf::Packet& packet, NetworkMsg type) override;

    void deserialize(sf::Packet& packet) override;

private:
    bool isPlayer = true;
};
