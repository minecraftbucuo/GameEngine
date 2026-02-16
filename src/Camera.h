//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include <SFML/Graphics.hpp>

class Camera {
public:
    Camera() = default;
    explicit Camera(sf::RenderWindow* window) {
        init(window);
    }

    void init(sf::RenderWindow* _window) {
        this->window = _window;
        this->floatRect = sf::FloatRect(0, 0,
                        static_cast<float>(window->getSize().x),
                        static_cast<float>(window->getSize().y));
        this->view = sf::View(floatRect);
        window->setView(view);
    }

    void init() {
        if (window) this->resize();
    }

    void resize() {
        this->floatRect.width = static_cast<float>(window->getSize().x);
        this->floatRect.height = static_cast<float>(window->getSize().y);
        updateView();
    }

    void setSize(const float width, const float height) {
        this->floatRect.width = width;
        this->floatRect.height = height;
        updateView();
    }

    void setPosition(const float x, const float y) {
        this->floatRect.left = x;
        this->floatRect.top = y;
        updateView();
    }

    void setPositionX(const float x) {
        this->floatRect.left = x;
        updateView();
    }

    sf::Vector2f getCenter() const {
        return view.getCenter();
    }

    void addPosition(const sf::Vector2i& pos) {
        this->floatRect.left += static_cast<float>(pos.x);
        this->floatRect.top += static_cast<float>(pos.y);
        updateView();
    }

    void handleEvent(const sf::Event& event) {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::Up) {
                this->floatRect.top -= 20;
                updateView();
            } else if (event.key.code == sf::Keyboard::Down) {
                this->floatRect.top += 20;
                updateView();
            } else if (event.key.code == sf::Keyboard::Left) {
                this->floatRect.left -= 20;
                updateView();
            } else if (event.key.code == sf::Keyboard::Right) {
                this->floatRect.left += 20;
                updateView();
            }
        } else if (event.type == sf::Event::MouseWheelScrolled) {
            float scale;
            if (event.mouseWheelScroll.delta > 0) {
                scale = 0.9f;
            } else {
                scale = 1.f / 0.9f;
            }
            floatRect.width *= scale;
            floatRect.height *= scale;
            updateView();
        }

        if (mouseControl && event.type == sf::Event::MouseButtonPressed) {
            isPressed = true;
            mousePos = sf::Mouse::getPosition(*window);
        }
        if (mouseControl && event.type == sf::Event::MouseButtonReleased) {
            isPressed = false;
        }
        if (mouseControl && isPressed && event.type == sf::Event::MouseMoved) {
            const auto pos = sf::Mouse::getPosition(*window);
            addPosition(mousePos - pos);
            mousePos = pos;
        }
    }

private:
    sf::FloatRect floatRect;
    sf::View view;
    sf::RenderWindow* window{};
    bool mouseControl = true;
    sf::Vector2i mousePos;
    bool isPressed = false;

    void updateView() {
        view = sf::View(floatRect);
        window->setView(view);
    }
};
