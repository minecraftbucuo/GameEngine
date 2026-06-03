//
// Created by MINEC on 2026/1/29.
//

#pragma once

#include "Animation.h"
#include "BaseState.h"

class MarioRunState : public BaseState {
public:
    explicit MarioRunState();
    ~MarioRunState() override = default;

    void update(const sf::Time& deltaTime) override;

    void handleEvent(const sf::Event& event) override;

#ifndef SERVER_BUILD
    void render(sf::RenderWindow* window) override;
#endif

    bool getIsLeft() const;

    void setIsLeft(const bool value) const;
#ifndef SERVER_BUILD
    Animation& getAnimation();
#endif
private:
#ifndef SERVER_BUILD
    Animation animation_right;
    Animation animation_left;
#endif
};
