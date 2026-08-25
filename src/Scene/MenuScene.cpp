//
// Created by MINEC on 2026/6/2.
//
#include "Core/Types.h"
#ifndef SERVER_BUILD
#include "MenuScene.h"
#include "AssetManager.h"
#include "SuperMarioScene.h"
#include "Button.h"
#include "Render/Renderer.h"
#include "SceneManager.h"
#include "Logger.h"
#include <algorithm>
#include <random>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <cstdlib>

// N5 自动寻址：页面 http(s) 来源 → ws(s)://host:port。
// 用于「页面与桥同端口」拓扑（start_bridge.ps1 / S2 nginx）：构建一次的 WEB 包
// 在任意开服者机器上零配置可用，无需按开服者 IP 重打 config。
// stringToNewUTF8 内部走 malloc，返回值必须 free（emscripten EM_ASM 文档约定）
static std::string pageWebSocketOrigin() {
    char* s = static_cast<char*>(EM_ASM_PTR(
        { return stringToNewUTF8((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host); }));
    if (!s) return {};
    std::string out(s);
    std::free(s);
    return out;
}

// 页面的主机名（不含端口）：判断页面是从本机还是远程服务器打开的
static std::string pageHostName() {
    char* s = static_cast<char*>(EM_ASM_PTR(
        { return stringToNewUTF8(location.hostname); }));
    if (!s) return {};
    std::string out(s);
    std::free(s);
    return out;
}

// 自动寻址决策：页面从本机打开（本机开发流，页面与桥常分端口）→ 连本机桥端口；
// 页面从远程打开（部署拓扑，桥的 --web 顺带发页面，同端口）→ 连页面自身来源。
// 这样 config.json 的 serverIp 保持 127.0.0.1 不用改：本机连本机桥，
// 部署后别人打开页面自动连到开服者，无需按 IP 重打 WEB 包。
static std::string resolveWebServerAddr() {
    const std::string host = pageHostName();
    if (host.empty() || host == "127.0.0.1" || host == "localhost" || host == "[::1]") {
        return "ws://127.0.0.1:" + std::to_string(CONFIG.network.webBridgePort);
    }
    return pageWebSocketOrigin();
}
#endif

MenuScene::MenuScene(eng::Renderer* _renderer) : Scene(_renderer, "MenuScene") {
    font = AssetManager::getInstance().getFontHandle();
    const eng::Vec2f titleSize = _renderer->measureText(font, titleText, TITLE_FONT_SIZE);
    titlePos = eng::Vec2f(_renderer->getSize().x * 0.5f - titleSize.x * 0.5f,
                          static_cast<float>(_renderer->getSize().y) * 0.18f);
}

void MenuScene::init() {
    Scene::init();
    if (is_init) return;
    is_init = true;
    initScene();
}

void MenuScene::initScene() {
    const float winW = static_cast<float>(renderer->getSize().x);
    const float winH = static_cast<float>(renderer->getSize().y);
    const float btnW = 280.f;
    const float btnH = 55.f;
    const float startY = winH * 0.45f;
    const float spacing = 35.f;

    auto makeButton = [&](const std::string& label, int index, auto&& callback) {
        auto btn = std::make_shared<Button>(0, 0, btnW, btnH, label);
        btn->setOnClick(std::forward<decltype(callback)>(callback));
        btn->setToRectCenter(0, startY + index * (btnH + spacing), winW, btnH);
        this->addObject(btn);
    };

    // WASM 移植 Step 5：WEB 无裸 socket，单机入口走 NetworkManager Local 模式
    // 联机 N3：Client 入口正式化——地址读 config.json（serverIp 支持完整 ws(s)://
    // URL 直连；否则按「ws://serverIp:webBridgePort」拼桥地址，port 键归桌面直连）
    int btnIndex = 0;
#ifdef __EMSCRIPTEN__
    makeButton("超级玛丽（单机）", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->startServer();
    });

    makeButton("超级玛丽 Client", btnIndex++, [&]() -> void {
        std::string addr = CONFIG.network.serverIp;
        // auto / 127.0.0.1 / localhost：自动寻址——本机打开的页面连本机桥，
        // 远程打开的页面连页面来源（详见 resolveWebServerAddr 注释）。
        // config.json 因此可以永远保持 127.0.0.1，部署不用重打 WEB 包。
        if (addr == "auto" || addr == "127.0.0.1" || addr == "localhost") {
            addr = resolveWebServerAddr();
            if (addr.empty()) LOG_WARN("auto address resolved to empty, page origin unavailable");
        } else if (addr.rfind("ws://", 0) != 0 && addr.rfind("wss://", 0) != 0) {
            addr = "ws://" + addr + ":" + std::to_string(CONFIG.network.webBridgePort);
        }
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->connectToServer(addr);
    });
#else
    makeButton("超级玛丽 Client", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->connectToServer(
            CONFIG.network.serverIp);
    });

    makeButton("超级玛丽 Server", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("SuperMarioScene");
        std::dynamic_pointer_cast<SuperMarioScene>(getSceneManager()->getCurrentScene())->startServer();
    });
#endif

    makeButton("3D 渲染", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("GameScene3D");
    });

    makeButton("Demo", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("GameScene");
    });

    makeButton("设置", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("SettingsScene");
    });

    makeButton("物理测试", btnIndex++, [&]() -> void {
        getSceneManager()->loadScene("PhysicsTestScene");
    });

    // 初始化背景粒子
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> distX(0.f, winW);
    std::uniform_real_distribution<float> distY(0.f, winH);
    std::uniform_real_distribution<float> distV(-20.f, 20.f);
    std::uniform_real_distribution<float> distR(3.f, 10.f);
    std::uniform_real_distribution<float> distAlpha(60.f, 180.f);
    std::uniform_real_distribution<float> distAlphaSpeed(10.f, 30.f);

    for (int i = 0; i < 80; ++i) {
        Particle p;
        p.radius = distR(rng);
        p.pos = {distX(rng), distY(rng)};
        p.velocity = {distV(rng), distV(rng)};
        p.alpha = distAlpha(rng);
        p.alphaSpeed = distAlphaSpeed(rng);
        particles.push_back(p);
    }
}

void MenuScene::update(eng::Time deltaTime) {
    Scene::update(deltaTime);

    const float winW = static_cast<float>(renderer->getSize().x);
    const float winH = static_cast<float>(renderer->getSize().y);
    const float dt = deltaTime.asSeconds();

    for (auto& p : particles) {
        p.pos += p.velocity * dt;

        // 呼吸效果：alpha 缓慢变化
        p.alpha += p.alphaSpeed * dt;
        if (p.alpha > 120.f || p.alpha < 20.f) p.alphaSpeed = -p.alphaSpeed;
        p.alpha = std::clamp(p.alpha, 0.f, 255.f);

        // 超出边界则从另一侧进入
        if (p.pos.x < -10.f) p.pos.x = winW;
        else if (p.pos.x > winW + 10.f) p.pos.x = -10.f;
        if (p.pos.y < -10.f) p.pos.y = winH;
        else if (p.pos.y > winH + 10.f) p.pos.y = -10.f;
    }
}

void MenuScene::render(eng::Renderer& _renderer) {
    // 背景粒子（呼吸 alpha，颜色与迁移前一致）
    for (const auto& p : particles) {
        _renderer.drawCircle(p.pos, p.radius, eng::Color(130, 200, 255,
                                static_cast<eng::Uint8>(p.alpha)));
    }
    renderObjects(_renderer);
    _renderer.drawText(font, titleText, titlePos, TITLE_FONT_SIZE, eng::Color::Yellow);
}
#endif
