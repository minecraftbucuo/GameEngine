//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <vector>
#include "Render/Handles.h"

namespace eng { class Renderer; }

class Animation {
public:
    // SDL3 迁移 6c：Frame 纯数据化，texture 存句柄而非 sf::Texture*
    struct Frame {
        eng::TextureHandle texture;
        eng::IntRect textureRect;
        eng::Vec2f origin = {0.f, 0.f};
        eng::Vec2f scale = {1.f, 1.f};
        unsigned int duration = 100;
    };

    Animation() = default;
    ~Animation() = default;

    void addFrame(const Frame& frame) const;

    void setBack(bool flag);

    void setFrames(std::vector<Frame>* _frames);

    void update(const eng::Time& deltaTime);

    void render(eng::Renderer& renderer, const eng::Vec2f& position);

    // 获取动画是否完整播放完一遍
    bool isOver() const;

    Frame& getFrame() const;

    std::vector<Frame>& getFrames() const;

    float getFrameWidth() const;

    float getFrameHeight() const;

private:
    unsigned int currentFrame = 0;
    unsigned int currentFrameDuration = 0;
    std::vector<Frame>* frames{};
    bool back = false;
    bool over = false;
    int add = 1;
};
#endif
