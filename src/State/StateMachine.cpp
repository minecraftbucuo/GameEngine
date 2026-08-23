//
// Created by MINEC on 2026/6/2.
//

#include "StateMachine.h"
#include "Logger.h"
#include "Core/Types.h"

void StateMachine::update(const eng::Time& deltaTime) {
    if (currentState) {
        currentState->update(deltaTime);
    }
}

void StateMachine::render(sf::RenderWindow* window) {
    if (currentState) {
        currentState->render(window);
    }
}

void StateMachine::handleEvent(const eng::EngineEvent& event) {
    if (currentState) {
        currentState->handleEvent(event);
    }
}

void StateMachine::setState(const std::string& stateName) {
    if (!states.contains(stateName)) {
        LOG_INFO_FMT("State {} does not exist!", stateName);
        return;
    }
    if (currentState && currentState->getName() == stateName) return;
    if (currentState) {
        currentState->stop();
    }
    currentState = states[stateName];
    currentState->start();
}

bool StateMachine::getIsLeft() const {
    return isLeft;
}

void StateMachine::setIsLeft(const bool value) {
    this->isLeft = value;
}

std::string StateMachine::getCurrentStateName() const {
    if (currentState) {
        return currentState->getName();
    }
    return "null";
}
