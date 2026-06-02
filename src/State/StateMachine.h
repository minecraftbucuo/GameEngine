//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <unordered_map>
#include "BaseState.h"
#include <memory>

class StateMachine : public Component {
public:
    StateMachine() = default;
    explicit StateMachine(GameObject* owner) : Component(owner) {}
    ~StateMachine() override = default;

    void update(const sf::Time& deltaTime) override;

    void render(sf::RenderWindow* window) override;

    void handleEvent(const sf::Event& event) override;

    template<typename T, typename... Args>
    void addState(Args&&... args) {
        std::shared_ptr<T> temp = std::make_shared<T>(std::forward<Args>(args)...);
        temp->setOwner(owner);
        states[temp->getName()] = temp;
    }

    void setState(const std::string& stateName);

    bool getIsLeft() const;

    void setIsLeft(const bool value);

    std::string getCurrentStateName() const;

    const std::shared_ptr<BaseState>& getCurrentState() const {
        return currentState;
    }

private:
    std::shared_ptr<BaseState> currentState{};
    std::unordered_map<std::string, std::shared_ptr<BaseState>> states;
    bool isLeft = false;
};
