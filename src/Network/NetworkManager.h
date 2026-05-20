//
// Created by MINEC on 2026/5/8.
//

#pragma once
#include <memory>
#include "ISerializable.h"
#include <string>
#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include "ConfigManager.h"
#include "GameObject.h"
#include "TcpClient.h"

class NetworkManager {
public:
    enum class NetworkType : uint8_t {
        None,
        Server,
        Client
    };
    NetworkManager() = default;
    ~NetworkManager() = default;

    NetworkType getNetworkType() const {
        return network_type;
    }

    bool startServer();

    bool connectToServer(const std::string& address);

    void update(const sf::Time& deltaTime);

    void handleEvent(const sf::Event& event);

    void serverUpdate(const sf::Time& deltaTime);

    void receiveNewConnection();

    void clientUpdate(const sf::Time& deltaTime);

    void addGameObjectAndSync(const std::shared_ptr<GameObject>& obj);

    void createNewPlayer(std::shared_ptr<TcpClient> newClient);

    void verifyClient();

    void respawnPlayer(const std::shared_ptr<TcpClient>& client);

    void addGameObject(const std::shared_ptr<GameObject>& obj);

private:
    inline static const std::string CLIENT_TOKEN = "minecraftbucuo/mario";
    inline static const sf::Time VERIFY_TIMEOUT = sf::seconds(10);

    NetworkType network_type = NetworkType::None;
    unsigned int port = CONFIG.network.port;
    TcpClient clientSocket;
    sf::TcpListener listener;
    std::vector<std::shared_ptr<TcpClient>> clients;
    std::vector<std::pair<TcpClient, sf::Clock>> unverified;
    std::unordered_map<TcpClient*, std::weak_ptr<ISerializable>> players;
    // 需要同步的游戏对象
    std::vector<std::weak_ptr<ISerializable>> game_objects;
    int past_time = 0;
};
