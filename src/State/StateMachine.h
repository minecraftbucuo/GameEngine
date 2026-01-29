//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <unordered_map>

#include "BaseState.h"

class StateMachine : public Component {
public:
    StateMachine() = default;
    explicit StateMachine(GameObject* owner) : Component(owner) {}
    ~StateMachine() override = default;

    void update(const sf::Time& deltaTime) override {
        if (currentState) {
            currentState->update(deltaTime);
        }
    }

    void render(sf::RenderWindow* window) override {
        if (currentState) {
            currentState->render(window);
        }
    }

    void handleEvent(const sf::Event& event) override {
        if (currentState) {
            currentState->handleEvent(event);
        }
    }

    template<typename T, typename... Args>
    void addState(Args&&... args) {
        std::shared_ptr<T> temp = std::make_shared<T>(std::forward<Args>(args)...);
        temp->setOwner(owner);
        states[temp->getName()] = temp;
    }

    void setState(const std::string& stateName) {
        if (currentState) {
            currentState->stop();
        }
        currentState = states[stateName];
        currentState->start();
    }

private:
    std::shared_ptr<BaseState> currentState{};
    std::unordered_map<std::string, std::shared_ptr<BaseState>> states;
};
