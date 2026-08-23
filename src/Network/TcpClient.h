//
// Created by MINEC on 2026/5/16.
// SDL_net 迁移 N4：传输层换 SDL_net（NET_StreamSocket），对外契约不变（eng::Packet + Status 四态）。
// 组帧仍为自研 uint32 大端长度前缀，线格式与 SFML 时代逐字节兼容。
//
// SDL_net 3 语义要点（已核本地 sdl_net-src/include/SDL3_net/SDL_net.h）：
// - 全异步模型：NET_WriteToStreamSocket 入内部队列（false=队列满，本帧未入队），
//   内部线程自动冲刷；NET_ReadFromStreamSocket >0 字节 / 0 无数据 / -1 断开或报废
// - 无阻塞概念：setBlocking 已随 SFML 移除
// - NET_Address 解析异步：NET_ResolveHostname → NET_WaitUntilResolved(timeoutMS)
//

#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL3_net/SDL_net.h>

#include "Packet.h"

class TcpClient {
public:
    // 复刻原四态语义（游戏层只认这个）
    enum class Status { Done, NotReady, Disconnected, Error };

    // socket 共享持有：拷贝 TcpClient 共享同一条连接（对齐旧 shared_ptr<sf::TcpSocket> 语义）；
    // disconnect 幂等（所有拷贝经 holder 看到同一状态），析构自动销毁未关的 socket
    struct SocketHolder {
        NET_StreamSocket* sock = nullptr;
        ~SocketHolder() { if (sock) NET_DestroyStreamSocket(sock); }
    };

    TcpClient() : m_holder(std::make_shared<SocketHolder>()) {}

    [[nodiscard]] bool isConnected() const { return m_holder && m_holder->sock; }

    void disconnect() const {
        if (m_holder && m_holder->sock) {
            NET_DestroyStreamSocket(m_holder->sock);
            m_holder->sock = nullptr;
        }
    }

    // timeoutSeconds：DNS 解析 + TCP 连接各自的上限；0 = 无限等（对齐旧 connect 语义）
    [[nodiscard]] Status connect(const std::string& address, const unsigned short port,
                                 const float timeoutSeconds = 0) const {
        const int timeoutMs = timeoutSeconds > 0 ? static_cast<int>(timeoutSeconds * 1000) : -1;

        NET_Address* addr = NET_ResolveHostname(address.c_str());
        if (!addr) return Status::Error;
        const bool resolved = NET_WaitUntilResolved(addr, timeoutMs) == NET_SUCCESS;
        NET_StreamSocket* sock = resolved ? NET_CreateClient(addr, port, 0) : nullptr;
        NET_UnrefAddress(addr);              // CreateClient 成功时持有自己的引用
        if (!sock) return Status::Error;

        const bool connected = NET_WaitUntilConnected(sock, timeoutMs) == NET_SUCCESS;
        if (!connected) {
            NET_DestroyStreamSocket(sock);
            return Status::Error;
        }
        m_holder->sock = sock;
        return Status::Done;
    }

    // ── 发送侧 ─────────────────────────────────────────────
    // 帧聚合：一帧内多条消息合并进同一流帧（复刻原 sf::Packet 聚合语义）
    void append(const eng::Packet& packet) {
        if (packet.getDataSize() == 0) return;
        m_outgoing.append(packet.getData(), packet.getDataSize());
    }

    // 帧末统一发送；SDL_net 内部队列满时暂存，下次调用继续冲刷
    Status send() {
        if (!tryFlush()) return Status::NotReady;
        if (m_outgoing.getDataSize() == 0) return Status::Done;
        frame(m_outgoing);
        m_outgoing.clear();
        return tryFlush() ? Status::Done : Status::NotReady;
    }

    // 立即单发（验证握手用：绕过帧聚合直接入队，内部线程异步冲刷）
    Status sendImmediate(const eng::Packet& packet) {
        if (!tryFlush()) return Status::NotReady;
        if (packet.getDataSize() == 0) return Status::Done;
        frame(packet);
        return tryFlush() ? Status::Done : Status::NotReady;
    }

    // ── 接收侧 ─────────────────────────────────────────────
    // 裸字节 → 长度前缀拆帧 → 完整 packet：
    // 无完整包返回 NotReady，对端关闭返回 Disconnected（SDL_net：-1 = 不可恢复）。
    Status receive(eng::Packet& packet) {
        for (;;) {
            if (tryExtractPacket(packet) == Status::Done) return Status::Done;
            std::uint8_t buf[4096];
            const int n = isConnected() ? NET_ReadFromStreamSocket(m_holder->sock, buf, sizeof buf) : -1;
            if (n > 0) {
                m_recvBuf.insert(m_recvBuf.end(), buf, buf + n);
                continue;                      // 可能凑出完整帧，回去再拆
            }
            if (n == 0) return Status::NotReady;
            return Status::Disconnected;       // -1：对端断开/连接报废
        }
    }

    // ── 服务端侧：accept / 远端信息 ─────────────────────────
    [[nodiscard]] bool acceptFrom(NET_Server* server) const {
        NET_StreamSocket* s = nullptr;
        if (!NET_AcceptClient(server, &s)) return false;   // 无待接入连接
        m_holder->sock = s;
        return true;
    }

    [[nodiscard]] std::string getRemoteAddress() const {
        if (!isConnected()) return "?";
        NET_Address* addr = NET_GetStreamSocketAddress(m_holder->sock);
        if (!addr) return "?";
        const char* s = NET_GetAddressString(addr);
        const std::string out = s ? s : "?";
        NET_UnrefAddress(addr);
        return out;
    }

private:
    // 打流帧：uint32 大端长度前缀 + payload（逐字节对齐 sf::Packet 流格式）
    void frame(const eng::Packet& packet) {
        const auto size = static_cast<std::uint32_t>(packet.getDataSize());
        m_sendBuf.assign(4 + size, 0);
        m_sendBuf[0] = static_cast<std::uint8_t>(size >> 24);
        m_sendBuf[1] = static_cast<std::uint8_t>(size >> 16);
        m_sendBuf[2] = static_cast<std::uint8_t>(size >> 8);
        m_sendBuf[3] = static_cast<std::uint8_t>(size);
        if (size > 0) std::memcpy(m_sendBuf.data() + 4, packet.getData(), size);
    }

    // 整帧入队；false = 队列满或无连接（m_sendBuf 保留待下帧冲刷）
    bool tryFlush() {
        if (m_sendBuf.empty()) return true;
        if (!isConnected()) return false;
        if (!NET_WriteToStreamSocket(m_holder->sock, m_sendBuf.data(),
                                     static_cast<int>(m_sendBuf.size()))) return false;
        m_sendBuf.clear();
        return true;
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

    std::shared_ptr<SocketHolder> m_holder;
    eng::Packet m_outgoing;               // 帧聚合缓冲
    std::vector<std::uint8_t> m_recvBuf;  // 接收缓冲（拆帧状态机）
    std::vector<std::uint8_t> m_sendBuf;  // 发送缓冲（SDL_net 队列满时暂存）
};
