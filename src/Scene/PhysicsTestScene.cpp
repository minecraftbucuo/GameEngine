//
// Created by MINEC on 2026/8/18.
//

#ifndef SERVER_BUILD
#include "PhysicsTestScene.h"
#include "GameObject.h"
#include "PhysicsBodyComponent.h"
#include "SceneManager.h"

// 简单的方块游戏对象，用 SFML 矩形渲染，物理驱动位置
class PhysicsBox : public GameObject {
public:
    PhysicsBox(float x, float y, float w, float h) : GameObject(x, y, w, h) {
        tag = "physics_box:" + std::to_string(id);
        // 在构造函数里添加组件，确保 start() 遍历时能调用到
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Dynamic);
        phys->setDensity(1.0f);
        phys->setFriction(0.3f);
        phys->setRestitution(0.5f);
        phys->setFixedRotation(true);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setPosition(getPosition());
        shape.setFillColor(sf::Color(100, 200, 255));
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
};

// 静态地面
class PhysicsGround : public GameObject {
public:
    PhysicsGround(float x, float y, float w, float h) : GameObject(x, y, w, h) {
        tag = "physics_ground:" + std::to_string(id);
        moveAble = false;
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Static);
        phys->setFriction(0.5f);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setPosition(getPosition());
        shape.setFillColor(sf::Color(80, 160, 80));
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
};

void PhysicsTestScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;

    // 动态方块：从上方落下
    addObjectWithMap(std::make_shared<PhysicsBox>(400, 100, 64, 64));
    addObjectWithMap(std::make_shared<PhysicsBox>(500, 0, 48, 48));

    // 静态地面
    addObjectWithMap(std::make_shared<PhysicsGround>(0, 800, 1200, 160));
}

void PhysicsTestScene::handleEvent(sf::Event& event) {
    Scene::handleEvent(event);
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            getSceneManager()->loadScene("MenuScene");
        }
    }
}
#endif
