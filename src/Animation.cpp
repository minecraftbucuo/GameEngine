//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "Animation.h"

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

void Animation::render(sf::RenderWindow* window, const eng::Vec2f& position) {
    sf::Sprite& sprite_ = this->getSprite();
    sprite_.setPosition(position);
    window->draw(sprite_);
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

sf::Sprite& Animation::getSprite() {
    sprite.setTexture(*(*frames)[currentFrame].texture);
    sprite.setTextureRect((*frames)[currentFrame].textureRect);
    sprite.setOrigin((*frames)[currentFrame].origin);
    sprite.setScale((*frames)[currentFrame].scale);
    return sprite;
}

float Animation::getFrameWidth() const {
    return getFrame().scale.x * static_cast<float>(getFrame().textureRect.width);
}

float Animation::getFrameHeight() const {
    return getFrame().scale.y * static_cast<float>(getFrame().textureRect.height);
}
#endif