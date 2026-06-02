//
// Created by MINEC on 2026/6/2.
//

#ifndef SERVER_BUILD
#include "GameScene3D.h"
#include "SceneContext.h"
#include "SceneManager.h"

void GameScene3D::init() {
    window->setSize(sf::Vector2u(1200, 1200));
    Scene::init();
    if (is_init) return;
    is_init = true;
    // this->addObject(std::make_shared<Penguin3D>());
    // this->addObject(std::make_shared<Cube3D>());
    this->addObject(std::make_shared<Cube3DWithController>());
    // this->addObject(std::make_shared<NewModel3D>());
    // this->addObject(std::make_shared<Human3D>());
}

void GameScene3D::handleEvent(sf::Event& event) {
    Scene::handleEvent(event);
    if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
        SceneContext::getInstance().getSceneManager()->loadScene("MenuScene");
    }
}

void GameScene3D::exit() {
    window->setSize(sf::Vector2u(CONFIG.window.width, CONFIG.window.height));
}
#endif
