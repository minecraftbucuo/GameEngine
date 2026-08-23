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
    // SDL3 迁移 6c：精灵数据化（原 sf::Sprite：tile_set (16,0,16,16)，4 倍放大）
    texture = AssetManager::getInstance().getTextureHandle("tile_set");

    this->setSize(64.f, 64.f);
#else
    this->setSize(CONFIG.game.defaultBlockSize, CONFIG.game.defaultBlockSize);
#endif

#ifndef SERVER_BUILD
    this->getComponent<Collision, BoxCollision>()->setSize(64.f, 64.f);
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
