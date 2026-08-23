//
// Created by MINEC on 2026/6/2.
//

#include <GameEngine.h>
#include "SceneManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <chrono>
#include <thread>
#ifndef SERVER_BUILD
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
    // SDL3 迁移 Step 6a：场景统一持有 Renderer（内部过渡期仍可取 sf::RenderWindow*）
    scene_manager->addScene<GameScene>(&renderer);
    scene_manager->addScene<GameScene3D>(&renderer);
    scene_manager->addScene<SuperMarioScene>(&renderer);
    scene_manager->addScene<MenuScene>(&renderer);
    scene_manager->addScene<SettingsScene>(&renderer);
    scene_manager->addScene<PhysicsTestScene>(&renderer);
    scene_manager->loadScene("MenuScene");
#else
    scene_manager->addScene<SuperMarioScene>();
    scene_manager->loadScene("SuperMarioScene");
#endif
}

#ifndef SERVER_BUILD
void GameEngine::start() {
    renderer.setFramerateLimit(CONFIG.window.fps);
    // SDL3 迁移 6e：计时改 std::chrono（sf::Clock 属 sfml-system，终态要删；
    // eng::Time 目前仍是 sf::Time 别名，Step 10 自研后此处只换包装）
    auto last = std::chrono::steady_clock::now();
    while (renderer.isWindowOpen()) {
        const auto now = std::chrono::steady_clock::now();
        const eng::Time deltaTime = sf::seconds(
            std::chrono::duration<float>(now - last).count());
        last = now;
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
        scene_manager->render(renderer);
        renderer.present();
    }
}
#else
[[noreturn]] void GameEngine::start() {
    // SDL3 迁移 6e：服务端固定节拍计时改 std::chrono（原 sf::Clock + sf::sleep）
    auto last = std::chrono::steady_clock::now();
    const auto frameTime = std::chrono::duration<float>(1.0f / static_cast<float>(CONFIG.window.fps));

    while (true) {
        const auto frameStart = std::chrono::steady_clock::now();
        const eng::Time deltaTime = sf::seconds(
            std::chrono::duration<float>(frameStart - last).count());
        last = frameStart;
        scene_manager->update(deltaTime);

        // 帧耗时不足目标节拍则睡眠剩余时间
        if (const auto elapsed = std::chrono::steady_clock::now() - frameStart; frameTime > elapsed) {
            std::this_thread::sleep_for(frameTime - elapsed);
        }
    }
}
#endif


