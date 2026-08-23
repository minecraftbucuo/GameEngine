//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"
#include <SFML/Graphics.hpp>

class Controller : public Component {
public:
    void handleEvent(const eng::EngineEvent& event) override;
};

