# 服务端 Web 调试面板 — 设计与实施计划

> 状态：初稿，待评审
> 目标构建：`BUILD_FOR_SERVER=ON`（`build-server/server/`）
> 影响范围：仅服务端；客户端与 WASM 构建应保持零改动

---

## 一、目标与边界

### 1.1 要解决的问题

服务端跑起来之后，游戏对象的状态全在内存里，外部无法观测。出现"敌人卡住不动""坐标漂移""碰撞没触发""客户端看到的和服务端不一致"这类问题时，只能靠加日志盲猜。

面板的目标是：**在浏览器里实时看到服务端每一帧的游戏对象状态**，把"盲猜"变成"看一眼"。

### 1.2 本期做（只读监控）

- 独立 HTTP 线程，浏览器打开即可看
- 游戏对象全量列表：id、类名、坐标、尺寸、速度、中心点、active/started/destroyed/movable
- 每个对象持有哪些组件（Collision / Move / Gravity / HealthBar 等）——排查"碰撞没触发"最有效
- 部分类型特有字段：Mario 血量与形态、Goomba 是否被踩扁/被火球打死、Koopa 状态机
- 物理世界统计（先留占位，见 §2 说明）
- 在线客户端列表：地址、连接状态、最后收包时间
- 面板自身开销指标：快照序号、构建耗时、平均帧时间

### 1.3 本期不做

- 命令下发（暂停 / 单步 / 踢人 / 改数值）——不做命令队列，架构因此简单很多
- 历史曲线、时间序列环形缓冲
- 日志与事件流聚合
- 内嵌前端到 exe（前端从磁盘读，改完刷新即生效）

---

## 二、现状与约束（已逐条核实）

| # | 事实 | 位置 | 对设计的影响 |
| --- | --- | --- | --- |
| 1 | C++ 标准是 **C++20** | `CMakeLists.txt:282` `cxx_std_20` | 可用 `std::atomic<std::shared_ptr>`，但本方案仍选 mutex（见 §3.2） |
| 2 | 源文件用 **GLOB_RECURSE** 收集，客户端与服务端**共用同一个 add_executable** | `CMakeLists.txt:279`、`L284-286` | `src/` 下新增的 `.cpp` 会被编进**客户端**，必须整文件 `#ifdef SERVER_BUILD` |
| 3 | header-only 库约定放 `lib/`，靠 `${LIB_DIR}` include 生效 | `CMakeLists.txt:277,301` | 放 `lib/httplib.h` 即零 CMake 改动 |
| 4 | 已有 **nlohmann/json 3.12.0** | `lib/nlohmann/json.hpp` | JSON 输出零成本 |
| 5 | `ws2_32` 已通过 SDL_net 间接链接 | `build-server/.../linkLibs.rsp` | httplib 不需要新增链接 |
| 6 | 服务端主循环是 `[[noreturn]] while(true)`，单线程无锁 | `GameEngine.cpp:131-148` | 面板线程必须独立，且不能直接读游戏对象 |
| 7 | 全仓库除 `Logger` 外**没有任何 mutex/atomic/thread** | grep 确认 | 引入线程要自己负责同步，没有现成设施 |
| 8 | `GameEngine::scene_manager` 是 private | `GameEngine.h:25` | 但快照生成点放在 `GameEngine::start()` 内部，**不需要新增 accessor** |
| 9 | `Scene::getGameObjects()` 是 public | `Scene.h:69` | 主线程可直接遍历 |
| 10 | `GameObject` 有 `getClassName()`，返回 "Mario"/"Goomba"/"Brick" | `GameObject.h:158` | 类型名直接可用，不需要反射 |
| 11 | `getComponent<T>()` 未命中会 `LOG_INFO_FMT` 打日志 | `GameObject.h:88-104` | 面板探测组件会刷爆 `log.txt`，**必须新增无日志的 `tryGetComponent<T>()`** |
| 12 | `rotation` 是 protected 且**没有 getter** | `GameObject.h` | 需补一个一行 getter |
| 13 | 网络序列化是**纯二进制 Packet、无字段名**，且 Brick/Ground/Box 没有实现 | `Packet.h`、`ISerializable.h` | **不能复用做 JSON**，必须独立写 dump 路径 |
| 14 | **物理子系统从未接入**：`usePhysics` 没有一处置 true，`PhysicsBodyComponent` 没有任何 `addComponent` 调用点 | grep 全仓库确认 | `Scene::getPhysicsWorld()` 恒为 `nullptr`，快照 `physics.enabled` 恒 false。**这是现状，不是面板 bug** |
| 15 | 实际在跑的是自研 `CollisionSystem`，不是 Box2D | `SuperMarioSceneMultiplayer.cpp:317` | 碰撞排查靠 `objects[].components` + 坐标，不靠 physics 段 |
| 16 | 服务端**没有** per-client RTT，也不记最后收包时间 | `NetworkManager.cpp:359-364` | 要展示需新增字段；RTT 本期不做（要改协议） |
| 17 | `Logger.h` 无条件 `#include <windows.h>` | `Logger.h:20-22` | **httplib 必须在 `.cpp` 里第一个 include**，否则 winsock 重定义编译失败 |
| 18 | 服务端输出目录是 `build-server/server/` | `CMakeLists.txt:58` | `../../web/debug` 正好指回源码树 |
| 19 | `Asset/config.json` 拷贝挂在 `PRE_BUILD` | `CMakeLists.txt:332` | MinGW 下目标已最新时不执行，改配置需 touch 源文件或用环境变量 |
| 20 | 已有 `network.webBridgePort: 8081` | `Asset/config.json` | 面板端口用 8090 避让 |


---

## 三、技术决策

### 3.1 线程模型

```
主线程（游戏循环 165Hz）                       HTTP 线程（httplib 线程池，2 线程）
──────────────────────────────────             ─────────────────────────────────────
GameEngine::start()
  scene_manager->update(dt)
    Scene::update（物理 step / 对象 update / 销毁清理）
    collisionSystem->checkCollisions()
    simple_network.update(dt)（收包 + 广播完成）
  ↓
  debug_panel->onFrame(scene, dt)
    ├ 限频判断（snapshotHz，默认 10）
    ├ 无人观看跳过（idleSkipSeconds，默认 5s）
    ├ buildDebugSnapshot() → JSON 字符串
    └ 加锁交换 shared_ptr
                                    ──────→   GET /api/snapshot
                                              加锁拷 shared_ptr（纳秒级）
                                              返回 *snap
```

**关键约束**：HTTP 线程永远不接触 `Scene*` / `GameObject*` / `b2World*`，只读一份不可变字符串。这从架构上消灭了 data race 和悬空指针。

**采样点选在 `scene_manager->update()` 之后**：此时本帧对象更新、碰撞结算、网络发包、销毁清理全部完成，拿到的是"本帧最终态"。而且放在 `GameEngine::start()` 里，任何场景都自动生效，不需要改场景代码。

### 3.2 快照同步：mutex + `shared_ptr<const std::string>`

- 主线程：`dump()` 成字符串 → 加锁换掉 `shared_ptr`
- HTTP 线程：加锁拷一份 `shared_ptr` → 解锁 → 返回内容
- 锁窗口只有一次指针拷贝（纳秒），旧快照在最后一个读者释放后自动回收，天然实现双缓冲

不选 `std::atomic<std::shared_ptr<const json>>`：10Hz 写、2Hz 读的频率下没有可测量收益，反而引入工具链差异（MinGW 与 MSVC 实现不同）。用 mutex 更稳。

**预序列化字符串而不是 json 对象**：HTTP 线程拿到就能直接发，不用在请求路径上重复 `dump()`。

### 3.3 httplib 引入方式：vendor 到 `lib/httplib.h`

不用 FetchContent。理由：

1. 项目已有先例（`lib/doctest`、`lib/nlohmann`），零 CMake 改动
2. 本机 GitHub 直连不通，FetchContent 会让 configure 阶段依赖代理
3. 只在 `SERVER_BUILD` 的一个 TU 里编译，客户端零成本

获取：走代理或镜像下载单头文件到 `lib/httplib.h`，落地后校验 `CPPHTTPLIB_VERSION`。版本号记入文档。

两个编译开关（写在 include 之前）：

```cpp
#define CPPHTTPLIB_THREAD_POOL_COUNT 2   // 默认按核数起线程，调试面板不需要那么多
#include <httplib.h>                      // 必须是本 TU 第一个 include（见 §2-17）
```

不定义 `CPPHTTPLIB_OPENSSL_SUPPORT`：纯 HTTP、局域网内用，避免引入 OpenSSL 与静态链接麻烦。

### 3.4 鉴权

- 主方式：请求头 `X-Panel-Token: <token>`，前端存 `sessionStorage`
- 兜底：`?token=` 参数，只为 curl/脚本方便
- **token 不进 URL、不进浏览器历史、不进 Referer**
- 校验用常量时间比较（逐字节 XOR 累加），失败返回 `401`，不做重定向
- 静态文件（`/`、`app.js`、`style.css`）不鉴权——页面壳不含数据，数据全在 `/api/*`
- 响应头统一加 `Cache-Control: no-store`、`X-Content-Type-Options: nosniff`、`Referrer-Policy: no-referrer`
- **token 为空时启动自动生成 16 位随机十六进制并打印到控制台**，绝不允许空 token 放行

### 3.5 前端交付：磁盘读取，源码树优先

`webRoot` 解析顺序，取第一个存在的目录：

1. 环境变量 `MARIO_PANEL_WEBROOT`
2. `config.json` 的 `debugPanel.webRoot`
3. `../../web/debug`（相对 exe 目录 → 命中源码树 `E:/Projects/GameEngine/web/debug`，开发场景）
4. `./web/debug`（相对 exe 目录 → 命中 CMake 拷贝的部署副本）

启动时打印实际使用的路径，避免"改了没生效"的扯皮。

开发时命中第 3 条，改 HTML 刷新浏览器即生效，**不需要重新编译**。exe 被搬到别处时自动回落到第 4 条。

---

## 四、快照数据模型

### 4.1 顶层结构

```json
{
  "meta": {
    "schemaVersion": 1,
    "seq": 4821,
    "uptimeSec": 421.37,
    "frame": 69531,
    "dtMs": 6.06,
    "avgFrameMs": 6.12,
    "buildCostUs": 84,
    "scene": "SuperMarioSceneMultiplayer",
    "objectCount": 6,
    "truncated": false,
    "nonFiniteObjects": 0
  },
  "objects": [ ... ],
  "physics": { "enabled": false, ... },
  "clients": { "count": 1, "unverified": 0, "list": [ ... ] }
}
```

### 4.2 `objects[]` 字段

```json
{
  "id": 3,
  "class": "Mario",
  "tag": "player:",
  "active": true,
  "started": true,
  "destroyed": false,
  "movable": true,
  "x": 412.5, "y": 512.0,
  "w": 48.0,  "h": 64.0,
  "vx": 0.0,  "vy": 0.0,
  "cx": 436.5, "cy": 544.0,
  "rotation": 0.0,
  "finite": true,
  "networkId": 3,
  "components": {
    "MoveComponent": true, "GravityComponent": true, "HealthBar": true,
    "Collision": true, "BoxCollision": true, "CircleCollision": false,
    "PhysicsBody": false
  },
  "physics": null,
  "typeData": { "kind": "Mario", "health": 2, "isBig": true, ... }
}
```

几个字段的排查价值：

| 字段 | 排查什么问题 |
| --- | --- |
| `started: false` | `start()` 从没执行——"对象完全不动"的常见根因 |
| `active: false` | 逻辑被冻结（不一定 bug，但要看是谁关的） |
| `destroyed: true` 持续出现 | 已标记销毁但下一帧没被清掉，清理路径有问题 |
| `components.Collision: false` | 碰撞不触发的直接原因 |
| `components.MoveComponent: false` | 对象不会移动的直接原因 |
| `networkId: null` | 静态对象（Brick/Ground/Box），不参与网络同步 |
| `finite: false` | 坐标炸成 NaN/Inf，前端洋红高亮 |


**`finite` 的必要性**：`nlohmann::dump()` 会把 NaN/Inf 静默序列化成 `null`，看起来像字段丢失。逐字段 `std::isfinite` 检查后显式输出 `finite:false`，并在 `meta.nonFiniteObjects` 计数。

### 4.3 类型特有字段：用 `dynamic_cast`，不给 GameObject 加虚函数

三种方案对比：

| 方案 | 结论 |
| --- | --- |
| A. `GameObject` 加虚函数 `appendDebugJson()` | 所有 gameplay 类都要 include nlohmann、都要 override，调试代码污染热路径类，删面板时要逐个回滚。**否决** |
| B. 面板侧 `dynamic_cast` | 调试代码全部集中在 `DebugSnapshot.cpp`，删面板 = 删文件；RTTI 已开启；10Hz 下开销可忽略。**采用** |
| C. 第一版不做 | 恰好丢掉"Goomba 为什么不动 / Koopa 卡在哪个状态"这类最关键信息。**否决** |


已确认存在的 getter（无需新增）：`Mario::getIsBig/getIsPlayer/isTransforming`、`Goomba::isDead`、`Koopa::getState/isShellMoving`、`HealthBar::getHealth/isDead`。

需补的 getter（都是一行）：`Goomba::isKilledByFireball`、`Koopa::isKilled`。

`HealthBar` 没有 `getMaxHealth()`，本期先输出固定值 3（源码里 `max_health = 3` 且无修改点），以后需要精确再加 getter。

---

## 五、HTTP 接口

| 方法 | 路径 | 鉴权 | 说明 |
| --- | --- | --- | --- |
| GET | `/` | 否 | 返回 `index.html` |
| GET | `/app.js` `/style.css` | 否 | 静态资源 |
| GET | `/api/health` | 是 | `{"ok":true,"seq":..,"ageMs":..,"uptimeSec":..}` |
| GET | `/api/snapshot` | 是 | 完整快照（compact） |
| GET | `/api/snapshot?pretty=1` | 是 | 缩进 2 空格，方便 curl 阅读 |
| 其他 `/api/*` | 是 | `404 {"error":"not_found"}` |


**只做两个接口的理由**：当前对象量级几十到几百，整包 20–100KB，2Hz 轮询毫无压力。多接口会引入"两次请求之间状态不一致"，反而妨碍排查。

鉴权统一在 `set_pre_routing_handler` 里拦截 `/api/` 前缀。

---

## 六、配置

### 6.1 `Asset/config.json` 新增段

```json
"debugPanel": {
    "enabled": true,
    "host": "0.0.0.0",
    "port": 8090,
    "token": "",
    "snapshotHz": 10,
    "maxObjects": 2000,
    "idleSkipSeconds": 5,
    "webRoot": ""
}
```

| 字段 | 结构体默认 | 说明 |
| --- | --- | --- |
| `enabled` | `false` | config 里显式写 `true` 才启动；结构体默认 false，避免测试意外起服务 |
| `host` | `"0.0.0.0"` | 局域网可访问；只想本机看改成 `127.0.0.1` |
| `port` | `8090` | 避让 `network.port=6666` 和 `webBridgePort=8081` |
| `token` | `""` | 空 = 启动自动生成并打印 |
| `snapshotHz` | `10` | clamp 到 1–60 |
| `maxObjects` | `2000` | 超出截断并置 `meta.truncated` |
| `idleSkipSeconds` | `5` | 无请求超过此时长则不构建快照 |
| `webRoot` | `""` | 空 = 按 §3.5 顺序自动解析 |


### 6.2 环境变量覆盖（优先级最高）

| 变量 | 作用 |
| --- | --- |
| `MARIO_PANEL_ENABLE=1/0` | 强制开关，不改 config |
| `MARIO_PANEL_HOST` | 换绑定地址 |
| `MARIO_PANEL_PORT` | 换端口 |
| `MARIO_PANEL_TOKEN` | 覆盖 token（真实 token 建议只走这里，不落盘） |
| `MARIO_PANEL_HZ` | 采样频率 |
| `MARIO_PANEL_WEBROOT` | 前端目录 |


解析顺序：环境变量 > config.json > 结构体默认值。

### 6.3 配置不写回 `save()`

`ConfigManager::save()` 会先读原文件再写回，未知段原样保留。不把 `debugPanel` 加进 `save()`，避免客户端把 token 落盘。

---

## 七、文件改动清单

### 7.1 新增

| 路径 | 作用 | 整文件 `#ifdef SERVER_BUILD` |
| --- | --- | --- |
| `lib/httplib.h` | vendor 的 cpp-httplib | 否（只在服务端 TU 里 include） |
| `src/Debug/DebugSnapshot.h` | 快照构建函数声明 | 是 |
| `src/Debug/DebugSnapshot.cpp` | 遍历 Scene / b2World / NetworkManager，产出 JSON | 是 |
| `src/Debug/ServerDebugPanel.h` | 面板类（PIMPL，不暴露 httplib） | 是 |
| `src/Debug/ServerDebugPanel.cpp` | HTTP 路由、鉴权、线程、快照发布 | 是 |
| `web/debug/index.html` | 页面骨架 | — |
| `web/debug/app.js` | 轮询、表格渲染、过滤、异常高亮 | — |
| `web/debug/style.css` | 样式 | — |
| `docs/server-debug-panel-plan.md` | 本设计文档 | — |
| `docs/server-debug-panel.md` | 使用说明（端口 / token / 排查手法） | — |


关键签名：

```cpp
// DebugSnapshot.h
std::string buildDebugSnapshot(Scene* scene,
                               const DebugSnapshotContext& ctx,
                               std::uint64_t* costUs);

// ServerDebugPanel.h —— 不 include httplib
class ServerDebugPanel {
public:
    ServerDebugPanel();
    ~ServerDebugPanel();
    ServerDebugPanel(const ServerDebugPanel&) = delete;
    ServerDebugPanel& operator=(const ServerDebugPanel&) = delete;

    bool start(const ConfigManager::DebugPanelConfig& cfg);
    void stop();
    void onFrame(Scene* scene, float dtSec);   // 主线程每帧调用

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
```

### 7.2 修改

| 文件 | 位置 | 改法 |
| --- | --- | --- |
| `CMakeLists.txt` | L331-341 `elseif (BUILD_FOR_SERVER)` 块内 | 追加 `copy_directory` 把 `web/debug` 拷到 exe 旁的 `web/debug` |
| `CMakeLists.txt` | L341 之后 | 加一个 `ALL` 的 phony target 做 web 资源同步（`PRE_BUILD` 在 MinGW 下目标最新时不执行） |
| `src/GameEngine.h` | L5-10 | 加 `#ifdef SERVER_BUILD class ServerDebugPanel; #endif` 前置声明 |
| `src/GameEngine.h` | L24-30 private 段 | 加 `#ifdef SERVER_BUILD std::unique_ptr<ServerDebugPanel> debug_panel; #endif` |
| `src/GameEngine.cpp` | L15-22 | 加 `#ifdef SERVER_BUILD #include "Debug/ServerDebugPanel.h" #endif` |
| `src/GameEngine.cpp` | L78-81 `init()` 末尾（`loadScene` 之后） | `debug_panel = std::make_unique<ServerDebugPanel>(); debug_panel->start(CONFIG.debugPanel);`。必须在 `current_path(getExeDir())` 和 `CONFIG.load()` 之后 |
| `src/GameEngine.cpp` | L141 `scene_manager->update()` 之后 | `#ifdef SERVER_BUILD if (debug_panel) debug_panel->onFrame(scene_manager->getCurrentScene().get(), dt); #endif` |
| `src/Manager/ConfigManager.h` | L85-100 后 | 加 `struct DebugPanelConfig { ... } debugPanel{};` |
| `src/Manager/ConfigManager.h` | L121-127 private | 加 `void parseDebugPanel(const json& j);` |
| `src/Manager/ConfigManager.cpp` | L31 后 | 加 `parseDebugPanel(config["debugPanel"]);` |
| `src/Manager/ConfigManager.cpp` | L40-46 `reset()` | 加 `debugPanel = DebugPanelConfig{};` |
| `src/Manager/ConfigManager.cpp` | L106 后 | 实现 `parseDebugPanel`，首行 `if (!j.is_object()) return;`，全部用 `j.value(key, 当前值)` |
| `src/Network/NetworkManager.h` | L92 后 public 段 | 加 `#ifdef SERVER_BUILD` 的 `struct ClientDebugInfo`、`collectClientDebugInfo() const`、`getUnverifiedCount() const` |
| `src/Network/NetworkManager.cpp` | 文件末尾 | 实现上述方法；`playerClass` 用 `dynamic_cast<GameObject*>(player.get())->getClassName()` |
| `src/Network/TcpClient.h` | `tryExtractPacket` 成功分支 | `#ifdef SERVER_BUILD` 更新 `last_receive_at / packets_received / bytes_received` |
| `src/Network/TcpClient.h` | 成员区 | `#ifdef SERVER_BUILD` 加三个字段；头部补 `#include <chrono>` |
| `src/GameObjects/GameObject.h` | L86-104 `getComponent` 之后 | 加**不打印日志**的 `tryGetComponent<T>()`（直接 `components.find`，未命中返回 `nullptr`） |
| `src/GameObjects/GameObject.h` | L134-136 `getSpeed()` 后 | 加 `float getRotation() const { return rotation; }` |
| `src/GameObjects/Goomba.h` | `isDead()` 附近 | 加 `bool isKilledByFireball() const { return killed_by_fireball; }` |
| `src/GameObjects/Koopa.h` | `getState()` 附近 | 加 `bool isKilled() const { return is_killed; }` |
| `Asset/config.json` | 顶层 | 加 `debugPanel` 段（见 §6.1） |


### 7.3 客户端隔离三条硬规则

1. **新增 `.cpp` 第一行有效代码是 `#ifdef SERVER_BUILD`，最后一行 `#endif`**。GLOB 会把它们编进客户端，条件编译后为空 TU。
2. **两个新头文件也整文件守卫**，且只从 `GameEngine.cpp` 的守卫分支 include。客户端连声明都看不到。
3. **`lib/httplib.h` 只在 `ServerDebugPanel.cpp` 里 include，且必须是第一个 include**（`Logger.h` 会带进 `windows.h`）。任何共享头（`Scene.h`、`GameObject.h`、`NetworkManager.h`）不得出现 httplib 或 nlohmann。

---

## 八、分阶段实施

### M1 — 骨架跑通：独立线程 + 鉴权 + health

**改动**：`lib/httplib.h`；`ServerDebugPanel.h/.cpp`（只做 `/api/health`）；`GameEngine` 接线；`ConfigManager` 加 `debugPanel` 段（先只读 enabled/host/port/token）。

**验收**：

- 启动日志打印 `Debug panel: http://0.0.0.0:8090/  token=xxxx`
- 带正确 token 的 `curl /api/health` 返回 `{"ok":true,...}`
- 不带 token / 错误 token 返回 401
- 端口被占用时控制台有明确错误，**游戏服务照常运行**
- `cmake --build --preset client` 仍然通过

### M2 — 对象快照 + 前端表格

**改动**：`DebugSnapshot.h/.cpp`（meta + objects）；`GameObject::tryGetComponent/getRotation`；`web/debug/*`；CMake 资源拷贝。

**验收**：

- `curl /api/snapshot | jq '.objects[] | select(.class=="Mario")'` 输出实时 x/y/vx/vy
- 客户端让马里奥向右走，浏览器表格里 x 在 500ms 内跟着变
- 页面可按 class 过滤、按 id 搜索；`active=false` 灰显、`started=false` 橙显、`finite=false` 洋红显

### M3 — 客户端列表 + 类型特有字段 + 物理占位

**改动**：`physics` 段（null-safe 遍历 `b2World`）；`NetworkManager::collectClientDebugInfo`；`TcpClient` 三个统计字段；`Goomba/Koopa` 两个 getter；`typeData`。

**验收**：

- `physics.enabled` 当前为 false（符合 §2-14 现状），物理接入后自动出现统计
- 踩扁一只 Goomba，500ms 内 `typeData.squashed=true`
- 踢一脚 Koopa，`state` 从 Walking → ShellIdle → ShellMoving 可见
- 关掉客户端，`clients.count` 下降；断网时 `lastRecvAgeMs` 持续增长

### M4 — 配置化与体验收尾

**改动**：环境变量覆盖；token 自动生成；webRoot 解析链；`idleSkipSeconds`；`?pretty=1`；前端"可疑对象"高亮（跨两次快照比较位移）；使用文档。

**验收**：

- `MARIO_PANEL_HZ=2` 启动即生效，不用改 config
- 关掉浏览器 5 秒后主循环零快照开销（对比 `meta.avgFrameMs`）
- "仅看可疑"筛选能挑出「`active && speed>1` 但位移≈0」的对象
- 文档里能查到端口、token、常见排查路径

---

## 九、风险与应对

| # | 风险 | 表现 | 应对 |
| --- | --- | --- | --- |
| 1 | 端口占用 | `listen()` 在子线程静默失败，面板"启动了但打不开" | 主线程先 `bind_to_port()`，失败立即报错并禁用面板；不做自动换端口 |
| 2 | token 泄露 | token 进 config 被提交，或拼在 URL 里进历史 | 默认自动随机生成；真实 token 走环境变量；前端用请求头；`Referrer-Policy: no-referrer` |
| 3 | 快照开销 | 对象多时主线程多出几毫秒 | 10Hz 限频 + 无人观看跳过 + `maxObjects` 截断 + `buildCostUs` 超阈值自动降频 |
| 4 | 线程生命周期 | 析构时 `std::thread` 仍 joinable → `terminate` | 固定顺序 `stop() → join() → 销毁 Server`；`bind` 在起线程之前 |
| 5 | 静态文件路径 | cwd 不对导致 404，或改完没生效 | 面板在 `current_path(getExeDir())` 之后启动；启动时打印实际 webRoot；目录不存在时返回内置提示页 |
| 6 | 客户端构建被污染 | 新 `.cpp` 被编进客户端，或共享头引入 httplib | §7.3 三条硬规则；每个里程碑都跑一遍 client preset |
| 7 | winsock 包含顺序 | `windows.h` 先于 `winsock2.h` → `timeval` 重定义 | httplib 必须是 `.cpp` 第一个 include；头文件用 PIMPL 隔离 |
| 8 | `getComponent` 刷日志 | 面板每帧盲探组件，`log.txt` 迅速膨胀 | 新增无日志的 `tryGetComponent<T>()`，面板只用它 |
| 9 | 数据竞争 | HTTP 线程遍历 `game_objects` 时对象被清 | 架构上禁止；HTTP 线程只读不可变字符串 |
| 10 | NaN 静默变 null | 坐标炸了但看起来像字段缺失 | 逐字段 `isfinite`，输出 `finite:false` + 计数 + 前端高亮 |
| 11 | 物理段恒空被误判 | 以为面板坏了 | schema 区分 `enabled:false` 与"有 world 但 0 body"；文档写明物理未接入 |
| 12 | Windows 防火墙弹窗 | 首次绑 `0.0.0.0` 提示是否允许 | 文档说明勾选"专用网络"；只想本机用就设 `127.0.0.1` |
| 13 | `GAME_WERROR=ON` 下 httplib 告警 | 第三方头带警告导致 CI 失败 | 在 include 外包 `#pragma GCC diagnostic push/ignored`，不全局关警告 |
| 14 | 多标签页打满线程池 | 每页 2Hz 轮询 | `visibilitychange` 时暂停轮询；线程池限 2；`keep_alive_timeout=5` |


---

## 十、验证方法

### 10.1 鉴权（不是"返回 200"就算过）

```
curl -s -o /dev/null -w "%{http_code}\n" http://127.0.0.1:8090/api/snapshot          # 期望 401
curl -s -o /dev/null -w "%{http_code}\n" -H "X-Panel-Token: wrong" .../api/snapshot    # 期望 401
curl -s -D - -o /dev/null -H "X-Panel-Token: $TOKEN" .../api/snapshot | grep -i content-type
# 期望 Content-Type: application/json
```

### 10.2 数据正确性：拿地图 JSON 当真值

`Asset/maps/level_1.json` 里第一块砖是 `{"x": 1373, "y": 584}`。

```
curl -s -H "X-Panel-Token: $TOKEN" .../api/snapshot \
 | jq '.objects[] | select(.class=="Brick") | [.id,.x,.y]' | head -5
```

必须能查到 x=1373、y=584 的 Brick。对不上说明读错了对象或单位混了（`GameObject::position` 是像素，不受 Box2D PPM=64 影响）。

### 10.3 快照新鲜度

连续三次取 `meta.seq`，必须递增，差值 ≈ snapshotHz。`seq` 不动但服务端日志还在滚 → 被限频或无人观看跳过；日志也不动 → 主循环卡死，与面板无关。

### 10.4 性能：证明没拖慢主循环

面板开启 + 浏览器 2Hz 轮询，稳定 60 秒后对比 `meta.avgFrameMs`，与关闭时差值应 < 0.2ms；`buildCostUs` 稳定在 200µs 以内。

### 10.5 状态字段端到端

| 操作 | 期望 |
| --- | --- |
| 踩扁 Goomba | 500ms 内 `squashed=true`、`dead=true`，几秒后对象消失 |
| 踢静止 Koopa | `stateCode` 1→2，`shellMoving=true` |
| 吃蘑菇 | `isBig` false→true，w/h 变大 |
| 客户端断网 | `lastRecvAgeMs` 持续增长，超时后 `count` 下降 |
| 跑 1 小时 | `seq` 单调、对象数不持续增长、RSS 平稳 |


### 10.6 并发压测

8 路并发 curl 快照 + 正常游玩 10 分钟。判据：不崩、`log.txt` 无访问违例、`seq` 无回退。

---

## 十一、已知限制

1. **物理段恒为空**：`usePhysics` 从未启用，`PhysicsBodyComponent` 没有任何调用点。面板对 physics 全程 null-safe，接入后自动有数据。
2. **无 per-client RTT**：服务端 ClientPing 只是原样回包，不记时间戳。要做需要改协议（ServerPing/ClientPong），不属于只读面板范围。本期只提供 `lastRecvAgeMs`。
3. **`HealthBar::maxHealth` 固定输出 3**：没有 getter，源码里也无修改点。需要精确时再加。
4. **组件类型名探测是硬编码表**：面板只探测 7 个已知组件类型。新增组件类型需在 `DebugSnapshot.cpp` 的探测表里登记。
5. **Ctrl+C 不走析构**：`GameEngine::start()` 是 `[[noreturn]]`，进程被强杀时 `stop()` 不会执行。属预期行为，不需要处理。
6. **前端未做移动端适配**：表格在手机上会很挤，本期只保证桌面浏览器可用。