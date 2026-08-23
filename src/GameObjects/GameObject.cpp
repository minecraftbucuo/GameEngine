//
// Created by MINEC on 2026/5/8.
//

#include "GameObject.h"

#include <ranges>

#include "Logger.h"
#include "Core/Types.h"

GameObject::GameObject() : position(0, 0), size(0, 0), speed(0, 0), active(true), started(false) {
    this->id = idCounter++;
}

GameObject::GameObject(const float posX, const float posY, const float width, const float height) {
    this->position = eng::Vec2f(posX, posY);
    this->size = eng::Vec2f(width, height);
    this->speed = eng::Vec2f(0, 0);
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


void GameObject::updateComponents(eng::Time deltaTime) {
    for (const auto& key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key.name());
            continue;
        }
        if (it->second->getActive())
            it->second->update(deltaTime);
    }
}

void GameObject::renderComponents(sf::RenderWindow* window) {
    for (const auto& key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key.name());
            continue;
        }
        if (it->second->getActive())
            it->second->render(window);
    }
}

void GameObject::handleComponents(const eng::EngineEvent& e) {
    for (const auto& key : components_vector) {
        auto it = components.find(key);
        if (it == components.end()) {
            LOG_ERROR_FMT("Component not found: {}", key.name());
            continue;
        }
        if (it->second->getActive())
            it->second->handleEvent(e);
    }
}
