//
// Created by MINEC on 2026/6/22.
//

#pragma once
#ifndef SERVER_BUILD
#include "Scene.h"

class TextInput;
class Toggle;

class SettingsScene : public Scene {
public:
    explicit SettingsScene(sf::RenderWindow* _window);
    ~SettingsScene() override = default;

    void init() override;
    void initScene();
    void update(sf::Time deltaTime) override;
    void render(sf::RenderWindow* _window) override;

private:
    sf::Text title;
    std::vector<sf::Text> labels;

    // 持有输入控件的引用，用于保存时读取
    std::shared_ptr<TextInput> widthInput;
    std::shared_ptr<TextInput> heightInput;
    std::shared_ptr<TextInput> fpsInput;
    std::shared_ptr<TextInput> ipInput;
    std::shared_ptr<TextInput> portInput;
    std::shared_ptr<TextInput> tickRateInput;
    std::shared_ptr<TextInput> gravityInput;
    std::shared_ptr<TextInput> playerSpeedInput;
    std::shared_ptr<TextInput> jumpForceInput;
    std::shared_ptr<Toggle> debugToggle;
};
#endif
