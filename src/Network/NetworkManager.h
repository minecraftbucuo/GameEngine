//
// Created by MINEC on 2026/5/8.
//

#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include "ISerializable.h"
#include <string>
#include <SDL3_net/SDL_net.h>   // N4：listener 用 NET_Server，传输层全 SDL_net

#include "ConfigManager.h"
#include "GameObject.h"
#include "TcpClient.h"
#include "Core/Event.h"

class Scene;

class NetworkManager {
public:
    enum class NetworkType : uint8_t {
        None,
        Server,
        Client,
        Local   // WASM 移植 Step 5：WEB 无裸 socket，本地单机=服务端逻辑+零同步
    };
    NetworkManager() = default;
    ~NetworkManager() {
#ifndef __EMSCRIPTEN__
        // WEB 下从未 NET_Init/NET_CreateServer（NET_Init 起线程必崩，见 N1 结论），
        // 无可清理资源；NET_Quit 严禁在未 Init 状态下调用
        if (listener) NET_DestroyServer(listener);
        NET_Quit();   // 与 startServer/connectToServer 的 NET_Init 配对
#endif
    }

    NetworkType getNetworkType() const {
        return network_type;
    }

    void setCurrentScene(Scene* scene) { current_scene = scene; }
    Scene* getCurrentScene() const { return current_scene; }

    bool startServer();

    bool connectToServer(const std::string& address);

    void update(const eng::Time& deltaTime);

    // N4 修复：场景重进时清空上一局会话（连接/同步表/标志）。
    // NetworkManager 随场景实例常驻缓存，不重置则旧连接与同步表跨局残留
    void resetSession();

    void handleEvent(const eng::EngineEvent& event);

    void serverUpdate(const eng::Time& deltaTime);

    void receiveNewConnection();

    void clientUpdate(const eng::Time& deltaTime);

    void addGameObjectAndSync(const std::shared_ptr<GameObject>& obj);

    void initClientScene(const std::shared_ptr<TcpClient>& newClient);

    void verifyClient();

    void respawnPlayer(const std::shared_ptr<TcpClient>& client);

    void addGameObject(const std::shared_ptr<GameObject>& obj);

    bool isClient() const;

    // N4 断线反馈：Client→None 转变瞬间置位（仅客户端路径；Server 端逐客户端
    // 断开属正常事件，Local 永不断线，均不触发）。场景每帧轮询驱动提示层
    bool wasConnectionLost() const { return connectionLost; }
    void clearConnectionLost() { connectionLost = false; }

    TcpClient& getClientSocket();

private:
    inline static const std::string CLIENT_TOKEN = "minecraftbucuo/mario";
    inline static const std::chrono::steady_clock::duration VERIFY_TIMEOUT = std::chrono::seconds(10);

    NetworkType network_type = NetworkType::None;
    unsigned int port = CONFIG.network.port;
    TcpClient clientSocket;
    NET_Server* listener = nullptr;   // NET_CreateServer 延迟创建；addr=NULL 监听全部接口
    std::vector<std::shared_ptr<TcpClient>> clients;
    std::vector<std::pair<TcpClient, std::chrono::steady_clock::time_point>> unverified;
    std::unordered_map<TcpClient*, std::weak_ptr<ISerializable>> players;
    // 需要同步的游戏对象
    std::vector<std::weak_ptr<ISerializable>> game_objects;
    std::int64_t tick_accum_us = 0;   // 广播节拍微秒累加器（毫秒截断会把 128Hz 实际跑成 ~83Hz）
    // WEB 联机 N2：connect() 异步化后验证应答后置 —— true 表示首条 bool+string
    // 应答尚未被 clientUpdate 消费（桌面同步路径不使用此标志）
    bool verifyPending = false;
    bool connectionLost = false;   // N4：断线一次性标志，场景重进时 clear
    Scene* current_scene{};
};
