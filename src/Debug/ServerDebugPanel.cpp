//
// 服务端 Web 调试面板实现
//
// 客户端构建也会用 GLOB 把这个文件编进来，所以整文件用 SERVER_BUILD 守卫。
//
#ifdef SERVER_BUILD

// ⚠️ httplib 必须是本 TU 第一个 include。
// Logger.h 无条件 include <windows.h>，而 httplib 内部先 include <winsock2.h>；
// 顺序反了会触发 struct timeval / fd_set 重定义，报错信息很难指向根因。
//
// httplib 的 mmap 实现无条件使用 CreateFile2 / CreateFileMappingFromApp，
// 这两个是 Windows 8 才有的 API。MinGW 默认 _WIN32_WINNT 低于 0x0602 时会报
// "'CreateFile2' has not been declared"，所以在这里抬高 API 级别。
// 只在 #ifndef 下定义，避免覆盖构建系统已经传入的值。
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

#define CPPHTTPLIB_THREAD_POOL_COUNT 2   // 调试面板用不到默认的按核数起线程
#include <httplib.h>

#include "ServerDebugPanel.h"

#include "Logger.h"
#include "PanelAuth.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <nlohmann/json.hpp>

namespace {

using namespace std::chrono;

std::int64_t nowMs() {
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// /api/* 统一响应头：面板数据不该被缓存，token 也不该经 Referer 漏出去
void applyApiHeaders(httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("Referrer-Policy", "no-referrer");
}

} // namespace

struct ServerDebugPanel::Impl {
    ConfigManager::DebugPanelConfig cfg;
    std::string token;
    httplib::Server server;
    std::thread thread;

    std::atomic<bool> running{false};
    std::atomic<std::int64_t> last_request_ms{0};   // 供 B4 的 idleSkip 使用
    std::atomic<std::uint64_t> frame_count{0};

    steady_clock::time_point started_at{};

    [[nodiscard]] bool authorized(const httplib::Request& req) const {
        // 主方式走请求头（不进 URL、浏览器历史、Referer）；?token= 只给 curl 兜底
        if (req.has_header("X-Panel-Token")) {
            return panel_auth::tokenEquals(req.get_header_value("X-Panel-Token"), token);
        }
        if (req.has_param("token")) {
            return panel_auth::tokenEquals(req.get_param_value("token"), token);
        }
        return false;
    }
};

ServerDebugPanel::ServerDebugPanel() : impl_(std::make_unique<Impl>()) {}

ServerDebugPanel::~ServerDebugPanel() {
    stop();
}

bool ServerDebugPanel::start(const ConfigManager::DebugPanelConfig& cfg) {
    if (impl_->running.load(std::memory_order_relaxed)) return true;

    impl_->cfg = cfg;
    // 空 token 绝不放行：自动生成并打印到控制台
    impl_->token = cfg.token.empty() ? panel_auth::generateToken() : cfg.token;

    Impl* impl = impl_.get();

    // /api/ 前缀统一鉴权；静态资源不鉴权（页面壳本身不含任何游戏数据）
    impl->server.set_pre_routing_handler(
        [impl](const httplib::Request& req, httplib::Response& res) {
            impl->last_request_ms.store(nowMs(), std::memory_order_relaxed);

            const bool is_api = req.path.rfind("/api/", 0) == 0;
            if (is_api && !impl->authorized(req)) {
                applyApiHeaders(res);
                res.status = 401;
                res.set_content(R"({"error":"unauthorized"})", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

    impl->server.Get("/api/health", [impl](const httplib::Request&, httplib::Response& res) {
        const double uptime = duration<double>(steady_clock::now() - impl->started_at).count();

        nlohmann::json j;
        j["ok"] = true;
        j["panelVersion"] = "1.0.0";
        j["schemaVersion"] = 1;
        j["uptimeSec"] = uptime;
        j["frames"] = impl->frame_count.load(std::memory_order_relaxed);
        j["port"] = impl->cfg.port;
        j["snapshotAgeMs"] = nullptr;   // B3 接入快照后回填

        applyApiHeaders(res);
        res.set_content(j.dump(), "application/json");
    });

#ifdef _WIN32
    // Windows 的 SO_REUSEADDR 语义与 Linux 不同：它允许绑定到「已被别的 socket 监听」
    // 的端口，于是第二个实例能"绑定成功"，但连接仍然路由到第一个实例——端口冲突被
    // 静默吞掉，表现成"面板启动了但数据是旧的"。httplib 默认会设这个选项，
    // 这里显式关掉，让 bind_to_port 在端口被占用时如实返回 false。
    // 代价：重启服务端时若监听 socket 处于 TIME_WAIT，可能短暂绑不上（有明确报错）。
    impl->server.set_socket_options([](auto sock) {
        int no = 0;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&no), sizeof(no));
    });
#endif

    // 先同步 bind：端口被占用时立刻知道，而不是在子线程里静默失败
    if (!impl->server.bind_to_port(cfg.host, cfg.port)) {
        LOG_ERROR_FMT("Debug panel: bind failed on {}:{} (port already in use?)",
                      cfg.host, cfg.port);
        return false;
    }

    impl->started_at = steady_clock::now();
    impl->running.store(true, std::memory_order_relaxed);

    impl->thread = std::thread([impl] {
        impl->server.listen_after_bind();
        impl->running.store(false, std::memory_order_relaxed);
    });

    LOG_INFO_FMT("Debug panel: http://{}:{}/  token={}", cfg.host, cfg.port, impl->token);
    return true;
}

void ServerDebugPanel::stop() {
    if (!impl_) return;

    // 顺序固定：先让 accept 循环退出，再 join；Server 本身是 Impl 的成员，
    // 一定在线程 join 之后才销毁，handler 不会在 Impl 析构后还被调用。
    if (impl_->server.is_running()) {
        impl_->server.stop();
    }
    if (impl_->thread.joinable()) {
        impl_->thread.join();
    }
    impl_->running.store(false, std::memory_order_relaxed);
}

bool ServerDebugPanel::isRunning() const {
    return impl_ && impl_->running.load(std::memory_order_relaxed);
}

void ServerDebugPanel::onFrame(Scene* scene, float dtSec) {
    if (!impl_->running.load(std::memory_order_relaxed)) return;

    // B2 只累计帧数；B3 起在这里限频构建快照并发布
    impl_->frame_count.fetch_add(1, std::memory_order_relaxed);
    (void)scene;
    (void)dtSec;
}

#endif // SERVER_BUILD
