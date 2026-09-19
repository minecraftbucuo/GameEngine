//
// Created by MINEC on 2026/5/8.
//
#include "Box.h"

#include "Collision.h"
#include "CollisionHandle.h"
#include "FrameManager.h"
#include "GravityComponent.h"
#include "MoveComponent.h"
#include "BoxCollision.h"
#include "Mushroom.h"
#include "Scene.h"
#include "CollisionSystem.h"
#include "Core/Types.h"

Box::Box(const float x, const float y, const std::string& tag) : BoxGameObject(x, y, 0, 0) {
    this->tag = tag + ":" + std::to_string(id);

    last_y = y;
#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame("box_frame"));
    animation.setBack(true);
#endif

#ifndef SERVER_BUILD
    // SDL3 迁移 6c：getSprite().getGlobalBounds() → 帧尺寸计算（帧 0，与迁移前一致）
    const auto w = animation.getFrameWidth();
    const auto h = animation.getFrameHeight();
#else
    const auto w = CONFIG.game.defaultBlockSize;
    const auto h = CONFIG.game.defaultBlockSize;
#endif
    this->setSize(w, h);
    this->getComponent<Collision, BoxCollision>()->setSize(w, h);

    this->removeComponent<CollisionHandle>();

    this->addComponent<MoveComponent>();
    this->addComponent<GravityComponent>()->setActive(false);
    className = "Box";
}

void Box::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>("onCollision" + this->tag,
        [this](const CollisionEvent& event) {
            if (event.b->getClassName() != "Mario") return;

            const auto& this_pos = event.a_position;
            const auto& other_pos = event.b_position;
            if (other_pos.y < this_pos.y) return;

            // 水平重合量
            const auto w = std::min(this_pos.x + this->getSize().x, other_pos.x + event.b->getSize().x) -
                std::max(this_pos.x, other_pos.x);

            if (w < 0.4 * this->getSize().x) return;

            const auto& move_component = this->getComponent<MoveComponent>();
            move_component->setSpeedY(-300.f);
            this->getComponent<GravityComponent>()->setActive(true);

            // 首次被顶：方块永久变暗，并请求在下一帧 update 里生成蘑菇
            if (!has_spawned) {
                has_spawned = true;
#ifndef SERVER_BUILD
                animation.setFrames(FrameManager::getInstance().getFrame("box_used_frame"));
                // 1 帧的静态帧不能再用乒乓模式（3 帧弹跳动画遗留），否则索引越界
                animation.setBack(false);
#endif
                // 单机本地生成；联机由服务端（主机）生成并广播 SpawnObject，客户端只等服务端消息
                if (getScene() && getScene()->getNetworkType() != NetworkManager::NetworkType::Client) {
                    pending_spawn = true;
                }
            }
        }
    );
}

void Box::update(eng::Time deltaTime) {
    BoxGameObject::update(deltaTime);
#ifndef SERVER_BUILD
    animation.update(deltaTime);
#endif

    // 上一帧碰撞回调记录的生成请求：对象循环容忍 game_objects 变化，这里才真正改场景
    if (pending_spawn) {
        pending_spawn = false;
        auto mushroom = std::make_shared<Mushroom>(this->getPosition().x, this->getPosition().y - 20.f, 120.f);
        mushroom->setScene(getScene());
        if (getScene()->getNetworkType() == NetworkManager::NetworkType::None) {
            // 单机：插到本方块之前（渲染顺序 = game_objects 顺序，升起过程被方块遮挡）
            auto& objs = getScene()->getGameObjects();
            size_t self_index = objs.size();
            for (size_t i = 0; i < objs.size(); ++i) {
                if (objs[i].get() == this) {
                    self_index = i;
                    break;
                }
            }
            objs.insert(objs.begin() + self_index, mushroom);
            // 与 SuperMarioSceneSingle::addObject 一致：带碰撞组件的对象要注册进碰撞系统
            if (auto* cs = getScene()->getCollisionSystem(); cs && mushroom->getComponent<Collision>()) {
                cs->addObject(mushroom);
            }
        } else {
            // 服务端（主机）：走网络生成链路（广播 SpawnObject + 本地添加 + 碰撞注册）
            getScene()->addObjectWithNetwork(mushroom);
            // 本地渲染顺序重排：把蘑菇从尾部移到本方块之前，升起过程的遮挡表现与单机一致
            auto& objs = getScene()->getGameObjects();
            size_t self_index = objs.size(), mush_index = objs.size();
            for (size_t i = 0; i < objs.size(); ++i) {
                if (objs[i].get() == this) self_index = i;
                if (objs[i] == mushroom) mush_index = i;
            }
            if (self_index < objs.size() && mush_index < objs.size() && mush_index > self_index) {
                objs.erase(objs.begin() + mush_index);
                objs.insert(objs.begin() + self_index, mushroom);
            }
        }
    }
    if (this->getPosition().y > last_y) {
        const auto& move_component = this->getComponent<MoveComponent>();
        move_component->setPositionY(last_y);
        move_component->setSpeedY(0.f);

        this->getComponent<GravityComponent>()->setActive(false);
    }
}

void Box::setPosition(const float posX, const float posY) {
    this->position = eng::Vec2f(posX, posY);
    const auto boxCollision = this->getComponent<Collision, BoxCollision>();
    boxCollision->setPosition(posX, posY);
}

#ifndef SERVER_BUILD
void Box::render(eng::Renderer& renderer) {
    animation.render(renderer, this->getPosition());
    BoxGameObject::render(renderer);
}
#endif
