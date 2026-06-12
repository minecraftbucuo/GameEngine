//
// Created by MINEC on 2026/5/8.
//

#include "GameObject.h"

#include <ranges>

#include "Logger.h"

GameObject::GameObject() : position(0, 0), size(0, 0), speed(0, 0), active(true), started(false) {
    this->id = idCounter++;
}

GameObject::GameObject(const float posX, const float posY, const float width, const float height) {
    this->position = sf::Vector2f(posX, posY);
    this->size = sf::Vector2f(width, height);
    this->speed = sf::Vector2f(0, 0);
    this->started = false;
    this->active = true;
    this->id = idCounter++;
}

void GameObject::start() {
    started = true;
    for (const auto& component : components | std::views::values) {
        component->start();
    }
}


void GameObject::updateComponents(sf::Time deltaTime) {
    for (const auto key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key);
            continue;
        }
        if (components[key]->getActive())
            components[key]->update(deltaTime);
    }
}

void GameObject::renderComponents(sf::RenderWindow* window) {
    for (const auto key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key);
            continue;
        }
        if (components[key]->getActive())
            components[key]->render(window);
    }
}

void GameObject::handleComponents(sf::Event& e) {
    for (const auto key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key);
            continue;
        }
        if (components[key]->getActive())
            components[key]->handleEvent(e);
    }
}
