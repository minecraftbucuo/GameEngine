//
// Created by MINEC on 2026/6/2.
//

#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Animation.h"
#include "Render/Renderer.h"
#include <cmath>

void Animation::addFrame(const Frame& frame) const {
    frames->push_back(frame);
}

void Animation::setBack(const bool flag) {
    this->back = flag;
}

void Animation::setFrames(std::vector<Frame>* _frames) {
    this->frames = _frames;
}

void Animation::update(const eng::Time& deltaTime) {
    currentFrameDuration += deltaTime.asMilliseconds();
    if (currentFrameDuration >= (*frames)[currentFrame].duration) {
        currentFrameDuration = 0;
        if (back) {
            if (currentFrame == 0) add = 1;
            else if (currentFrame == frames->size() - 1) {
                add = -1;
                over = true;
            }
            currentFrame = currentFrame + add;
        } else {
            if (currentFrame + 1 == frames->size()) over = true;
            currentFrame = (currentFrame + 1) % frames->size();
        }
    }
}

void Animation::render(eng::Renderer& renderer, const eng::Vec2f& position) {
    const Frame& f = getFrame();
    // 负 scale.x（JSON 帧数据里的镜像帧）→ flipX；dst 尺寸取绝对值
    const bool flip = f.scale.x < 0.f;
    const float w = getFrameWidth();
    const float h = getFrameHeight();
    // 复现原 SFML 帧布局：显示区域端点 = position + scale*(v - origin)，v∈{0, rectSize}
    //（左向帧 origin=(rect.w,0)+负 scale ⇒ 与右向帧同区域、内容镜像）
    const float rectW = static_cast<float>(f.textureRect.width);
    const float left = flip
        ? position.x + f.scale.x * (rectW - f.origin.x)
        : position.x - f.scale.x * f.origin.x;
    const float top = position.y - f.scale.y * f.origin.y;
    renderer.drawTexture(f.texture,
                         eng::FloatRect(static_cast<float>(f.textureRect.left),
                                        static_cast<float>(f.textureRect.top),
                                        rectW,
                                        static_cast<float>(f.textureRect.height)),
                         eng::FloatRect(left, top, w, h),
                         0.f, {0.f, 0.f}, eng::Color::White, flip);
}

bool Animation::isOver() const {
    return over;
}

Animation::Frame& Animation::getFrame() const {
    return (*frames)[currentFrame];
}

std::vector<Animation::Frame>& Animation::getFrames() const {
    return (*frames);
}

float Animation::getFrameWidth() const {
    return std::abs(getFrame().scale.x) * static_cast<float>(getFrame().textureRect.width);
}

float Animation::getFrameHeight() const {
    return std::abs(getFrame().scale.y) * static_cast<float>(getFrame().textureRect.height);
}
#endif
