//
// Created by MINEC on 2026/2/19.
//

#pragma once
#include <functional>

#include "GameObject.h"
#include "AssetManager.h"
#include "SceneContext.h"

class Button : public GameObject {
public:
    Button(const float x, const float y, const float w, const float h, const sf::String& button_text = "Button") {
        shape = sf::RectangleShape(sf::Vector2f(w, h));
        shape.setPosition(x, y);
        text.setFont(AssetManager::getInstance().getFont());
        text.setString(button_text);
        const float scale = std::min(h * 0.7f / text.getLocalBounds().height, w * 0.7f / text.getLocalBounds().width);
        text.setScale(scale, scale);
        text.setFillColor(sf::Color::Black);
        text.setPosition(x + (w - text.getGlobalBounds().width) * 0.5f, y + (h - text.getGlobalBounds().height) * 0.5f);
        this->tag = "Button:" + std::to_string(this->id);
        className = "Button";
    }
    void render(sf::RenderWindow* window) override {
        if (is_hover) {
            shape.setFillColor(sf::Color::Green);
        } else {
            shape.setFillColor(sf::Color::Blue);
        }
        window->draw(shape);
        window->draw(text);
    }

    void handleEvent(sf::Event& event) override {
        if (event.type == sf::Event::MouseMoved) {
            if (isMouseOver()) {
                is_hover = true;
            } else {
                is_hover = false;
            }
        } else if (event.type == sf::Event::MouseButtonPressed) {
            if (is_hover && onClick) onClick();
        }
    }

    bool isMouseOver() const {
        const sf::FloatRect buttonBounds = shape.getGlobalBounds();
        const auto mouse_pos = SceneContext::getInstance().getMousePosition();
        return buttonBounds.contains(static_cast<float>(mouse_pos.x), static_cast<float>(mouse_pos.y));
    }

    void setOnClick(std::function<void()>&& _onClick) {
        this->onClick = std::move(_onClick);
    }

    void setToRectCenter(const float x, const float y, const float w, const float h) {
        shape.setPosition(x + w * 0.5f - shape.getSize().x * 0.5f, y + h * 0.5f - shape.getSize().y * 0.5f);
        text.setPosition(x + (w - text.getGlobalBounds().width) * 0.5f, y + (h - text.getGlobalBounds().height) * 0.5f);
    }

private:
    sf::RectangleShape shape;
    std::function<void()> onClick;
    sf::Text text;
    bool is_hover = false;
};
