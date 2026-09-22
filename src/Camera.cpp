//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Camera.h"
#include "Core/Input.h"
#include "Manager/ConfigManager.h"

Camera::Camera(eng::Renderer* renderer) {
    init(renderer);
}

void Camera::init(eng::Renderer* _renderer) {
    this->renderer = _renderer;
    // 等比无黑边（Expand）：视口高度恒为设计高度，宽度按窗口宽高比自适应，
    // 视口比例恒等于窗口比例 ⇒ 渲染层 x/y 缩放系数一致，画面等比铺满、无黑边无变形
    resize();
}

void Camera::init() {
    if (renderer) this->resize();
}

void Camera::resize() {
    // 视口高度恒为设计高度，宽度按当前窗口宽高比自适应（Expand 方案核心）；
    // 窗口尺寸不可用时兜底 CONFIG 设计值（winH 至少为设计高，不会除零）
    float winW = static_cast<float>(CONFIG.window.width);
    float winH = static_cast<float>(CONFIG.window.height);
    if (renderer) {
        const eng::Vec2u size = renderer->getSize();
        if (size.x > 0 && size.y > 0) {
            winW = static_cast<float>(size.x);
            winH = static_cast<float>(size.y);
        }
    }
    this->floatRect.width = static_cast<float>(CONFIG.window.height) * (winW / winH);
    this->floatRect.height = static_cast<float>(CONFIG.window.height);
    updateView();
}

void Camera::setSize(const float width, const float height) {
    this->floatRect.width = width;
    this->floatRect.height = height;
    updateView();
}

void Camera::setPosition(const float x, const float y) {
    this->floatRect.left = x;
    this->floatRect.top = y;
    updateView();
}

eng::Vec2f Camera::getPosition() const {
    return {this->floatRect.left, this->floatRect.top};
}

void Camera::setPositionX(const float x) {
    this->floatRect.left = x;
    updateView();
}

void Camera::setMouseControl(const bool flag) {
    this->mouseControl = flag;
}

eng::Vec2f Camera::getCenter() const {
    return {floatRect.left + floatRect.width * 0.5f,
            floatRect.top + floatRect.height * 0.5f};
}

void Camera::addPosition(const eng::Vec2i& pos) {
    this->floatRect.left += static_cast<float>(pos.x);
    this->floatRect.top += static_cast<float>(pos.y);
    updateView();
}

void Camera::handleEvent(const eng::EngineEvent& event) {
    if (!CONFIG.game.debug)
        return;
    if (event.type == eng::EventType::KeyPress) {
        if (event.key == eng::Key::Up) {
            this->floatRect.top -= 20;
            updateView();
        } else if (event.key == eng::Key::Down) {
            this->floatRect.top += 20;
            updateView();
        } else if (event.key == eng::Key::Left) {
            this->floatRect.left -= 20;
            updateView();
        } else if (event.key == eng::Key::Right) {
            this->floatRect.left += 20;
            updateView();
        }
    } else if (event.type == eng::EventType::MouseWheel) {
        float scale;
        if (event.wheelDelta > 0) {
            scale = 0.9f;
        } else {
            scale = 1.f / 0.9f;
        }
        floatRect.width *= scale;
        floatRect.height *= scale;
        updateView();
    }

    if (mouseControl && event.type == eng::EventType::MouseButtonPress) {
        isPressed = true;
        mousePos = eng::Input::getMousePosition();
    }
    if (mouseControl && event.type == eng::EventType::MouseButtonRelease) {
        isPressed = false;
    }
    if (mouseControl && isPressed && event.type == eng::EventType::MouseMove) {
        const auto pos = eng::Input::getMousePosition();
        addPosition(mousePos - pos);
        mousePos = pos;
    }
}

eng::Vec2f Camera::getViewSize() const {
    return floatRect.getSize();
}

void Camera::updateView() {
    // floatRect.left/top 为可视区左上角（原 sf::View(FloatRect) 构造语义），
    // Renderer::setCamera 收中心坐标，在此换算
    renderer->setCamera(eng::Vec2f(floatRect.left + floatRect.width * 0.5f,
                                   floatRect.top + floatRect.height * 0.5f),
                        eng::Vec2f(floatRect.width, floatRect.height));
}
#endif
