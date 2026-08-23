//
// Created by MINEC on 2026/6/2.
//

#include "Brick.h"
#include "AssetManager.h"
#include "BoxGameObject.h"
#include "Collision.h"
#include "BoxCollision.h"
#include "ConfigManager.h"
#include "Core/Types.h"

Brick::Brick(const float x, const float y, const std::string& tag) : BoxGameObject(x, y, 0, 0) {
    this->tag = tag + ":" + std::to_string(id);
    this->moveAble = false;
#ifndef SERVER_BUILD
    sprite.setTexture(AssetManager::getInstance().getTexture("tile_set"));
    sprite.setTextureRect(eng::IntRect(16, 0, 16, 16));
    sprite.setScale(4.f, 4.f);
    sprite.setPosition(this->getPosition());

    this->setSize(sprite.getGlobalBounds().width, sprite.getGlobalBounds().height);
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif

#ifndef SERVER_BUILD
    this->getComponent<Collision, BoxCollision>()->setSize(sprite.getGlobalBounds().width, sprite.getGlobalBounds().height);
#else
    this->getComponent<Collision, BoxCollision>()->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif
    className = "Brick";
}

void Brick::setPosition(const float posX, const float posY) {
    this->position = eng::Vec2f(posX, posY);
    const auto boxCollision = this->getComponent<Collision, BoxCollision>();
    boxCollision->setPosition(posX, posY);
}
