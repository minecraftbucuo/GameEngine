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
        if (listener) NET_DestroyServer(listener);
        NET_Quit();   // 与 startServer/connectToServer 的 NET_Init 配对
    }

    NetworkType getNetworkType() const {
        return network_type;
    }

    void setCurrentScene(Scene* scene) { current_scene = scene; }
    Scene* getCurrentScene() const { return current_scene; }

    bool startServer();

    bool connectToServer(const std::string& address);

    void update(const eng::Time& deltaTime);

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
    Scene* current_scene{};
};
