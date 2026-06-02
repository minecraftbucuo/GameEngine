//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include "BoxGameObject.h"

class Ground : public BoxGameObject {
public:
    Ground(float x, float y, float width, float height, const std::string& tag = "ground");

    void setPosition(float posX, float posY) override;
};


