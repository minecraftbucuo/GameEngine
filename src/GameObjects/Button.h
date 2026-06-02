//
// Created by MINEC on 2026/2/19.
//

#pragma once
#ifndef SERVER_BUILD
#include <functional>

#include "GameObject.h"

class Button : public GameObject {
public:
    Button(float x, float y, float w, float h, const sf::String& button_text = "Button");
    void render(sf::RenderWindow* window) override;

    void handleEvent(sf::Event& event) override;

    bool isMouseOver() const;

    void setOnClick(std::function<void()>&& _onClick);

    void setToRectCenter(float x, float y, float w, float h);

    void runOnClick() const;

private:
    sf::RectangleShape shape;
    std::function<void()> onClick;
    sf::Text text;
    bool is_hover = false;
};
#endif
