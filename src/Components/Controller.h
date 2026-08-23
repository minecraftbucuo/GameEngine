//
// Created by MINEC on 2025/12/10.
//

#pragma once

#include "Component.h"

class Controller : public Component {
public:
    void handleEvent(const eng::EngineEvent& event) override;
};

