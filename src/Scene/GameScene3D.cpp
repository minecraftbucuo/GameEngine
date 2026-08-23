//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "GameScene3D.h"
#include "SceneManager.h"

void GameScene3D::init() {
    renderer->setSize(eng::Vec2u(1200, 1200));
    Scene::init();
    if (is_init) return;
    is_init = true;
    // this->addObject(std::make_shared<Penguin3D>());
    // this->addObject(std::make_shared<Cube3D>());
    this->addObject(std::make_shared<Cube3DWithController>());
    // this->addObject(std::make_shared<NewModel3D>());
    // this->addObject(std::make_shared<Human3D>());
}

void GameScene3D::handleEvent(const eng::EngineEvent& event) {
    Scene::handleEvent(event);
    if (event.type == eng::EventType::KeyPress && event.key == eng::Key::Escape) {
        this->getSceneManager()->loadScene("MenuScene");
    }
}

void GameScene3D::exit() {
    renderer->setSize(eng::Vec2u(CONFIG.window.width, CONFIG.window.height));
}
#endif
