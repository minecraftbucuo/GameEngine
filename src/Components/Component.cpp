//
// Created by MINEC on 2026/6/2.
//

#include "Component.h"

void Component::setOwner(GameObject* obj) {
    owner = obj;
}

GameObject* Component::getOwner() const {
    return owner;
}

void Component::setActive(const bool value) {
    active = value;
}

bool Component::getActive() const {
    return active;
}
