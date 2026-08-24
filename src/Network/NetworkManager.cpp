//
// Created by MINEC on 2026/5/8.
//

#include "NetworkManager.h"

#include <ranges>
#include <thread>

#include "Logger.h"
#include "Scene.h"
#include "NetworkProtocol.h"

bool NetworkManager::startServer() {
    if (network_type == NetworkType::Server) return true;
    if (network_type == NetworkType::Client) {
        LOG_INFO("Cannot start server while already running as a client!");
        return false;
    }
#ifdef __EMSCRIPTEN__
    // WASM 移植 Step 5：浏览器无裸 socket，不创建监听，直接进本地单机模式
    //（游戏逻辑与 Server 一致，仅无网络同步）
    LOG_INFO("WEB build: entering local offline mode (no sockets)");
    network_type = NetworkType::Local;
    return true;
#endif
    LOG_INFO_FMT("Starting server on port {} ...", port);

    // SDL 核心初始化（服务端构建无渲染器路径，SDL_net 的线程/原子依赖它）
    if (!SDL_Init(0)) {
        LOG_WARN("Failed to init SDL for networking");
        return false;
    }
    if (!NET_Init()) {
        LOG_WARN("Failed to init SDL_net");
        return false;
    }

    listener = NET_CreateServer(nullptr, static_cast<Uint16>(port), 0);   // nullptr = 监听全部接口
    if (!listener) {
        LOG_WARN("Failed to start server");
        return false;
    }

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

#ifndef __EMSCRIPTEN__
    // SDL 核心初始化（客户端通常已由渲染器初始化，SDL_Init 幂等无害）。
    // WEB 下必须整体跳过：NET_Init 无条件起解析线程，无 pthread 环境必崩
    //（见 docs/websocket-net-plan.md N1 结论）；客户端传输层由 TcpClient 的 WS 后端自理
    if (!SDL_Init(0) || !NET_Init()) {
        LOG_WARN("Failed to init SDL_net");
        return false;
    }
#endif

    if (clientSocket.connect(address, port, CONFIG.network.timeout) != TcpClient::Status::Done) {
        LOG_WARN("Failed to connect to server!");
        return false;
    }

    // 发送验证 Packet
    eng::Packet verifyPacket;
    verifyPacket << CLIENT_TOKEN;
    clientSocket.sendImmediate(verifyPacket);

#ifdef __EMSCRIPTEN__
    // WEB：握手与收发全异步，无同步等待点 —— 下面那段阻塞轮询（sleep 5ms 循环）
    // 会卡死浏览器事件循环，WS 握手永远完不成。改为乐观置位 Client 后立即返回：
    // - 验证帧此时暂存在发送缓冲，update() 每帧的 send() 在握手 OPEN 后自动冲刷
    // - 验证应答（首条 bool+string）由 clientUpdate 每帧收口（verifyPending）
    // - 桥不通/服务端拒绝时 onclose/onerror 折算为 Disconnected/Error，
    //   clientUpdate 复位 None，与桌面同步路径的 return false 等价
    verifyPending = true;
    network_type = NetworkType::Client;
    LOG_INFO("WEB: WebSocket handshake initiated, verification pending");
    return true;
#else
    // 等待服务端返回验证结果（SDL_net 全异步：轮询等待至超时；
    // SFML 时代此处靠阻塞 socket 天然等待，语义等价）
    eng::Packet resultPacket;
    TcpClient::Status status = TcpClient::Status::NotReady;
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<float>(CONFIG.network.timeout));
    while (std::chrono::steady_clock::now() < deadline
           && status == TcpClient::Status::NotReady) {
        status = clientSocket.receive(resultPacket);
        if (status == TcpClient::Status::NotReady) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    if (status != TcpClient::Status::Done) {
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

    network_type = NetworkType::Client;
    LOG_INFO("Connected to server successfully!");
    return true;
#endif
}

void NetworkManager::resetSession() {
    // 服务端：断开全部客户端并销毁监听（桌面重开服拿到全新 listener）
    if (network_type == NetworkType::Server) {
        for (const auto& client : clients) {
            client->disconnect();
        }
#ifndef __EMSCRIPTEN__
        if (listener) {
            NET_DestroyServer(listener);
            listener = nullptr;
        }
#endif
    } else if (network_type == NetworkType::Client) {
        // 桌面客户端 ESC 离场此前不disconnect，连接悬到进程结束；WEB 下
        // 已被服务端关闭，此处 delete 释放句柄并注销迟到回调
        clientSocket.disconnect();
    }
    clients.clear();
    unverified.clear();
    players.clear();
    game_objects.clear();
    tick_accum_us = 0;
    verifyPending = false;
    connectionLost = false;
    network_type = NetworkType::None;   // 由随后的 startServer/connectToServer 重新置位
}

void NetworkManager::update(const eng::Time& deltaTime) {
    if (network_type == NetworkType::None || network_type == NetworkType::Local) return;
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

void NetworkManager::handleEvent(const eng::EngineEvent& event) {
    // Local 模式无连接资源，跳过断连清理（与 None 同为纯本地状态）
    if ((network_type == NetworkType::Server || network_type == NetworkType::Client)
        && event.type == eng::EventType::WindowClose) {
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
        if (event.type == eng::EventType::KeyPress) {
            if (event.key == eng::Key::R) {
                if (players[&clientSocket].expired()) {
                eng::Packet packet;
                packet << NetworkMsg::ClientRespawn;
                    clientSocket.append(packet);
                }
            }
        }
    }
}

void NetworkManager::receiveNewConnection() {
    if (const auto newClient = TcpClient(); newClient.acceptFrom(listener)) {
        unverified.emplace_back(newClient, std::chrono::steady_clock::now());
        LOG_INFO_FMT("New TCP connection established with {}", newClient.getRemoteAddress());
    }
}

// 初始化客户端的场景并且将 newClient 加入到 clients vector 中
void NetworkManager::initClientScene(const std::shared_ptr<TcpClient>& newClient) {
    // 给客户端发送当前场景信息
    for (auto it = game_objects.begin(); it != game_objects.end();) {
        if (const auto obj = it->lock()) {
            eng::Packet packet;
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

    eng::Packet packet;
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
        auto& [client, connectedAt] = *it;
        if (eng::Packet verifyPacket; client.receive(verifyPacket) == TcpClient::Status::Done) {
            std::string token;
            verifyPacket >> token;
            if (token == CLIENT_TOKEN) {
                eng::Packet resultPacket;
                constexpr bool success = true;
                std::string message = "Hello brave Mario!";
                resultPacket << success << message;
                client.sendImmediate(resultPacket);
                initClientScene(std::make_shared<TcpClient>(client));
                it = unverified.erase(it); // 已验证，从未验证列表中删除
            } else {
                client.disconnect(); // 直接关闭连接，拒绝其他客户端
                it = unverified.erase(it);
                LOG_WARN_FMT("Wrong client token with '{}'", token);
            }
        } else if (const auto elapsed = std::chrono::steady_clock::now() - connectedAt; elapsed >= VERIFY_TIMEOUT) {
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
    eng::Packet packet;
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

void NetworkManager::serverUpdate(const eng::Time& deltaTime) {
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

        eng::Packet packet;
        TcpClient::Status status = client->receive(packet);
        // 客户端断开连接处理
        if (status == TcpClient::Status::Error || status == TcpClient::Status::Disconnected) {
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
        while (status == TcpClient::Status::Done) {
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
            eng::Packet packet;
            packet << NetworkMsg::RemoveObject << id;
            client->append(packet);
        }
    }

    // 向客户端同步数据（微秒累加并保留余量，消除节拍漂移）
    tick_accum_us += deltaTime.asMicroseconds();
    const auto tick_interval_us = static_cast<std::int64_t>(1000000) / CONFIG.network.tickRate;
    if (tick_accum_us < tick_interval_us) return;
    tick_accum_us -= tick_interval_us;
    LOG_TRACE("Sending update packets to clients");
    for (const auto& client : clients) {
        for (const auto& obj : game_objects) {
            eng::Packet packet;
            obj.lock()->serialize(packet, NetworkMsg::UpdateObject);
            if (packet.getDataSize() == 0) {
                continue;
            }
            client->append(packet);
        }
    }
}

void NetworkManager::clientUpdate(const eng::Time& deltaTime) {
    eng::Packet packet;
    TcpClient::Status status = clientSocket.receive(packet);
    if (status == TcpClient::Status::Error || status == TcpClient::Status::Disconnected) {
        verifyPending = false;   // WEB：验证未完成即断线，清标志防跨连接残留
        connectionLost = true;   // N4：驱动场景断线提示层（一次性，场景重进时 clear）
        network_type = NetworkType::None;
        LOG_WARN("Server disconnected");
        return;
    }
#ifdef __EMSCRIPTEN__
    // WEB 异步验证收口：connect() 乐观返回后首条应答是验证结果（bool+string），
    // 必须先于 NetworkMsg 消息流消费（服务端先 sendImmediate 应答、下一 tick 才
    // 发 Spawn 流，线序可靠）；失败则复位 None，与桌面同步路径失败等价
    if (verifyPending) {
        if (status != TcpClient::Status::Done) return;   // 应答未到，下一帧再收
        verifyPending = false;
        bool success;
        std::string message;
        packet >> success >> message;
        if (!success) {
            LOG_WARN_FMT("Verification failed: {}", message);
            clientSocket.disconnect();
            connectionLost = true;   // N4：被拒同样属于断线类事件，玩家需感知
            network_type = NetworkType::None;
            return;
        }
        LOG_INFO_FMT("Verification succeeded: {}", message);
        LOG_INFO("Connected to server successfully!");
        status = clientSocket.receive(packet);           // 紧随其后的 Spawn 流走通用循环
    }
#endif
    while (status == TcpClient::Status::Done) {
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
            eng::Packet spawn_packet;
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
