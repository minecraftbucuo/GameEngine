//
// Created by MINEC on 2025/12/18.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Component.h"

class CameraComponent : public Component {
public:
    CameraComponent() = default;
    void update(const eng::Time& deltaTime) override;
};
#endif
