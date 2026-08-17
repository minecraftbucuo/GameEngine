//
// Created by MINEC on 2026/5/8.
//

#include "NetworkManager.h"

#include <ranges>

#include "Logger.h"
#include "Scene.h"
#include "NetworkProtocol.h"

bool NetworkManager::startServer() {
    if (network_type == NetworkType::Server) return true;
    if (network_type == NetworkType::Client) {
        LOG_INFO("Cannot start server while already running as a client!");
        return false;
    }
    LOG_INFO_FMT("Starting server on port {} ...", port);

    if (listener.listen(port) != sf::Socket::Done) {
        LOG_WARN("Failed to start server");
        return false;
    }

    listener.setBlocking(false);
    network_type = NetworkType::Server;
    LOG_INFO("Server started successfully!");
    return true;
}

bool NetworkManager::connectToServer(const std::string& address) {
    if (network_type == NetworkType::Client) return true;
    if (network_type == NetworkType::Server) {
        LOG_WARN("Cannot connect to server while already running as a server!");
        return false;
    }
    LOG_INFO_FMT("Connecting to server at {}:{}", address, port);

    if (clientSocket.connect(address, port,
                             sf::seconds(CONFIG.network.timeout)) != sf::Socket::Done) {
        LOG_WARN("Failed to connect to server!");
        return false;
    }

    // 发送验证 Packet
    sf::Packet verifyPacket;
    verifyPacket << CLIENT_TOKEN;
    clientSocket.getSocket().send(verifyPacket);

    // 等待服务端返回验证结果
    sf::Packet resultPacket;
    if (clientSocket.receive(resultPacket) != sf::Socket::Done) {
        LOG_WARN("Failed to receive verifying response!");
        return false;
    }
    bool success;
    std::string message;
    resultPacket >> success >> message;
    if (!success) {
        LOG_WARN_FMT("Verification failed: {}", message);
        return false;
    }
    LOG_INFO_FMT("Verification succeeded: {}", message);

    clientSocket.setBlocking(false);
    network_type = NetworkType::Client;
    LOG_INFO("Connected to server successfully!");
    return true;
}

void NetworkManager::update(const sf::Time& deltaTime) {
    if (network_type == NetworkType::None) return;
    if (network_type == NetworkType::Server) {
        serverUpdate(deltaTime);
        // 将收集到的要发送的数据一次性发出去
        for (const auto& client : clients) {
            client->send();
        }
    }
    else {
        clientUpdate(deltaTime);
        clientSocket.send();
    }
}

void NetworkManager::handleEvent(const sf::Event& event) {
    if (network_type != NetworkType::None && event.type == sf::Event::Closed) {
        if (network_type == NetworkType::Server) {
            for (const auto& client : clients) {
                client->disconnect();
            }
            clients.clear();
        }
        else {
            clientSocket.disconnect();
        }
    }
    if (network_type == NetworkType::Client) {
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::R) {
                if (players[&clientSocket].expired()) {
                    sf::Packet packet;
                    packet << NetworkMsg::ClientRespawn;
                    clientSocket.append(packet);
                }
            }
        }
    }
}

void NetworkManager::receiveNewConnection() {
    if (const auto newClient = TcpClient(); listener.accept(newClient.getSocket()) == sf::Socket::Done) {
        newClient.setBlocking(false);
        unverified.emplace_back(newClient, sf::Clock());
        LOG_INFO_FMT("New TCP connection established with {}:{}",
            newClient.getSocket().getRemoteAddress().toString(),
            newClient.getSocket().getRemotePort());
    }
}

// 初始化客户端的场景并且将 newClient 加入到 clients vector 中
void NetworkManager::initClientScene(const std::shared_ptr<TcpClient>& newClient) {
    // 给客户端发送当前场景信息
    for (auto it = game_objects.begin(); it != game_objects.end();) {
        if (const auto obj = it->lock()) {
            sf::Packet packet;
            obj->serialize(packet, NetworkMsg::SpawnObject);
            newClient->append(packet);
            ++it;
        } else {
            it = game_objects.erase(it);
        }
    }

    // 创建新加入的玩家
    // spawnEntityWithNetwork 方法会广播生成新对象的消息给clients vector里的所有客户端
    const auto newPlayer = current_scene->spawnEntityWithNetwork();
    players[newClient.get()] = std::dynamic_pointer_cast<ISerializable>(newPlayer);

    sf::Packet packet;
    // 发送新玩家信息给新玩家自己
    players[newClient.get()].lock()->serialize(packet, NetworkMsg::SpawnPlayer);
    newClient->append(packet);

    clients.emplace_back(newClient);

#ifndef SERVER_BUILD
    LOG_INFO_FMT("New client connected! Total number of players: {}", clients.size() + 1u);
#else
    LOG_INFO_FMT("New client connected! Total number of players: {}", clients.size());
#endif
}

void NetworkManager::verifyClient() {
    // 验证 TCP 连接是否由 Mario 客户端发起
    // 并且检查客户端是否连接过长时间而没有发送任何 Packet
    for (auto it = unverified.begin(); it != unverified.end();) {
        auto& [client, clock] = *it;
        if (sf::Packet verifyPacket; client.receive(verifyPacket) == sf::Socket::Done) {
            std::string token;
            verifyPacket >> token;
            if (token == CLIENT_TOKEN) {
                sf::Packet resultPacket;
                constexpr bool success = true;
                std::string message = "Hello brave Mario!";
                resultPacket << success << message;
                client.getSocket().send(resultPacket);
                initClientScene(std::make_shared<TcpClient>(client));
                it = unverified.erase(it); // 已验证，从未验证列表中删除
            } else {
                client.disconnect(); // 直接关闭连接，拒绝其他客户端
                it = unverified.erase(it);
                LOG_WARN_FMT("Wrong client token with '{}'", token);
            }
        } else if (const sf::Time elapsed = clock.getElapsedTime(); elapsed >= VERIFY_TIMEOUT) {
            client.disconnect(); // 关闭超时连接，防止 DDoS
            it = unverified.erase(it);
            LOG_WARN("No verify packet sent from client and time out");
        } else {
            ++it;
        }
    }
}

void NetworkManager::respawnPlayer(const std::shared_ptr<TcpClient>& client) {
    // 创建重生玩家
    const auto newPlayer = current_scene->spawnEntity();
    players[client.get()] = std::dynamic_pointer_cast<ISerializable>(newPlayer);

    // 发送玩家重生信息给其他客户端
    sf::Packet packet;
    players[client.get()].lock()->serialize(packet, NetworkMsg::SpawnObject);
    for (const auto& _client : clients) {
        if (_client == client) continue;
        _client->append(packet);
    }

    packet.clear();
    // 发送重生玩家信息给玩家自己
    players[client.get()].lock()->serialize(packet, NetworkMsg::SpawnPlayer);
    client->append(packet);

    addGameObject(newPlayer);
}

void NetworkManager::serverUpdate(const sf::Time& deltaTime) {
    // 接收新用户连接
    receiveNewConnection();
    // 验证 TCP 客户端是否为 Mario 客户端
    verifyClient();
    // 处理客户端数据
    std::unordered_map<unsigned int, bool> removeIdsMap; // 存储需要删除的玩家id
    for (auto it = clients.begin(); it != clients.end();) {
        const auto& client = *it;

        const auto player = players[client.get()].lock();
        // 如果玩家死亡会导致这个弱指针失效
        if (!player) {
            LOG_TRACE("client player died and weak ptr is invalid");
            players.erase(client.get());
        }

        sf::Packet packet;
        sf::Socket::Status status = client->receive(packet);
        // 客户端断开连接处理
        if (status == sf::Socket::Error || status == sf::Socket::Disconnected) {
            if (player) {
                const unsigned int id = player->getNetworkId();
                removeIdsMap[id] = true;
                players.erase(client.get());
                player->disconnect();
                it = clients.erase(it);
                LOG_DEBUG("client removed in clients vector");
                continue;
            }
            // 失效了也需要移除，因为client断开了
            it = clients.erase(it);
            LOG_INFO("client game object is released");
            continue;
        }
        // 处理玩家的输入操作
        while (status == sf::Socket::Done) {
            // 客户端会把一帧内产生的多个输入消息合并进同一个 Packet。
            // 这里必须把 Packet 内的消息全部读完，否则松开按键等后续输入会被忽略。
            NetworkMsg msg_type;
            while (packet >> msg_type) {
                if (msg_type == NetworkMsg::ClientRespawn) {
                    LOG_INFO("client request to respawn");
                    respawnPlayer(client);
                } else if (msg_type == NetworkMsg::ClientInput) {
                    if (player) {
                        player->deserialize(packet);
                    }
                }
            }
            status = client->receive(packet);
        }
        ++it;
    }
    // 在 game_objects 中删除已销毁的对象
    std::erase_if(game_objects, [](const auto& obj) -> bool {
        return obj.expired();
    });

    // 通知所有在线玩家删除不在线的玩家
    for (const auto& client : clients) {
        for (const auto& id : removeIdsMap | std::views::keys) {
            sf::Packet packet;
            packet << NetworkMsg::RemoveObject << id;
            client->append(packet);
        }
    }

    // 向客户端同步数据
    past_time += deltaTime.asMilliseconds();
    if (past_time < 1000 / CONFIG.network.tickRate) return;
    past_time = 0;
    LOG_TRACE("Sending update packets to clients");
    for (const auto& client : clients) {
        for (const auto& obj : game_objects) {
            sf::Packet packet;
            obj.lock()->serialize(packet, NetworkMsg::UpdateObject);
            if (packet.getDataSize() == 0) {
                continue;
            }
            client->append(packet);
        }
    }
}

void NetworkManager::clientUpdate(const sf::Time& deltaTime) {
    sf::Packet packet;
    sf::Socket::Status status = clientSocket.receive(packet);
    if (status == sf::Socket::Error || status == sf::Socket::Disconnected) {
        network_type = NetworkType::None;
        LOG_WARN("Server disconnected");
        return;
    }
    while (status == sf::Socket::Done) {
        NetworkMsg type;
        while (packet >> type) {
            if (type == NetworkMsg::SpawnObject || type == NetworkMsg::SpawnPlayer) {
                LOG_INFO_FMT("Received packet, type: SpawnObject, IsPlayer: {}", type == NetworkMsg::SpawnPlayer);
                auto obj = std::dynamic_pointer_cast<ISerializable>(
                    current_scene->spawnEntityWithNetwork(packet));
                if (type == NetworkMsg::SpawnPlayer) {
                    players[&clientSocket] = std::move(obj);
                }
            }
            else if (type == NetworkMsg::UpdateObject) {
                LOG_TRACE("Received packet, type: UpdateObject");
                unsigned int id;
                packet >> id;
                const std::shared_ptr<ISerializable>& obj = std::dynamic_pointer_cast<ISerializable>(
                    current_scene->findGameObjectById(id));
                if (!obj) {
                    LOG_ERROR_FMT("Object with ID {} are not found", id);
                    packet.clear();
                    continue;
                }
                obj->deserialize(packet);
            }
            else if (type == NetworkMsg::RemoveObject) {
                LOG_TRACE("Received packet, type: RemoveObject");
                unsigned int id;
                packet >> id;
                current_scene->findGameObjectById(id)->destroy();
                current_scene->removeObjectById(id);
            }
            else if (type == NetworkMsg::SpawnFireBall) {
                LOG_TRACE("Received packet, type: SpawnFireBall");
                current_scene->spawnEntityWithNetwork(packet);
            }
        }
        status = clientSocket.receive(packet);
    }
}

void NetworkManager::addGameObjectAndSync(const std::shared_ptr<GameObject>& obj) {
    const auto& serializable_obj = std::dynamic_pointer_cast<ISerializable>(obj);
    game_objects.emplace_back(serializable_obj);
    if (this->network_type == NetworkType::Server) {
        // 向所有客户端广播新对象
        for (const auto& client : clients) {
            sf::Packet spawn_packet;
            serializable_obj->serialize(spawn_packet, NetworkMsg::SpawnObject);
            client->append(spawn_packet);
        }
    }
}

void NetworkManager::addGameObject(const std::shared_ptr<GameObject>& obj) {
    game_objects.emplace_back(std::dynamic_pointer_cast<ISerializable>(obj));
}

bool NetworkManager::isClient() const {
    return this->network_type == NetworkType::Client;
}

TcpClient& NetworkManager::getClientSocket() {
    return clientSocket;
}
