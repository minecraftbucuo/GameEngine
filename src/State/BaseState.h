//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <SFML/Graphics.hpp>
#include "Component.h"

class BaseState : public Component {
public:
    explicit BaseState(std::string name) : name(std::move(name)) {}

    virtual void stop() {}

    [[nodiscard]] const std::string& getName() const {
        return name;
    }

protected:
    std::string name;
};