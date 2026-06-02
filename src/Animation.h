//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <vector>
#include <SFML/Graphics.hpp>


class Animation {
public:
    struct Frame {
        sf::Texture* texture;
        sf::IntRect textureRect;
        sf::Vector2f origin = {0.f, 0.f};
        sf::Vector2f scale = {1.f, 1.f};
        unsigned int duration = 100;
    };

    Animation() = default;
    ~Animation() = default;

    void addFrame(const Frame& frame) const;

    void setBack(bool flag);

    void setFrames(std::vector<Frame>* _frames);

    void update(const sf::Time& deltaTime);

    void render(sf::RenderWindow* window, const sf::Vector2f& position);

    // 获取动画是否完整播放完一遍
    bool isOver() const;

    Frame& getFrame() const;

    std::vector<Frame>& getFrames() const;

    sf::Sprite& getSprite();

    float getFrameWidth() const;

    float getFrameHeight() const;

private:
    unsigned int currentFrame = 0;
    unsigned int currentFrameDuration = 0;
    std::vector<Frame>* frames{};
    sf::Sprite sprite;
    bool back = false;
    bool over = false;
    int add = 1;
};
