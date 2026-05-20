//
// Created by MINEC on 2026/5/16.
//

#pragma once
#include "SFML/Network.hpp"

class TcpClient {
public:
    TcpClient() : m_socket(std::make_shared<sf::TcpSocket>()) { }

    void disconnect() const {
        m_socket->disconnect();
    }

    void append(const sf::Packet& packet) {
        if (packet.getDataSize() == 0) return;
        m_packet.append(packet.getData(), packet.getDataSize());
    }

    sf::Socket::Status receive(sf::Packet& packet) const {
        return m_socket->receive(packet);
    }

    [[nodiscard]] sf::TcpSocket& getSocket() const {
        return *m_socket;
    }

    void setBlocking(const bool blocking) const {
        m_socket->setBlocking(blocking);
    }

    [[nodiscard]] sf::Socket::Status connect(const sf::IpAddress& remoteAddress, const unsigned short remotePort, const sf::Time timeout = sf::Time::Zero) const {
        return m_socket->connect(remoteAddress, remotePort, timeout);
    }

    void send() {
        if (m_packet.getDataSize() == 0) return;
        m_socket->send(m_packet);
        m_packet.clear();
    }

private:
    std::shared_ptr<sf::TcpSocket> m_socket;
    sf::Packet m_packet;
};
