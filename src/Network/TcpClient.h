//
// Created by MINEC on 2026/5/16.
// SDL_net 迁移 N4：传输层换 SDL_net（NET_StreamSocket），对外契约不变（eng::Packet + Status 四态）。
// 组帧仍为自研 uint32 大端长度前缀，线格式与 SFML 时代逐字节兼容。
//
// 联机上浏览器（websocket-net N2）：__EMSCRIPTEN__ 下传输层换 <emscripten/websocket.h>，
// 经 websockify 桥还原成对端眼中的裸 TCP（docs/websocket-net-plan.md 方向 A′）：
// - 连接异步：浏览器无同步等待点，connect() 发起握手即乐观返回 Done；
//   真实就绪由 WS 事件回调推进 holder 状态位，receive()/tryFlush() 按
//   open/closed/errored 折算既有四态 —— 游戏层契约零变化
// - 收：onmessage 回调只搬字节进 staged 暂存缓冲（回调内禁止发送），
//   receive() 时并入 m_recvBuf 交给拆帧状态机 —— WS 消息边界差异被状态机天然吸收
// - 发：握手完成前 tryFlush() 缓存整帧待发，OPEN 后由每帧 send() 自动冲刷
//   （验证包在 connect 后立刻 sendImmediate 的场景因此无需特殊处理）
// 桌面/服务端路径不进任何 ifdef 分支，行为零变化。
//

#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#endif

#include <SDL3_net/SDL_net.h>    // WEB 下仅 acceptFrom 等签名引用头文件，运行期不触碰 SDL_net

#include "Packet.h"

class TcpClient {
public:
    // 复刻原四态语义（游戏层只认这个）
    enum class Status { Done, NotReady, Disconnected, Error };

    // socket 共享持有：拷贝 TcpClient 共享同一条连接（对齐旧 shared_ptr<sf::TcpSocket> 语义）；
    // disconnect 幂等（所有拷贝经 holder 看到同一状态），析构自动销毁未关的 socket
    struct SocketHolder {
#ifdef __EMSCRIPTEN__
        EMSCRIPTEN_WEBSOCKET_T sock = 0;   // 本版 emsdk 宏定义=int；0=未创建，>0=句柄
        bool connecting = false;           // WS 已创建，握手进行中
        bool open = false;                 // 握手完成可收发（WEB 版 isConnected 的真值）
        bool closed = false;               // onclose 已到（含握手失败后的必然回调）
        bool errored = false;              // onerror 已到
        std::vector<std::uint8_t> staged;  // onmessage 投递的原始字节（事件泵间暂存）

        ~SocketHolder() {
            if (sock) {
                emscripten_websocket_close(sock, 1000, "destroyed");
                emscripten_websocket_delete(sock);
            }
        }
#else
        NET_StreamSocket* sock = nullptr;
        ~SocketHolder() { if (sock) NET_DestroyStreamSocket(sock); }
#endif
    };

    TcpClient() : m_holder(std::make_shared<SocketHolder>()) {}

#ifdef __EMSCRIPTEN__
    [[nodiscard]] bool isConnected() const { return m_holder && m_holder->open; }
#else
    [[nodiscard]] bool isConnected() const { return m_holder && m_holder->sock; }
#endif

    void disconnect() const {
        if (!m_holder || !m_holder->sock) return;
#ifdef __EMSCRIPTEN__
        emscripten_websocket_close(m_holder->sock, 1000, "client quit");
        emscripten_websocket_delete(m_holder->sock);   // 同时注销全部事件回调
        m_holder->sock = 0;
        m_holder->connecting = false;
        m_holder->open = false;
        m_holder->closed = true;
        m_holder->staged.clear();
#else
        NET_DestroyStreamSocket(m_holder->sock);
        m_holder->sock = nullptr;
#endif
    }

    // ── 连接 ───────────────────────────────────────────────
#ifdef __EMSCRIPTEN__
    // timeoutSeconds 无意义（异步握手无同步等待点），保留形参只为契约一致。
    // 非 const：重连前需清 TcpClient 级缓冲（桌面分支不动）
    [[nodiscard]] Status connect(const std::string& address, const unsigned short port,
                                 const float timeoutSeconds = 0) {
        (void)timeoutSeconds;

        // 允许直接传完整 ws:// / wss:// URL（wss 部署用）
        std::string url;
        if (address.rfind("ws://", 0) == 0 || address.rfind("wss://", 0) == 0) {
            url = address;
        } else {
            url = "ws://" + address + ":" + std::to_string(port);
        }

        // 断线重连复位（同一 TcpClient 会随场景缓存复用）：旧句柄不删会泄漏，
        // 且其迟到事件会污染新连接的状态位；旧流的半帧混入新流会造成拆帧错位
        // （长度前缀读到垃圾 → 越界读崩溃）
        if (m_holder->sock) {
            emscripten_websocket_delete(m_holder->sock);   // 关闭旧 WS 并注销其全部回调
            m_holder->sock = 0;
        }
        m_holder->open = false;
        m_holder->closed = false;
        m_holder->errored = false;
        m_holder->staged.clear();
        m_recvBuf.clear();     // 旧连接的半帧绝不流入新流
        m_sendBuf.clear();     // 旧连接的待发帧不重发到新连接
        m_outgoing.clear();

        EmscriptenWebSocketCreateAttributes attrs{};
        attrs.url = url.c_str();
        attrs.protocols = "binary";              // 与 websockify 默认子协议匹配（风险 R6）
        attrs.createOnMainThread = true;

        // 本版 emsdk：emscripten_websocket_new 创建即开始连接（无独立 connect 函数）；
        // >0 = 成功返回句柄，<0 = 错误码，0 = 未知失败
        const EMSCRIPTEN_WEBSOCKET_T sock = emscripten_websocket_new(&attrs);
        if (sock <= 0) return Status::Error;
        m_holder->sock = sock;
        m_holder->connecting = true;

        // new 之后注册回调无竞态：连接事件走 JS 事件循环，本同步块返回前不会触发
        emscripten_websocket_set_onopen_callback(sock, m_holder.get(), wsOnOpen);
        emscripten_websocket_set_onmessage_callback(sock, m_holder.get(), wsOnMessage);
        emscripten_websocket_set_onclose_callback(sock, m_holder.get(), wsOnClose);
        emscripten_websocket_set_onerror_callback(sock, m_holder.get(), wsOnError);

        return Status::Done;                     // 乐观返回，就绪与否见 receive()/isConnected()
    }
#else
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
#endif

    // ── 发送侧 ─────────────────────────────────────────────
    // 帧聚合：一帧内多条消息合并进同一流帧（复刻原 sf::Packet 聚合语义）
    void append(const eng::Packet& packet) {
        if (packet.getDataSize() == 0) return;
        m_outgoing.append(packet.getData(), packet.getDataSize());
    }

    // 帧末统一发送；底层不可写时暂存，下次调用继续冲刷
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
    // 无完整包返回 NotReady，对端关闭返回 Disconnected。
#ifdef __EMSCRIPTEN__
    Status receive(eng::Packet& packet) {
        // 先把回调攒的字节并入拆帧缓冲：一条 WS 消息可能含任意多个完整帧，
        // 也可能只有半帧，全部交由既有状态机消化
        m_recvBuf.insert(m_recvBuf.end(), m_holder->staged.begin(), m_holder->staged.end());
        m_holder->staged.clear();

        // 缓冲里还有完整帧则先交付（关连接前的尾巴数据不丢）
        if (tryExtractPacket(packet) == Status::Done) return Status::Done;

        if (m_holder->errored) return Status::Error;
        if (m_holder->closed) return Status::Disconnected;
        return Status::NotReady;
    }
#else
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
#endif

    // ── 服务端侧：accept / 远端信息 ─────────────────────────
    // 注意：本版 SDL_net 的 NET_AcceptClient 无连接时也返回 true（true=无错误，
    // 已核 sdl_net-src SDL_net.c L1863 "return true; // nothing new."），
    // 必须验出参非空才算真正接到连接
    [[nodiscard]] bool acceptFrom(NET_Server* server) const {
#ifdef __EMSCRIPTEN__
        (void)server;      // WEB 无监听端（startServer 走 Local 分支），仅为可编译保留签名
        return false;
#else
        NET_StreamSocket* s = nullptr;
        NET_AcceptClient(server, &s);
        if (!s) return false;
        m_holder->sock = s;
        return true;
#endif
    }

    [[nodiscard]] std::string getRemoteAddress() const {
#ifdef __EMSCRIPTEN__
        // 浏览器拿不到桥后面的真实对端地址，占位即可（仅日志用途）
        return "?";
#else
        if (!isConnected()) return "?";
        NET_Address* addr = NET_GetStreamSocketAddress(m_holder->sock);
        if (!addr) return "?";
        const char* s = NET_GetAddressString(addr);
        const std::string out = s ? s : "?";
        NET_UnrefAddress(addr);
        return out;
#endif
    }

private:
#ifdef __EMSCRIPTEN__
    // ── WS 事件回调（主线程事件泵触发）：只改状态/搬字节，严禁回调内 send。
    //    本版 emsdk 的回调 typedef 返回 bool（非 EM_BOOL），签名必须逐字对齐
    static bool wsOnOpen(int, const EmscriptenWebSocketOpenEvent*, void* userData) {
        auto* h = static_cast<SocketHolder*>(userData);
        h->connecting = false;
        h->open = true;
        return false;
    }

    static bool wsOnMessage(int, const EmscriptenWebSocketMessageEvent* e, void* userData) {
        auto* h = static_cast<SocketHolder*>(userData);
        if (!e->isText && e->numBytes > 0) {   // 二进制子协议之外的文本帧一律忽略
            h->staged.insert(h->staged.end(), e->data, e->data + e->numBytes);
        }
        return false;
    }

    static bool wsOnClose(int, const EmscriptenWebSocketCloseEvent*, void* userData) {
        auto* h = static_cast<SocketHolder*>(userData);
        h->connecting = false;
        h->open = false;
        h->closed = true;
        return false;
    }

    static bool wsOnError(int, const EmscriptenWebSocketErrorEvent*, void* userData) {
        static_cast<SocketHolder*>(userData)->errored = true;
        return false;
    }
#endif

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

    // 整帧入队；false = 底层不可写或无连接（m_sendBuf 保留待下帧冲刷）
    bool tryFlush() {
        if (m_sendBuf.empty()) return true;
        if (!isConnected()) return false;
#ifdef __EMSCRIPTEN__
        // 引擎只在游戏帧内发送（不在 WS 回调里发送），满足 websocket.h 的调用约束
        if (emscripten_websocket_send_binary(m_holder->sock, m_sendBuf.data(),
                                             static_cast<std::uint32_t>(m_sendBuf.size()))
            != EMSCRIPTEN_RESULT_SUCCESS) return false;
#else
        if (!NET_WriteToStreamSocket(m_holder->sock, m_sendBuf.data(),
                                     static_cast<int>(m_sendBuf.size()))) return false;
#endif
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
    std::vector<std::uint8_t> m_sendBuf;  // 发送缓冲（底层不可写时暂存）
};
