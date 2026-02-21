//
// Created by MINEC on 2026/2/20.
//

#pragma once
#include <SFML/Network.hpp>
#include <iostream>
#include "Mario.h"

class SimpleNetwork {
public:
    enum class NetworkType {
        None,
        Server,
        Client
    };
    SimpleNetwork() = default;
    ~SimpleNetwork() = default;

    bool startServer() {
        std::cout << "Starting server on port " << port << "..." << std::endl;

        if (listener.listen(port) != sf::Socket::Done) {
            std::cout << "Failed to start server" << std::endl;
            return false;
        }

        listener.setBlocking(false);
        network_type = NetworkType::Server;
        std::cout << "Server started successfully!" << std::endl;
        return true;
    }

    bool connectToServer(const std::string& address) {
        std::cout << "Connecting to " << address << ":" << port << "..." << std::endl;

        if (clientSocket.connect(address, port, sf::seconds(5)) != sf::Socket::Done) {
            std::cout << "Failed to connect to server!" << std::endl;
            return false;
        }

        clientSocket.setBlocking(false);
        network_type = NetworkType::Client;
        std::cout << "Connected to server successfully!" << std::endl;
        return true;
    }

    void update(const sf::Time& deltaTime) {
        if (network_type == NetworkType::None) return;
        if (network_type == NetworkType::Server) {
            serverUpdate(deltaTime);
        } else {
            clientUpdate(deltaTime);
        }
    }

    void handleEvent(const sf::Event& event) {
        if (network_type != NetworkType::Client) return;
        sf::Packet packet;
        if (event.type == sf::Event::KeyPressed) {
            if (event.key.code == sf::Keyboard::W) {
                packet << 0;
                clientSocket.send(packet);
            } else if (event.key.code == sf::Keyboard::A) {
                packet << 1;
                clientSocket.send(packet);
            } else if (event.key.code == sf::Keyboard::D) {
                packet << 2;
                clientSocket.send(packet);
            }
        } else if (event.type == sf::Event::KeyReleased) {
            if (event.key.code == sf::Keyboard::A) {
                packet << 3;
                clientSocket.send(packet);
            } else if (event.key.code == sf::Keyboard::D) {
                packet << 4;
                clientSocket.send(packet);
            }
        }
    }

    void serverUpdate(const sf::Time& deltaTime) {
        // 接收新用户连接
        if (const auto newClient = std::make_shared<sf::TcpSocket>(); listener.accept(*newClient) == sf::Socket::Done) {
            newClient->setBlocking(false);

            // 给客户端发送当前场景信息
            for (const auto& obj : game_objects) {
                sf::Packet packet;
                packet << 0;
                if (obj->getClassName() == "Mario") {
                    const bool is_jump = obj->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
                    packet << 1 << obj->getPosition().x << obj->getPosition().y << obj->getSpeed().x << obj->getSpeed().y << is_jump;
                } else if (obj->getClassName() == "Player") {
                    packet << 2 << obj->getPosition().x << obj->getPosition().y << obj->getSpeed().x << obj->getSpeed().y << obj->getSize().x / 2;
                } else if (obj->getClassName() == "BoxGameObject") {
                    packet << 3 << obj->getPosition().x << obj->getPosition().y << obj->getSpeed().x << obj->getSpeed().y << obj->getSize().x << obj->getSize().y;
                }
                packet << obj->getId();
                newClient->send(packet);
            }
            // 创建新加入的玩家
            const auto current_scene = SceneContext::getInstance().getSceneManager()->getCurrentScene();
            const auto newPlayer = std::make_shared<Mario>(100.f, 100.f, false);
            current_scene->addObjectWithNetwork(newPlayer);
            players[newClient.get()] = newPlayer;

            // 发送新玩家信息给所有玩家
            sf::Packet packet;
            packet << 0;
            const bool is_jump = newPlayer->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
            packet << 1 << newPlayer->getPosition().x << newPlayer->getPosition().y << newPlayer->getSpeed().x << newPlayer->getSpeed().y << is_jump;
            packet << newPlayer->getId();

            for (const auto& client : clients) {
                client->send(packet);
            }

            // 发送新玩家信息给新玩家自己
            packet.clear();
            packet << 0;
            packet << 0 << newPlayer->getPosition().x << newPlayer->getPosition().y << newPlayer->getSpeed().x << newPlayer->getSpeed().y << is_jump;
            packet << newPlayer->getId();
            newClient->send(packet);

            clients.emplace_back(newClient);

            std::cout << "New client connected!" << std::endl;
        }

        // 处理客户端数据
        for (const auto& client : clients) {
            sf::Packet packet;
            while (client->receive(packet) == sf::Socket::Done) {
                const auto& marioController = players[client.get()]->getComponent<MarioController>();
                int type;
                packet >> type;
                if (type == 0) {
                    marioController->jump();
                    players[client.get()]->getComponent<StateMachine>()->setState("MarioJumpState");
                } else if (type == 1) {
                    marioController->runLeft();
                } else if (type == 2) {
                    marioController->runRight();
                } else if (type == 3 || type == 4) {
                    marioController->stopRun();
                }
            }
        }

        // 向客户端同步数据
        for (const auto& client : clients) {
            for (const auto& obj : game_objects) {
                sf::Packet packet;
                packet << 1;
                if (obj->getClassName() == "Mario") {
                    const bool is_jump = obj->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
                    packet << 0 << obj->getPosition().x << obj->getPosition().y << obj->getSpeed().x << obj->getSpeed().y << is_jump;
                } else {
                    packet << 1 << obj->getPosition().x << obj->getPosition().y << obj->getSpeed().x << obj->getSpeed().y;
                }
                packet << obj->getId();
                client->send(packet);
            }
        }
    }

    void clientUpdate(const sf::Time& deltaTime) {
        sf::Packet packet;
        while (clientSocket.receive(packet) == sf::Socket::Done) {
            int type;
            packet >> type;
            if (type == 0) {
                int obj_type;
                packet >> obj_type;
                if (obj_type == 0) {
                    float x, y, s_x, s_y;
                    bool is_jump;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> is_jump >> id;
                    std::shared_ptr<Mario> player = std::make_shared<Mario>(x, y);
                    player->setId(id);
                    const auto& move_component = player->getComponent<MoveComponent>();
                    move_component->setSpeed(s_x, s_y);
                    SceneContext::getInstance().getSceneManager()->getCurrentScene()->addObjectWithMap(player);
                } else if (obj_type == 1) {
                    float x, y, s_x, s_y;
                    bool is_jump;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> is_jump >> id;
                    std::shared_ptr<Mario> player = std::make_shared<Mario>(x, y, false);
                    player->setId(id);
                    const auto& move_component = player->getComponent<MoveComponent>();
                    move_component->setSpeed(s_x, s_y);
                    SceneContext::getInstance().getSceneManager()->getCurrentScene()->addObjectWithMap(player);
                } else if (obj_type == 2) {
                    float x, y, s_x, s_y, radius;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> radius >> id;
                    std::shared_ptr<Player> player = std::make_shared<Player>(x, y, radius);
                    player->setId(id);
                    const auto& move_component = player->getComponent<MoveComponent>();
                    move_component->setSpeed(s_x, s_y);
                    SceneContext::getInstance().getSceneManager()->getCurrentScene()->addObjectWithMap(player);
                } else if (obj_type == 3) {
                    float x, y, s_x, s_y, w, h;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> w >> h >> id;
                    std::shared_ptr<BoxGameObject> box = std::make_shared<BoxGameObject>(x, y, w, h);
                    box->setId(id);

                    const auto& move_component = box->addComponent<MoveComponent>();
                    move_component->setSpeed(s_x, s_y);
                    SceneContext::getInstance().getSceneManager()->getCurrentScene()->addObjectWithMap(box);
                }
            } else if (type == 1) {
                int obj_type;
                packet >> obj_type;
                if (obj_type == 0) {
                    float x, y, s_x, s_y;
                    bool is_jump;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> is_jump >> id;
                    const std::shared_ptr<GameObject>& player = SceneContext::getInstance().getSceneManager()->getCurrentScene()->findGameObjectById(id);
                    const auto& move_component = player->getComponent<MoveComponent>();
                    move_component->setPosition(x, y);
                    move_component->setSpeed(s_x, s_y);
                    if (is_jump) player->getComponent<StateMachine>()->setState("MarioJumpState");
                } else if (obj_type == 1) {
                    float x, y, s_x, s_y;
                    unsigned int id;
                    packet >> x >> y >> s_x >> s_y >> id;
                    const std::shared_ptr<GameObject>& player = SceneContext::getInstance().getSceneManager()->getCurrentScene()->findGameObjectById(id);
                    const auto& move_component = player->getComponent<MoveComponent>();
                    move_component->setPosition(x, y);
                    move_component->setSpeed(s_x, s_y);
                }
            }
        }
    }

    void addGameObject(const std::shared_ptr<GameObject>& obj) {
        game_objects.emplace_back(obj);
    }

private:
    NetworkType network_type = NetworkType::None;
    unsigned int port = 8888;
    sf::TcpSocket clientSocket;
    sf::TcpListener listener;
    std::vector<std::shared_ptr<sf::TcpSocket>> clients;
    std::unordered_map<sf::TcpSocket*, std::shared_ptr<GameObject>> players;
    std::vector<std::shared_ptr<GameObject>> game_objects;
    long long past_time = 0;
};