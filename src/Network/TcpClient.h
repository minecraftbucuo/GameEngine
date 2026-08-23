//
// Created by MINEC on 2026/5/16.
// SDL_net 迁移 N3：对外契约切 eng::Packet，传输层暂仍 sf::TcpSocket 裸字节。
// 组帧自研：uint32 大端长度前缀 + payload，逐字节对齐 sf::Packet 流格式
// （已核 SFML 2.6.1 TcpSocket.cpp L267 htonl / L321 ntohl 源码），
// 保证迁移期新旧 exe 互通。N4 将内部传输换 SDL_net，本文件对外接口不再变。
//

#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SFML/Network.hpp>   // N3 过渡：仅传输层使用，N4 整体移除

#include "Packet.h"

class TcpClient {
public:
    // 复刻 sf::Socket::Status 四态（游戏层从此只认这个）
    enum class Status { Done, NotReady, Disconnected, Error };

    TcpClient() : m_socket(std::make_shared<sf::TcpSocket>()) {}

    void disconnect() const {
        m_socket->disconnect();
    }

    void setBlocking(const bool blocking) const {
        m_socket->setBlocking(blocking);   // N4 删除：SDL_net 全异步无此概念
    }

    [[nodiscard]] Status connect(const std::string& address, const unsigned short port,
                                 const float timeoutSeconds = 0) const {
        return map(m_socket->connect(sf::IpAddress(address), port, sf::seconds(timeoutSeconds)));
    }

    // ── 发送侧 ─────────────────────────────────────────────
    // 帧聚合：一帧内多条消息合并进同一流帧（复刻原 sf::Packet 聚合语义）
    void append(const eng::Packet& packet) {
        if (packet.getDataSize() == 0) return;
        m_outgoing.append(packet.getData(), packet.getDataSize());
    }

    // 帧末统一发送；内核缓冲满时暂存剩余，下次调用继续冲刷
    Status send() {
        if (const Status s = flushPending(); s != Status::Done) return s;
        if (m_outgoing.getDataSize() == 0) return Status::Done;
        frame(m_outgoing);
        m_outgoing.clear();
        return flushPending();
    }

    // 立即单发（验证握手用：绕过帧聚合，阻塞 socket 下同步发完）
    Status sendImmediate(const eng::Packet& packet) {
        if (const Status s = flushPending(); s != Status::Done) return s;
        if (packet.getDataSize() == 0) return Status::Done;
        frame(packet);
        return flushPending();
    }

    // ── 接收侧 ─────────────────────────────────────────────
    // 裸字节 → 长度前缀拆帧 → 完整 packet；与 sf::TcpSocket::receive(Packet&) 语义一致：
    // 无完整包返回 NotReady，对端关闭返回 Disconnected。
    Status receive(eng::Packet& packet) {
        for (;;) {
            if (const Status s = tryExtractPacket(packet); s == Status::Done) return s;
            std::uint8_t buf[4096];
            std::size_t got = 0;
            switch (m_socket->receive(buf, sizeof buf, got)) {
                case sf::Socket::Done:
                    m_recvBuf.insert(m_recvBuf.end(), buf, buf + got);
                    break;                      // 继续尝试拆帧
                case sf::Socket::NotReady:
                    return Status::NotReady;
                case sf::Socket::Disconnected:
                    return Status::Disconnected;
                default:
                    return Status::Error;
            }
        }
    }

    // ── N3 过渡：listener accept / 远端信息（getSocket 不再暴露）──
    bool acceptFrom(sf::TcpListener& listener) const {
        return listener.accept(*m_socket) == sf::Socket::Done;
    }

    [[nodiscard]] std::string getRemoteAddress() const {
        return m_socket->getRemoteAddress().toString();
    }

    [[nodiscard]] unsigned short getRemotePort() const {
        return m_socket->getRemotePort();
    }

private:
    [[nodiscard]] static Status map(const sf::Socket::Status s) {
        switch (s) {
            case sf::Socket::Done:        return Status::Done;
            case sf::Socket::NotReady:    return Status::NotReady;
            case sf::Socket::Disconnected: return Status::Disconnected;
            default:                      return Status::Error;   // Partial 归入 Error（连接阶段不出现）
        }
    }

    // 打流帧：uint32 大端长度前缀 + payload（逐字节对齐 sf::Packet 流格式）
    void frame(const eng::Packet& packet) {
        const auto size = static_cast<std::uint32_t>(packet.getDataSize());
        m_sendBuf.assign(4 + size, 0);
        m_sendBuf[0] = static_cast<std::uint8_t>(size >> 24);
        m_sendBuf[1] = static_cast<std::uint8_t>(size >> 16);
        m_sendBuf[2] = static_cast<std::uint8_t>(size >> 8);
        m_sendBuf[3] = static_cast<std::uint8_t>(size);
        if (size > 0) std::memcpy(m_sendBuf.data() + 4, packet.getData(), size);
        m_sendPos = 0;
    }

    // 冲刷发送缓冲；Done=全部发出，NotReady=内核缓冲满（暂存待下帧）
    Status flushPending() {
        while (m_sendPos < m_sendBuf.size()) {
            std::size_t sent = 0;
            switch (m_socket->send(m_sendBuf.data() + m_sendPos, m_sendBuf.size() - m_sendPos, sent)) {
                case sf::Socket::Done:
                    m_sendPos = m_sendBuf.size();
                    break;
                case sf::Socket::Partial:
                    m_sendPos += sent;
                    break;
                case sf::Socket::NotReady:
                    return Status::NotReady;
                case sf::Socket::Disconnected:
                    return Status::Disconnected;
                default:
                    return Status::Error;
            }
        }
        m_sendBuf.clear();
        m_sendPos = 0;
        return Status::Done;
    }

    // 缓冲够一整帧则弹出一个完整 packet
    Status tryExtractPacket(eng::Packet& packet) {
        if (m_recvBuf.size() < 4) return Status::NotReady;
        const auto len = static_cast<std::uint32_t>(
            (static_cast<std::uint32_t>(m_recvBuf[0]) << 24)
          | (static_cast<std::uint32_t>(m_recvBuf[1]) << 16)
          | (static_cast<std::uint32_t>(m_recvBuf[2]) << 8)
          | static_cast<std::uint32_t>(m_recvBuf[3]));
        if (m_recvBuf.size() < 4 + len) return Status::NotReady;

        packet.clear();
        packet.append(m_recvBuf.data() + 4, len);
        m_recvBuf.erase(m_recvBuf.begin(), m_recvBuf.begin() + 4 + static_cast<std::ptrdiff_t>(len));
        return Status::Done;
    }

    std::shared_ptr<sf::TcpSocket> m_socket;
    eng::Packet m_outgoing;               // 帧聚合缓冲
    std::vector<std::uint8_t> m_recvBuf;  // 接收缓冲（拆帧状态机）
    std::vector<std::uint8_t> m_sendBuf;  // 发送缓冲（含内核满时的暂存）
    std::size_t m_sendPos = 0;
};
