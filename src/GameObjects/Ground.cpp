//
// Created by MINEC on 2026/6/2.
//

#include "Ground.h"
#include "Collision.h"
#include "BoxCollision.h"

Ground::Ground(const float x, const float y, const float width, const float height, const std::string& tag) : BoxGameObject(x, y, width, height) {
    this->tag = tag + ":" + std::to_string(id);
    this->moveAble = false;
    className = "Ground";
}

void Ground::setPosition(const float posX, const float posY) {
    this->position = sf::Vector2f(posX, posY);
    const auto boxCollision = this->getComponent<Collision, BoxCollision>();
    boxCollision->setPosition(posX, posY);
}
