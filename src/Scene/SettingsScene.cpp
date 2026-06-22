//
// Created by MINEC on 2026/6/22.
//

#ifndef SERVER_BUILD
#include "SettingsScene.h"
#include "AssetManager.h"
#include "SceneManager.h"
#include "TextInput.h"
#include "Toggle.h"
#include "Button.h"
#include "ConfigManager.h"

SettingsScene::SettingsScene(sf::RenderWindow* _window) : Scene(_window, "SettingsScene") {
    title.setString(L"设置");
    title.setFont(AssetManager::getInstance().getFont());
    title.setCharacterSize(48);
    title.setFillColor(sf::Color::White);
    title.setPosition(_window->getSize().x * 0.5f - title.getGlobalBounds().width * 0.5f, 50.f);
}

void SettingsScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;
    initScene();
}

static sf::Text makeLabel(const sf::String& str, float x, float y) {
    sf::Text t;
    t.setFont(AssetManager::getInstance().getFont());
    t.setCharacterSize(20);
    t.setFillColor(sf::Color::White);
    t.setString(str);
    t.setPosition(x, y);
    return t;
}

static sf::Text makeGroupTitle(const sf::String& str, float x, float y) {
    sf::Text t;
    t.setFont(AssetManager::getInstance().getFont());
    t.setCharacterSize(28);
    t.setFillColor(sf::Color(180, 220, 255));
    t.setString(str);
    t.setPosition(x, y);
    return t;
}

void SettingsScene::initScene() {
    const float winW = static_cast<float>(window->getSize().x);
    const float labelX = winW * 0.25f;
    const float inputX = winW * 0.5f;
    const float inputW = 250.f;
    const float inputH = 36.f;
    const float rowH = 50.f;
    const float groupGap = 30.f;

    float y = 140.f;

    // ── 窗口设置 ──
    labels.push_back(makeGroupTitle(L"窗口设置(重启生效)", labelX, y));
    y += 50.f;

    labels.push_back(makeLabel(L"宽度", labelX, y + 8.f));
    widthInput = std::make_shared<TextInput>(inputX, y, inputW, inputH, "1200");
    widthInput->setString(std::to_string(CONFIG.window.width));
    widthInput->setAllowedChars("0123456789");
    addObject(widthInput);
    y += rowH;

    labels.push_back(makeLabel(L"高度", labelX, y + 8.f));
    heightInput = std::make_shared<TextInput>(inputX, y, inputW, inputH, "960");
    heightInput->setString(std::to_string(CONFIG.window.height));
    heightInput->setAllowedChars("0123456789");
    addObject(heightInput);
    y += rowH;

    labels.push_back(makeLabel(L"帧率上限", labelX, y + 8.f));
    fpsInput = std::make_shared<TextInput>(inputX, y, inputW, inputH, "165");
    fpsInput->setString(std::to_string(CONFIG.window.fps));
    fpsInput->setAllowedChars("0123456789");
    addObject(fpsInput);
    y += rowH + groupGap;

    // ── 网络设置 ──
    labels.push_back(makeGroupTitle(L"网络设置", labelX, y));
    y += 50.f;

    labels.push_back(makeLabel(L"服务器 IP", labelX, y + 8.f));
    ipInput = std::make_shared<TextInput>(inputX, y, inputW, inputH, "127.0.0.1");
    ipInput->setString(CONFIG.network.serverIp);
    ipInput->setAllowedChars("0123456789.");
    addObject(ipInput);
    y += rowH;

    labels.push_back(makeLabel(L"端口", labelX, y + 8.f));
    portInput = std::make_shared<TextInput>(inputX, y, inputW, inputH, "6666");
    portInput->setString(std::to_string(CONFIG.network.port));
    portInput->setAllowedChars("0123456789");
    addObject(portInput);
    y += rowH + groupGap;

    // ── 游戏设置 ──
    labels.push_back(makeGroupTitle(L"游戏设置", labelX, y));
    y += 50.f;

    labels.push_back(makeLabel(L"调试模式", labelX, y + 8.f));
    debugToggle = std::make_shared<Toggle>(inputX, y + 3.f, 60.f, 30.f, CONFIG.game.debug);
    addObject(debugToggle);
    y += rowH + groupGap * 2;

    // ── 按钮 ──
    const float btnW = 150.f;
    const float btnH = 50.f;
    const float btnSpacing = 40.f;

    auto saveBtn = std::make_shared<Button>(
        winW * 0.5f - btnW - btnSpacing * 0.5f, y, btnW, btnH, L"保存");
    saveBtn->setOnClick([this]() {
        // 读取输入值写入 CONFIG
        auto toUint = [](const sf::String& s, unsigned fallback) -> unsigned {
            try { return static_cast<unsigned>(std::stoi(s.toAnsiString())); }
            catch (...) { return fallback; }
        };
        auto toInt = [](const sf::String& s, int fallback) -> int {
            try { return std::stoi(s.toAnsiString()); }
            catch (...) { return fallback; }
        };

        CONFIG.window.width = toUint(widthInput->getString(), CONFIG.window.width);
        CONFIG.window.height = toUint(heightInput->getString(), CONFIG.window.height);
        CONFIG.window.fps = toInt(fpsInput->getString(), CONFIG.window.fps);
        CONFIG.network.serverIp = ipInput->getString().toAnsiString();
        CONFIG.network.port = toInt(portInput->getString(), CONFIG.network.port);
        CONFIG.game.debug = debugToggle->getState();

        CONFIG.save();
        getSceneManager()->loadScene("MenuScene");
    });
    addObject(saveBtn);

    auto backBtn = std::make_shared<Button>(
        winW * 0.5f + btnSpacing * 0.5f, y, btnW, btnH, L"返回");
    backBtn->setOnClick([this]() {
        getSceneManager()->loadScene("MenuScene");
    });
    addObject(backBtn);
}

void SettingsScene::update(sf::Time deltaTime) {
    Scene::update(deltaTime);
}

void SettingsScene::render(sf::RenderWindow* _window) {
    Scene::render(_window);
    _window->draw(title);
    for (auto& label : labels) {
        _window->draw(label);
    }
}

#endif
