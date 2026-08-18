//
// Created by MINEC on 2026/8/18.
//

#ifndef SERVER_BUILD
#include "PhysicsTestScene.h"
#include "GameObject.h"
#include "PhysicsBodyComponent.h"
#include "PhysicsTypes.h"
#include "PhysicsWorld.h"
#include "SceneManager.h"
#include "Logger.h"
#include <random>

// 可控的玩家方块：方向键移动，空格跳跃
class PhysicsPlayer : public GameObject {
public:
    PhysicsPlayer(float x, float y) : GameObject(x, y, 48, 48) {
        tag = "physics_player";
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Dynamic);
        phys->setDensity(1.0f);
        phys->setFriction(0.3f);
        phys->setRestitution(0.0f);
        phys->setFixedRotation(false);
        phys->setLinearDamping(0.5f);
        phys->setAngularDamping(0.5f);
        phys->setCollisionFilter(physics::Category::Player, physics::Category::All);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setOrigin(getSize() * 0.5f);
        shape.setPosition(getPosition() + getSize() * 0.5f);
        shape.setRotation(rotation);
        shape.setFillColor(sf::Color(255, 200, 80));
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }

    void handleEvent(sf::Event& event) override {
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Space) {
            if (onGround) {
                auto phys = getComponent<PhysicsBodyComponent>();
                if (phys && phys->getBody()) {
                    // 直接设置向上的速度，避免冲量单位转换问题
                    b2Vec2 v = phys->getBody()->GetLinearVelocity();
                    phys->getBody()->SetLinearVelocity(b2Vec2(v.x, -20.0f));
                }
            }
        }
    }

    void update(sf::Time deltaTime) override {
        GameObject::update(deltaTime);
        auto phys = getComponent<PhysicsBodyComponent>();
        if (!phys || !phys->getBody()) return;

        // 直接从 Box2D 接触列表判断是否在地面
        onGround = checkOnGround(phys->getBody());

        // 左右移动（只改 x，保留 y 速度）
        b2Vec2 v = phys->getBody()->GetLinearVelocity();
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) v.x = -20;
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) v.x = 20;
        else v.x = 0;
        phys->getBody()->SetLinearVelocity(v);
    }

    // 遍历 body 的接触边，检查是否踩在东西上
    bool checkOnGround(b2Body* body) {
        for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
            if (edge->contact && edge->contact->IsTouching()) {
                b2WorldManifold manifold;
                edge->contact->GetWorldManifold(&manifold);
                // 法线 y 分量绝对值大于 0.5 说明是上下方向的接触
                if (std::abs(manifold.normal.y) > 0.5f) return true;
            }
        }
        return false;
    }

private:
    bool onGround = false;
};

// 普通动态方块
class PhysicsBox : public GameObject {
public:
    PhysicsBox(float x, float y, float w, float h, sf::Color color = sf::Color(100, 200, 255))
        : GameObject(x, y, w, h), fillColor(color) {
        tag = "physics_box:" + std::to_string(id);
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Dynamic);
        phys->setDensity(1.0f);
        phys->setFriction(0.3f);
        phys->setRestitution(0.0f);
        phys->setFixedRotation(false);
        phys->setLinearDamping(0.1f);
        phys->setAngularDamping(0.05f);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setOrigin(getSize() * 0.5f);
        shape.setPosition(getPosition() + getSize() * 0.5f);
        shape.setRotation(rotation);
        shape.setFillColor(fillColor);
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
private:
    sf::Color fillColor;
};

// 弹性球
class PhysicsBall : public GameObject {
public:
    PhysicsBall(float x, float y, float r) : GameObject(x, y, r*2, r*2), radius(r) {
        tag = "physics_ball:" + std::to_string(id);
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Dynamic);
        phys->setDensity(0.5f);
        phys->setFriction(0.2f);
        phys->setRestitution(0.4f);
        phys->setShapeCircle(radius);
        phys->setLinearDamping(0.3f);
        phys->setAngularDamping(0.3f);
    }

    void render(sf::RenderWindow* window) override {
        sf::CircleShape shape(radius);
        shape.setOrigin(radius, radius);
        shape.setPosition(getPosition() + sf::Vector2f(radius, radius));
        shape.setRotation(rotation);
        shape.setFillColor(sf::Color(255, 100, 100));
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
private:
    float radius;
};

// 静态地面/墙壁
class PhysicsGround : public GameObject {
public:
    PhysicsGround(float x, float y, float w, float h, sf::Color color = sf::Color(80, 160, 80))
        : GameObject(x, y, w, h), fillColor(color) {
        tag = "physics_ground:" + std::to_string(id);
        moveAble = false;
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Static);
        phys->setFriction(0.5f);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setPosition(getPosition());
        shape.setFillColor(fillColor);
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
private:
    sf::Color fillColor;
};

// 倾斜平台（静态）
class PhysicsPlatform : public GameObject {
public:
    PhysicsPlatform(float x, float y, float w, float h, float angleDeg)
        : GameObject(x, y, w, h), angle(angleDeg) {
        tag = "physics_platform:" + std::to_string(id);
        moveAble = false;
        auto phys = addComponent<PhysicsBodyComponent>();
        phys->setBodyType(physics::BodyType::Static);
        phys->setFriction(0.3f);
        phys->setShapeBox(w, h);
        phys->setInitialAngle(angleDeg * 3.14159265f / 180.0f);
    }

    void render(sf::RenderWindow* window) override {
        sf::RectangleShape shape(getSize());
        shape.setPosition(getPosition().x + getSize().x * 0.5f, getPosition().y + getSize().y * 0.5f);
        shape.setOrigin(getSize() * 0.5f);
        shape.setRotation(angle);
        shape.setFillColor(sf::Color(160, 120, 80));
        shape.setOutlineColor(sf::Color::White);
        shape.setOutlineThickness(2);
        window->draw(shape);
    }
private:
    float angle;
};

void PhysicsTestScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;

    LOG_INFO("PhysicsTestScene::init() - setting up scene");

    // 玩家
    player = std::make_shared<PhysicsPlayer>(300, 600);
    addObjectWithMap(player);

    // 边界墙
    addObjectWithMap(std::make_shared<PhysicsGround>(0, 0, 40, 960, sf::Color(120, 120, 120)));       // 左墙
    addObjectWithMap(std::make_shared<PhysicsGround>(1160, 0, 40, 960, sf::Color(120, 120, 120)));   // 右墙
    addObjectWithMap(std::make_shared<PhysicsGround>(0, 900, 1200, 60));                              // 底

    // 倾斜平台
    addObjectWithMap(std::make_shared<PhysicsPlatform>(150, 750, 300, 20, -15));
    addObjectWithMap(std::make_shared<PhysicsPlatform>(700, 600, 300, 20, 15));
    addObjectWithMap(std::make_shared<PhysicsPlatform>(400, 400, 250, 20, -10));

    // 堆叠的方块（在地面上）
    addObjectWithMap(std::make_shared<PhysicsBox>(500, 800, 50, 50, sf::Color(100, 200, 255)));
    addObjectWithMap(std::make_shared<PhysicsBox>(560, 800, 50, 50, sf::Color(100, 255, 200)));
    addObjectWithMap(std::make_shared<PhysicsBox>(530, 740, 50, 50, sf::Color(255, 200, 100)));

    // 落到倾斜平台上的方块（会旋转滑动）
    addObjectWithMap(std::make_shared<PhysicsBox>(250, 500, 50, 50, sf::Color(200, 100, 255)));
    addObjectWithMap(std::make_shared<PhysicsBox>(750, 300, 60, 40, sf::Color(255, 150, 150)));
    addObjectWithMap(std::make_shared<PhysicsBox>(450, 200, 45, 45, sf::Color(150, 255, 100)));

    // 高弹性球
    addObjectWithMap(std::make_shared<PhysicsBall>(900, 100, 25));
    addObjectWithMap(std::make_shared<PhysicsBall>(950, 0, 20));

    // 散落方块
    addObjectWithMap(std::make_shared<PhysicsBox>(100, 200, 40, 40, sf::Color(200, 100, 255)));
}

void PhysicsTestScene::handleEvent(sf::Event& event) {
    Scene::handleEvent(event);
    if (event.type == sf::Event::KeyPressed) {
        if (event.key.code == sf::Keyboard::Escape) {
            getSceneManager()->loadScene("MenuScene");
        }
        if (event.key.code == sf::Keyboard::R) {
            exit();
            getSceneManager()->loadScene("PhysicsTestScene");
        }
    }
    // 鼠标点击放球
    if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
        sf::Vector2i mousePos = sf::Mouse::getPosition(*window);
        sf::Vector2f worldPos = window->mapPixelToCoords(mousePos);
        auto ball = std::make_shared<PhysicsBall>(worldPos.x - 20.f, worldPos.y - 20.f, 20.0f);
        addObjectWithMap(ball); // 先设置 scene
        ball->start();          // 再 start 创建 body
    }
}

void PhysicsTestScene::exit() {
    is_init = false;
    player.reset();
    game_objects.clear();
    game_objects_map.clear();
    if (physics_world) {
        physics_world->clear();
    }
}
#endif
