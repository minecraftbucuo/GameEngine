//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Camera.h"
#include <SFML/Graphics.hpp>

Camera::Camera(sf::RenderWindow* window) {
    init(window);
}

void Camera::init(sf::RenderWindow* _window) {
    this->window = _window;
    this->floatRect = eng::FloatRect(0, 0,
                    static_cast<float>(window->getSize().x),
                    static_cast<float>(window->getSize().y));
    this->view = sf::View(floatRect);
    window->setView(view);
}

void Camera::init() {
    if (window) this->resize();
}

void Camera::resize() {
    this->floatRect.width = static_cast<float>(window->getSize().x);
    this->floatRect.height = static_cast<float>(window->getSize().y);
    updateView();
}

void Camera::setSize(const float width, const float height) {
    this->floatRect.width = width;
    this->floatRect.height = height;
    updateView();
}

void Camera::setPosition(const float x, const float y) {
    this->floatRect.left = x;
    this->floatRect.top = y;
    updateView();
}

eng::Vec2f Camera::getPosition() const {
    return {this->floatRect.left, this->floatRect.top};
}

void Camera::setPositionX(const float x) {
    this->floatRect.left = x;
    updateView();
}

void Camera::setMouseControl(const bool flag) {
    this->mouseControl = flag;
}

eng::Vec2f Camera::getCenter() const {
    return view.getCenter();
}

void Camera::addPosition(const eng::Vec2i& pos) {
    this->floatRect.left += static_cast<float>(pos.x);
    this->floatRect.top += static_cast<float>(pos.y);
    updateView();
}

void Camera::handleEvent(const sf::Event& event) {
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

eng::Vec2f Camera::getViewSize() const {
    return floatRect.getSize();
}

void Camera::updateView() {
    view = sf::View(floatRect);
    window->setView(view);
}
#endif
