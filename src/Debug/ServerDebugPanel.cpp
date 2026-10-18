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

#include "DebugSnapshot.h"
#include "Logger.h"
#include "PanelAuth.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
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

// 前端目录解析顺序，取第一个存在的：
//   1. config 里显式配置的 webRoot
//   2. ../../web/debug —— 相对 exe 目录，命中源码树（开发场景，改完刷新即生效）
//   3. ./web/debug     —— 相对 exe 目录，命中 CMake 拷贝的部署副本
// 基准是 exe 目录：GameEngine::init 已把 cwd 设过去，面板在其之后启动。
std::string resolveWebRoot(const std::string& configured) {
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!configured.empty()) {
        if (fs::is_directory(configured, ec)) {
            return fs::weakly_canonical(configured, ec).string();
        }
        LOG_WARN_FMT("Debug panel: configured webRoot not found: {}", configured);
    }

    const std::array<const char*, 2> candidates = {"../../web/debug", "./web/debug"};
    for (const char* candidate : candidates) {
        if (fs::is_directory(candidate, ec)) {
            return fs::weakly_canonical(candidate, ec).string();
        }
    }
    return {};
}

constexpr const char* PLACEHOLDER_HTML = R"HTML(<!doctype html>
<html lang="zh-CN"><meta charset="utf-8">
<title>调试面板 · 前端未找到</title>
<style>
body{background:#0d1117;color:#e6edf3;font-family:system-ui,-apple-system,sans-serif;
     padding:40px;line-height:1.7;max-width:720px}
h2{margin:0 0 16px;font-size:18px;font-weight:500}
code{background:#161b22;border:1px solid #30363d;padding:2px 6px;border-radius:4px;
     font-family:ui-monospace,Consolas,monospace;font-size:13px}
p{color:#8b949e;margin:12px 0}
</style>
<h2>调试面板前端目录未找到</h2>
<p>HTTP 服务已经起来了，但没找到前端文件。</p>
<p>请确认以下任一目录存在：</p>
<p><code>./web/debug</code>（exe 旁，CMake 构建时拷贝）<br>
<code>../../web/debug</code>（源码树，开发时使用）</p>
<p>或在 <code>Asset/config.json</code> 的 <code>debugPanel.webRoot</code> 里显式指定路径。</p>
<p>服务端日志里有实际尝试的路径。</p>
</html>)HTML";

} // namespace

struct ServerDebugPanel::Impl {
    ConfigManager::DebugPanelConfig cfg;
    std::string token;
    httplib::Server server;
    std::thread thread;

    std::atomic<bool> running{false};
    std::atomic<std::int64_t> last_request_ms{0};   // 空闲跳采的判据
    std::atomic<std::uint64_t> frame_count{0};

    // 有请求进来时置位，让主线程下一帧立刻重建快照（否则最长要等一个采样周期）
    std::atomic<bool> force_build{false};
    std::atomic<std::uint64_t> idle_skips{0};       // 因空闲跳过的构建次数

    // ── 快照（主线程写，HTTP 线程读）──────────────────────────────
    // 锁只保护一次 shared_ptr 交换；旧快照在最后一个读者释放后自动回收，
    // 等价于双缓冲，不需要手写两个槽位。
    std::mutex snapshot_mutex;
    std::shared_ptr<const std::string> snapshot;
    std::atomic<std::int64_t> snapshot_built_ms{0};
    std::atomic<std::uint64_t> build_cost_us{0};
    std::atomic<std::uint64_t> seq{0};

    // 仅主线程访问
    steady_clock::time_point last_build{};
    milliseconds build_interval{100};
    float avg_frame_ms = 0.f;
    int slow_builds = 0;            // 连续"构建过慢"次数，用于自动降级

    // 实际生效的采样频率：配置值可能被自动降级改写，所以单独记一份
    std::atomic<int> effective_hz{10};

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

    // 是否该跳过本帧的快照构建。调用方已完成限频，这里只管"没人看就别算"。
    // 返回 true 时不能顺手消费 force_build —— 只在该真正跳过的路径上消费。
    [[nodiscard]] bool shouldSkipIdle() {
        if (cfg.idleSkipSeconds <= 0) return false;

        {
            std::lock_guard<std::mutex> lock(snapshot_mutex);
            if (!snapshot) return false;   // 一份都还没有：先产一份，否则首次访问会 503
        }

        const std::int64_t last_req = last_request_ms.load(std::memory_order_relaxed);
        const bool idle = last_req == 0 ||
                          nowMs() - last_req > static_cast<std::int64_t>(cfg.idleSkipSeconds) * 1000;
        if (!idle) return false;

        // 空闲期间来了请求：这一帧必须重建，把旧数据换成新的
        if (force_build.exchange(false, std::memory_order_relaxed)) return false;

        return true;
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

    const int hz = std::clamp(cfg.snapshotHz, 1, 60);
    impl_->effective_hz.store(hz, std::memory_order_relaxed);
    impl_->build_interval = milliseconds(1000 / hz);

    Impl* impl = impl_.get();

    // /api/ 前缀统一鉴权；静态资源不鉴权（页面壳本身不含任何游戏数据）
    impl->server.set_pre_routing_handler(
        [impl](const httplib::Request& req, httplib::Response& res) {
            impl->last_request_ms.store(nowMs(), std::memory_order_relaxed);
            // 空闲跳采期间有人来访问，通知主线程立刻重建，别让人盯着旧数据
            impl->force_build.store(true, std::memory_order_relaxed);

            const bool is_api = req.path.rfind("/api/", 0) == 0;
            if (is_api && !impl->authorized(req)) {
                applyApiHeaders(res);
                res.status = 401;
                res.set_content(R"({"error":"unauthorized"})", "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });

    impl->server.Get("/api/health", [impl](const httplib::Request& req, httplib::Response& res) {
        const double uptime = duration<double>(steady_clock::now() - impl->started_at).count();

        nlohmann::json j;
        j["ok"] = true;
        j["panelVersion"] = "1.0.0";
        j["schemaVersion"] = 1;
        j["uptimeSec"] = uptime;
        j["frames"] = impl->frame_count.load(std::memory_order_relaxed);
        j["port"] = impl->cfg.port;

        // 配置值与实际生效值分开报：自动降级后两者会不一致
        j["snapshotHz"] = impl->cfg.snapshotHz;
        j["effectiveHz"] = impl->effective_hz.load(std::memory_order_relaxed);
        j["idleSkips"] = impl->idle_skips.load(std::memory_order_relaxed);
        j["idleSkipSeconds"] = impl->cfg.idleSkipSeconds;
        j["buildCostUs"] = impl->build_cost_us.load(std::memory_order_relaxed);

        const auto built = impl->snapshot_built_ms.load(std::memory_order_relaxed);
        if (built == 0) {
            j["snapshotAgeMs"] = nullptr;   // 第一份快照还没产出
        } else {
            j["snapshotAgeMs"] = nowMs() - built;
        }

        applyApiHeaders(res);
        res.set_content(req.has_param("pretty") ? j.dump(2) : j.dump(), "application/json");
    });

    impl->server.Get("/api/snapshot", [impl](const httplib::Request& req, httplib::Response& res) {
        // 锁窗口只有一次 shared_ptr 拷贝，之后在锁外读取内容
        std::shared_ptr<const std::string> snap;
        {
            std::lock_guard<std::mutex> lock(impl->snapshot_mutex);
            snap = impl->snapshot;
        }

        applyApiHeaders(res);
        if (!snap) {
            res.status = 503;
            res.set_content(R"({"error":"snapshot_not_ready"})", "application/json");
            return;
        }

        // 快照本体是压缩过的单行 JSON。?pretty=1 只给人眼看，重新解析一遍的
        // 开销（38KB 量级）远小于一次构建，不值得为它缓存两份。
        if (req.has_param("pretty")) {
            try {
                res.set_content(nlohmann::json::parse(*snap).dump(2), "application/json");
            } catch (const nlohmann::json::exception&) {
                res.set_content(*snap, "application/json");   // 理论上不可能，降级直出
            }
            return;
        }
        res.set_content(*snap, "application/json");
    });

    // 静态资源也禁止缓存：开发时改 HTML/JS 刷新即生效，不用清缓存
    impl->server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Cache-Control", "no-store");
    });

    // ── 静态前端 ──────────────────────────────────────────────────
    const std::string web_root = resolveWebRoot(cfg.webRoot);
    if (!web_root.empty()) {
        if (!impl->server.set_mount_point("/", web_root)) {
            LOG_WARN_FMT("Debug panel: set_mount_point failed for {}", web_root);
        }
        LOG_INFO_FMT("Debug panel web root: {}", web_root);
    } else {
        LOG_WARN("Debug panel: web root not found (web/debug), serving placeholder page");
    }

    impl->server.Get("/", [web_root](const httplib::Request&, httplib::Response& res) {
        if (!web_root.empty()) {
            std::ifstream in(web_root + "/index.html", std::ios::binary);
            if (in) {
                std::ostringstream ss;
                ss << in.rdbuf();
                res.set_content(ss.str(), "text/html; charset=utf-8");
                return;
            }
        }
        res.status = 503;
        res.set_content(PLACEHOLDER_HTML, "text/html; charset=utf-8");
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
    LOG_INFO_FMT("Debug panel: {}Hz, maxObjects={}, idleSkip={}s{}",
                 impl->effective_hz.load(std::memory_order_relaxed), cfg.maxObjects,
                 cfg.idleSkipSeconds,
                 cfg.idleSkipSeconds > 0 ? " (skip building while nobody watches)" : "");
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

    impl_->frame_count.fetch_add(1, std::memory_order_relaxed);

    // 指数平均：面板开启前后的 avgFrameMs 差值就是它自己的开销
    const float dtMs = dtSec * 1000.f;
    impl_->avg_frame_ms = impl_->avg_frame_ms * 0.95f + dtMs * 0.05f;

    // 限频：主循环 165Hz，快照 10Hz 就够人看了
    const auto now = steady_clock::now();
    if (now - impl_->last_build < impl_->build_interval) return;
    impl_->last_build = now;

    if (scene == nullptr) return;

    // 没人看就不算：省掉的是主线程上遍历全部对象 + 序列化的开销
    if (impl_->shouldSkipIdle()) {
        impl_->idle_skips.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    DebugSnapshotContext ctx;
    ctx.seq = impl_->seq.fetch_add(1, std::memory_order_relaxed) + 1;
    ctx.frame = impl_->frame_count.load(std::memory_order_relaxed);
    ctx.uptimeSec = duration<double>(now - impl_->started_at).count();
    ctx.dtMs = dtMs;
    ctx.avgFrameMs = impl_->avg_frame_ms;
    ctx.lastBuildCostUs = impl_->build_cost_us.load(std::memory_order_relaxed);
    ctx.snapshotHz = impl_->effective_hz.load(std::memory_order_relaxed);
    ctx.maxObjects = impl_->cfg.maxObjects;

    std::uint64_t cost = 0;
    std::string json = buildDebugSnapshot(scene, ctx, &cost);

    {
        std::lock_guard<std::mutex> lock(impl_->snapshot_mutex);
        impl_->snapshot = std::make_shared<const std::string>(std::move(json));
    }
    impl_->build_cost_us.store(cost, std::memory_order_relaxed);
    impl_->snapshot_built_ms.store(nowMs(), std::memory_order_relaxed);

    // ── 开销过高时自动降采样 ──────────────────────────────────────
    // 判据是「构建耗时超过半个帧预算」，而不是绝对毫秒数：机器慢、帧长的时候
    // 同样 3ms 并不算贵。连续 3 次才动手，避免被偶发的一次抖动带偏。
    // 只降不升：升回去会在临界点来回抖，收益不抵复杂度。
    constexpr int kMinAutoHz = 2;
    const double costMs = static_cast<double>(cost) / 1000.0;
    const double frameMs = impl_->avg_frame_ms > 0.01f ? impl_->avg_frame_ms : 16.0;

    if (costMs > frameMs * 0.5) {
        if (++impl_->slow_builds >= 3) {
            const int current = impl_->effective_hz.load(std::memory_order_relaxed);
            if (current > kMinAutoHz) {
                const int next = std::max(kMinAutoHz, current / 2);
                impl_->effective_hz.store(next, std::memory_order_relaxed);
                impl_->build_interval = milliseconds(1000 / next);
                LOG_WARN_FMT("Debug panel: snapshot build takes {:.2f}ms on a {:.2f}ms frame, "
                             "auto-degraded to {}Hz (was {}Hz)",
                             costMs, frameMs, next, current);
            }
            impl_->slow_builds = 0;
        }
    } else {
        impl_->slow_builds = 0;
    }
}

#endif // SERVER_BUILD
