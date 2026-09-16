//
// Created by MINEC on 2026/6/22.
//

#pragma once
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include <string>
#include <vector>
#include "Scene.h"
#include "Render/Handles.h"

class TextInput;
class Toggle;
class Button;

class SettingsScene : public Scene {
public:
    explicit SettingsScene(eng::Renderer* _renderer);
    ~SettingsScene() override = default;

    void init() override;

    void initScene();

    void handleEvent(const eng::EngineEvent& event) override;

    void update(eng::Time deltaTime) override;

    void render(eng::Renderer& _renderer) override;

private:
    // 文本标签（SDL3 迁移 6d：sf::Text 数据化）
    struct Label {
        std::string text;
        eng::Vec2f pos;
        unsigned size;
        eng::Color color;
    };

    eng::FontHandle font;
    Label title;
    std::vector<Label> labels;

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

    // 按钮引用：自适应视口宽度变化后需按新视口重新定位
    std::shared_ptr<Button> saveBtn;
    std::shared_ptr<Button> backBtn;

    void relayout();
};
#endif
