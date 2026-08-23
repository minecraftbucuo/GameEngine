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

GameEngine::~GameEngine() = default;

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

    // SDL3 迁移 Step 5：窗口创建统一走 Renderer（内部注册轮询窗口）
    renderer.createWindow(eng::Vec2u(CONFIG.window.width, CONFIG.window.height), CONFIG.window.title);
#endif
    scene_manager = std::make_shared<SceneManager>();
#ifndef SERVER_BUILD
    // 过渡期：场景仍接收 sf::RenderWindow*（Step 6a 起改为 Renderer&）
    scene_manager->addScene<GameScene>(renderer.getSfmlWindow());
    scene_manager->addScene<GameScene3D>(renderer.getSfmlWindow());
    scene_manager->addScene<SuperMarioScene>(renderer.getSfmlWindow());
    scene_manager->addScene<MenuScene>(renderer.getSfmlWindow());
    scene_manager->addScene<SettingsScene>(renderer.getSfmlWindow());
    scene_manager->addScene<PhysicsTestScene>(renderer.getSfmlWindow());
    scene_manager->loadScene("MenuScene");
#else
    scene_manager->addScene<SuperMarioScene>();
    scene_manager->loadScene("SuperMarioScene");
#endif
}

#ifndef SERVER_BUILD
void GameEngine::start() {
    renderer.setFramerateLimit(CONFIG.window.fps);
    sf::Clock clock;
    while (renderer.isWindowOpen()) {
        const eng::Time deltaTime = clock.restart();
        eng::EngineEvent event{};
        while (renderer.pollEvent(event)) {
            if (event.type == eng::EventType::WindowClose) {
                renderer.closeWindow();
                break;
            }
            scene_manager->handleEvent(event);
        }
        if (!renderer.isWindowOpen()) break;
        scene_manager->update(deltaTime);
        renderer.clear();
        // 过渡期：场景渲染仍走旧 sf::RenderWindow 路径（Step 6a~6e 渐进切换）
        scene_manager->render(renderer.getSfmlWindow());
        renderer.present();
    }
}
#else
[[noreturn]] void GameEngine::start() {
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


