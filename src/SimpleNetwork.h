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

    void serverUpdate(const sf::Time& deltaTime) {
        // 接收新用户连接
        if (const auto newClient = std::make_shared<sf::TcpSocket>(); listener.accept(*newClient) == sf::Socket::Done) {
            newClient->setBlocking(false);
            clients.emplace_back(newClient);
            const auto current_scene = SceneContext::getInstance().getSceneManager()->getCurrentScene();
            const auto newPlayer = std::make_shared<Mario>(100.f, 100.f, false);
            current_scene->addObject(newPlayer);
            players[newClient.get()] = newPlayer;
            std::cout << "New client connected!" << std::endl;
        }

        for (const auto& client : clients) {
            sf::Packet packet;
            while (client->receive(packet) == sf::Socket::Done) {
                float x, y, s_x, s_y;
                bool is_jump;
                packet >> x >> y >> s_x >> s_y >> is_jump;
                players[client.get()]->getComponent<MoveComponent>()->setPosition(x, y);
                players[client.get()]->getComponent<MoveComponent>()->setSpeed(s_x, s_y);
                if (is_jump) players[client.get()]->getComponent<StateMachine>()->setState("MarioJumpState");
            }
        }
    }

    void clientUpdate(const sf::Time& deltaTime) {
        past_time += deltaTime.asMicroseconds();
        if (past_time <= 100) return;
        past_time = 0;
        sf::Packet packet;
        const auto& player = game_objects.front();
        const bool is_jump = player->getComponent<StateMachine>()->getCurrentStateName() == "MarioJumpState";
        packet << player->getPosition().x << player->getPosition().y << player->getSpeed().x << player->getSpeed().y << is_jump;
        clientSocket.send(packet);
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