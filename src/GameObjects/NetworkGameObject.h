//
// Created by MINEC on 2026/5/9.
//

#pragma once
#include "GameObject.h"
#include "ISerializable.h"

class NetworkGameObject : public GameObject, public ISerializable {
public:
    NetworkGameObject(const float posX, const float posY, const float width, const float height) : GameObject(
        posX, posY, width, height) {
    }

    NetworkGameObject() = default;

    ~NetworkGameObject() override = default;

    unsigned int getNetworkId() override {
        return this->getId();
    }

    void disconnect() override  {
        destroy();
    }
};


