//
// Created by MINEC on 2026/6/2.
//

#include <GameEngine.h>
#include "SceneManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#ifndef SERVER_BUILD
#include <SFML/Graphics.hpp>
#include "GameScene.h"
#include "GameScene3D.h"
#include "MenuScene.h"
#include "SettingsScene.h"
#include "PhysicsTestScene.h"
#include "Core/EventConvertSFML.h"
#endif
#include "SuperMarioScene.h"
#include "FrameManager.h"
#include "Core/Types.h"

static std::filesystem::path getExeDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::canonical(path).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}

GameEngine::~GameEngine() {
#ifndef SERVER_BUILD
    delete window;
#endif
}

void GameEngine::init() {
    // 更改工作目录为主程序所在目录
    LOG_INFO_FMT("Program directory: {}", getExeDir().generic_string());
    std::filesystem::current_path(getExeDir());

    Logger::getInstance().setLogFile("log.txt");
    Logger::getInstance().setLogLevel(LogLevel::Debug); // 只显示 Debug 及以上级别
#ifndef SERVER_BUILD
    LOG_INFO("GAME START!");
#else
    LOG_INFO("GAME SERVER START!");
#endif

    if (!CONFIG.load()) {
        LOG_WARN("Config file load fail, using default config");
    }

#ifndef SERVER_BUILD
    // 加载 SuperMarioScene 的资源
    LOG_INFO("Loading SuperMarioScene resources...");
    AssetManager::getInstance().loadTexture(CONFIG.getTexturePath("superMario").c_str());
    AssetManager::getInstance().loadSoundBuffer(CONFIG.getSoundPath("superMario").c_str());
    FrameManager::getInstance().loadFrame();
    LOG_INFO("SuperMarioScene resources loaded.");

    if (!window) window = new sf::RenderWindow(
        sf::VideoMode(CONFIG.window.width, CONFIG.window.height), CONFIG.window.title);
    // 注册轮询窗口：eng::Input::getMousePosition 依赖（SDL3 迁移 Step 3）
    eng::detail::setInputWindow(window);
#endif
    scene_manager = std::make_shared<SceneManager>();
#ifndef SERVER_BUILD
    scene_manager->addScene<GameScene>(window);
    scene_manager->addScene<GameScene3D>(window);
    scene_manager->addScene<SuperMarioScene>(window);
    scene_manager->addScene<MenuScene>(window);
    scene_manager->addScene<SettingsScene>(window);
    scene_manager->addScene<PhysicsTestScene>(window);
    scene_manager->loadScene("MenuScene");
#else
    scene_manager->addScene<SuperMarioScene>();
    scene_manager->loadScene("SuperMarioScene");
#endif
}

#ifndef SERVER_BUILD
void GameEngine::start() const {
    window->setFramerateLimit(CONFIG.window.fps);
    sf::Clock clock;
    while (window->isOpen()) {
        const eng::Time deltaTime = clock.restart();
        sf::Event event{};
        while (window->pollEvent(event)) {
            // sf::Event → EngineEvent 转换（SFML 特有事件被过滤）
            const auto engineEvent = eng::toEngineEvent(event);
            if (!engineEvent) continue;
            if (engineEvent->type == eng::EventType::WindowClose) {
                window->close();
                break;
            }
            scene_manager->handleEvent(*engineEvent);
        }
        if (!window->isOpen()) break;
        scene_manager->update(deltaTime);
        window->clear();
        scene_manager->render(window);
        window->display();
    }
}
#else
[[noreturn]] void GameEngine::start() const {
    sf::Clock clock;
    const float targetFPS = CONFIG.window.fps;
    const eng::Time frameTime = sf::seconds(1.0f / targetFPS);

    while (true) {
        const eng::Time deltaTime = clock.restart();
        scene_manager->update(deltaTime);

        // 计算帧时间并睡眠剩余时间
        if (const eng::Time sleepTime = frameTime - clock.getElapsedTime(); sleepTime > eng::Time::Zero) {
            sf::sleep(sleepTime);
        }
    }
}
#endif


