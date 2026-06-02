//
// Created by MINEC on 2025/12/13.
//

#pragma once

#include "EventBus.h"
#include "GameObject.h"

class BoxGameObject : public GameObject {
public:
    BoxGameObject();
    BoxGameObject(float posX, float posY, float width, float height, const std::string& tag = "box");

    ~BoxGameObject() override;

    void start() override;

};

