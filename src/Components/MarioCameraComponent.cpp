//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "MarioCameraComponent.h"
#include "Camera.h"
#include "GameObject.h"
#include "Scene.h"

namespace {
    // 摄像机跟随区间（马里奥的 x 坐标）：
    // 越过 START_X 才开始跟随，越过 STOP_X 后定格（终点城堡前停住不再跟）
    const float CAMERA_FOLLOW_START_X = 500.f;
    const float CAMERA_FOLLOW_STOP_X = 13808.f;
}

void MarioCameraComponent::start() {
    if (const Camera* camera = owner->getScene()->getCamera()) {
        this->position = camera->getPosition();
    }
}

void MarioCameraComponent::update(const eng::Time& deltaTime) {
    // 分支必须先判 STOP_X 再判 START_X：马里奥越过 STOP_X 后摄像机
    // 定格在最大跟随位置（STOP_X - START_X），不再跟随
    if (owner->getPosition().x > CAMERA_FOLLOW_STOP_X)
        this->setTargetPositionX(CAMERA_FOLLOW_STOP_X - CAMERA_FOLLOW_START_X);
    else if (owner->getPosition().x > CAMERA_FOLLOW_START_X)
        this->setTargetPositionX(owner->getPosition().x - CAMERA_FOLLOW_START_X);
    else
        this->setTargetPositionX(0);
    if (this->target_position != this->position) {
        if (Camera* camera = owner->getScene()->getCamera()) {
            auto add_position = (target_position - position) * 0.03f;
            if (std::abs(add_position.x) < 0.2f) add_position.x = 0.f;
            position = position + add_position;
            camera->setPosition(position.x, position.y);
        }
    }
}

void MarioCameraComponent::setTargetPosition(const eng::Vec2f& pos) {
    this->target_position = pos;
}

void MarioCameraComponent::setTargetPositionX(const float x) {
    this->target_position.x = x;
}

#endif
