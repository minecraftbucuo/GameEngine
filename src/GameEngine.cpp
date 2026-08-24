//
// Created by MINEC on 2026/6/2.
//

#include <GameEngine.h>
#include "SceneManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <chrono>
#include <thread>
#include <algorithm>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
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
#if defined(__EMSCRIPTEN__)
    // WEB 构建：无 exe 概念，Asset 已由 --preload-file 挂载到 MEMFS 根（/Asset），
    // 工作目录固定为 "/" 即可让 "./Asset/..." 相对路径照常解析
    return std::filesystem::path("/");
#elif defined(_WIN32)
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
    // SDL3 迁移 Step 6a：场景统一持有 Renderer
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
    lastFrameTime = std::chrono::steady_clock::now();
#ifdef __EMSCRIPTEN__
    // WASM 移植 Step 3：rAF 天然按刷新率调度，内部限帧必须关闭
    renderer.setFramerateLimit(0);
    // fps=0 → requestAnimationFrame；simulate_infinite_loop 使 main 交还控制权后不再返回，
    // frameStep 返回 false（关窗）时由 cancel 停掉回调
    emscripten_set_main_loop_arg(
        [](void* arg) {
            if (!static_cast<GameEngine*>(arg)->frameStep()) {
                emscripten_cancel_main_loop();
            }
        },
        this, /*fps=0→rAF*/ 0, /*simulate_infinite_loop*/ 1);
#else
    // SDL3 迁移 Step 10：计时 std::chrono + 自研 eng::Time（sfml-system 依赖解除）
    renderer.setFramerateLimit(CONFIG.window.fps);
    while (frameStep()) {}
#endif
}

bool GameEngine::frameStep() {
    const auto now = std::chrono::steady_clock::now();
    // dt 上限钳制 50ms：切后台/切标签页回来会产生巨帧导致瞬移穿墙（两平台共同防护）
    const float dtSec = std::min(
        std::chrono::duration<float>(now - lastFrameTime).count(), 0.05f);
    lastFrameTime = now;
    const eng::Time deltaTime = eng::Time::seconds(dtSec);

    eng::EngineEvent event{};
    while (renderer.pollEvent(event)) {
        if (event.type == eng::EventType::WindowClose) {
            renderer.closeWindow();
            return false;
        }
        scene_manager->handleEvent(event);
    }
    if (!renderer.isWindowOpen()) return false;

    scene_manager->update(deltaTime);
    renderer.clear();
    scene_manager->render(renderer);
    renderer.present();
    return true;
}
#else
[[noreturn]] void GameEngine::start() {
    // SDL3 迁移 6e：服务端固定节拍计时改 std::chrono（原 sf::Clock + sf::sleep）
    auto last = std::chrono::steady_clock::now();
    const auto frameTime = std::chrono::duration<float>(1.0f / static_cast<float>(CONFIG.window.fps));

    while (true) {
        const auto frameStart = std::chrono::steady_clock::now();
        const eng::Time deltaTime = eng::Time::seconds(
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


