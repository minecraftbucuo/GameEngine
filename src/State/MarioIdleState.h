//
// Created by MINEC on 2026/1/30.
//

#pragma once
#include "BaseState.h"
#include "Core/Types.h"

class MarioIdleState : public BaseState {
public:
    explicit MarioIdleState();
    ~MarioIdleState() override = default;

    void start() override;

    void update(const eng::Time& deltaTime) override;

    void handleEvent(const sf::Event& event) override;
#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif
    bool getIsLeft() const;

    void setIsLeft(bool value) const;

private:
#ifndef SERVER_BUILD
    sf::Sprite left_sprite;
    sf::Sprite right_sprite;
#endif
};
