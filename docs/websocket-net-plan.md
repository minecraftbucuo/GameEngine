# Web 联机方案计划（websocket-net）

> 前置：《wasm-web-port-plan》已于 2026-08-24 收官（网页版单机全流程可玩）。
> 本文将其阶段四登记的三个方向展开为可执行方案。
> 状态：**计划评审中**——实施未开始，待运营决策 Q1~Q3 确认后拉分支开工。

## 一、目标与非目标

**用户诉求**：网页版马里奥能联机，"只有单机没意思"。

| 场景 | 可行性 | 说明 |
| --- | --- | --- |
| S1 网页客户端 ↔ 局域网桌面服务器 | ✅ 本期目标 | 朋友电脑开服，浏览器加入 |
| S2 网页客户端 ↔ 公网常驻服务器 | ✅ 同链路 | VPS 跑 headless 服务端（SERVER_BUILD 现成）+ 桥，属部署问题 |
| S3 网页 ↔ 网页直连 | ❌ 非目标 | 浏览器无法监听端口，P2P 需 WebRTC 数据通道，成本另计 |

**核心约束**：浏览器只能发起 WebSocket 连接，不能建裸 TCP。
⇒ 无论 S1/S2，链路中必须有一个 **WS↔TCP 桥**组件，游戏服务端本体保持纯 TCP 不动。

## 二、方向选型（2026-08-24 N1 审计后修订：选定 A′）

| | ~~方向 A~~：websockify 桥 + SOCKFS 透明隧道 | **方向 A′：TcpClient 换 websocket.h 后端 + websockify 桥（选定）** | 方向 B：两端全量 WS 化 |
| --- | --- | --- | --- |
| 客户端代码 | 近乎零改动 | TcpClient 加 `__EMSCRIPTEN__` 分支（约 150 行，接口不变） | 重写 connect/send/recv 为 `<emscripten/websocket.h>` |
| 服务端代码 | 零改动 | **零改动**（继续 SDL_net 纯 TCP，桥转译回 TCP） | 需实现 RFC6455 握手+帧解析 |
| pthread | 不需要 | **不需要** | 不需要 |
| 致命问题 | ❌ **已被 N1 证伪**：见下 | 无已知阻断 | 工程量约 A′ 的 5~10 倍 |

> **N1 审计结论（2026-08-24，方向 A 否决依据）**：
> [SDL_net.c L1211-L1215](../build-web/_deps/sdl_net-src/src/SDL_net.c)：`NET_Init()` 无条件自旋
> `MIN_RESOLVER_THREADS=2` 个解析线程，Emscripten 无 pthread 时线程创建失败 ⇒ **NET_Init 整体失败**
> ⇒ SOCKFS 透明隧道方案在「不开 pthread」（wasm 计划决策 6）前提下无法成立。
> 所幸 [NET_Init 是运行期按需调用](../src/Network/NetworkManager.cpp#L34)（startServer/connectToServer 内），
> 当前网页版因 Step 5 已拦截这两条路径而未受影响。

**A′ 设计要点**：`TcpClient` 对外契约（Status 四态 / connect / append+send / receive / disconnect）
原样保留，仅在类内部 `#ifdef __EMSCRIPTEN__` 切换为 `<emscripten/websocket.h>` 实现；
uint32 大端长度前缀分帧与 `m_recvBuf` 拆帧状态机**原样复用**（它们不依赖传输层）；
链路上的 websockify 桥把浏览器的 WebSocket 转译回裸 TCP，游戏服务端零感知。
方向 C（维持离线）已被用户否决。

## 三、有利事实（基于 TcpClient.h 审计，2026-08-24）

1. 全部 I/O 经 `NET_StreamSocket` 抽象 ⇒ SOCKFS 拦截 BSD socket 层即可生效，TcpClient 逻辑原样运行
2. SDL_net 无阻塞语义（setBlocking 已随 SFML 迁移移除）⇒ 无阻塞主线程风险
3. 自研 `uint32 大端长度前缀` + `m_recvBuf` 拆帧状态机 ⇒ **WS 消息边界差异天然免疫**
   （半包/粘包/任意切分都被状态机吸收），线格式与桌面版逐字节兼容，Packet 协议复用
4. 服务端 `SERVER_BUILD` 与客户端代码本就分离 ⇒ 服务端零感知

## 四、风险清单（2026-08-24 随 A′ 修订）

| # | 风险 | 应对 |
| --- | --- | --- |
| ~~R1~~ | ~~NET_ResolveHostname 依赖解析线程~~ | **已证实为阻断**（见 N1 结论），A′ 方案整体绕开 SDL_net 初始化 |
| R2′ | websocket.h 的 send/recv 语义与 TCP 流差异（消息边界、背压） | 拆帧状态机天然吸收；N2 实测验证 |
| R3 | https 页面禁止 ws:// 明文（混合内容策略），上线必须 wss | 开发期 http://localhost + ws:// 不受限；公网由反代终结 TLS（N5） |
| R4 | websockify 为常驻进程：S1 场景朋友要同时跑「服务端 exe + 桥」两件东西 | N5 写一键启动脚本/说明降低使用门槛 |
| R5′ | `connect()` 在 websocket.h 下是异步握手，需映射为 Status 四态（Done/NotReady/Disconnected/Error） | N2 实现时以轮询 readyState 的方式对齐既有契约 |
| R6 | websockify 默认子协议协商与 websocket.h 的 `binary` 子协议需匹配 | N2 探测首验项；不一致时桥侧加 `--ws-subprotocol` 参数 |

## 五、分步计划（延续原子性纪律：每步独立构建验证）

### Step N0 — 环境准备（用户执行）✅ 完成（2026-08-24，Python 3.12.5 + websockify 已装）
- [x] `pip install websockify`；`python -m http.server` 可用
- **产出**：桥工具就绪

### Step N1 — 静态审计 ✅ 完成（2026-08-24，结论见第二节引用块）
- [x] 读 vendored sdl_net-src：证实 `NET_Init()` 硬依赖线程 ⇒ 方向 A 否决，改选 A′
- [x] 修订本文档选型表、风险清单与步骤

### Step N2 — TcpClient WEB 后端 + 最小链路探测 ✅ 完成（2026-08-24，go/no-go 门通过）
- [x] 代码侧完成（2026-08-24，`__EMSCRIPTEN__` 后端已并入，细节见下方「N2 实现纪要」）
- [x] 用户实测通过：桌面开服（CONFIG.network.port=6666）+ `websockify 8081 127.0.0.1:6666`
      + 网页点「超级玛丽 Client（测试）」按钮，联机正常
- [x] R6 子协议匹配确认（websocket.h 声明 `binary`，桥零参数直通）
- **通过标准已达成**：网页端联机正常 ⇔ 方向 A′ 整体成立，后续步骤按计划推进
- **改动文件**：`src/Network/TcpClient.h`、`src/Network/NetworkManager.h/.cpp`、
  `src/Scene/MenuScene.cpp`、`CMakeLists.txt`
- **原子性保证**：桌面/服务端构建不进任何新增 ifdef 分支，行为零变化

**N2 实现纪要（2026-08-24）**
- `TcpClient::SocketHolder`（WEB 形态）：WS 句柄 + connecting/open/closed/errored 四个
  状态位 + `staged` 暂存缓冲；onmessage 回调只搬字节，回调内严禁发送（websocket.h 约束）
- connect：构造 `ws://addr:port`（或透传完整 ws/wss URL），声明 `binary` 子协议；
  本版 emsdk 的 `emscripten_websocket_new` **创建即连接**（无独立 connect 函数，
  句柄类型为宏 `EMSCRIPTEN_WEBSOCKET_T`＝int，成功值 >0），new 后注册回调
  （事件走 JS 事件循环，同步块内无竞态），乐观返回 Done，真实结果走事件回调
- 四态折算：receive() 先并 staged → 拆帧状态机（WS 消息边界差异天然免疫，R2′ 兑现）；
  有帧交 Done；errored→Error；closed→Disconnected；否则 NotReady。
  tryFlush 在 OPEN 前返回 false 并保留整帧 —— 验证包「先入队、握手完成后自动冲刷」
- NetworkManager 两处异步化：①客户端路径跳过 SDL_Init/NET_Init（N1 结论）；
  ②`connectToServer` 的阻塞验证轮询在 WEB 下换成乐观返回 + `verifyPending` 标志，
  首条 bool+string 应答由 `clientUpdate` 每帧收口（阻塞轮询会卡死浏览器事件循环）
- NetworkManager 析构的 NET_Quit 在 WEB 下防护（未 Init 严禁 Quit）
- MenuScene 新增 WEB 专属「超级玛丽 Client（测试）」按钮（写死桥地址，N3 转正式可配置）

### Step N3 — 正式入口改造 ✅ 代码完成（2026-08-24，待用户回归）
- [x] MenuScene WEB 下「超级玛丽 Client」按钮正式化（与「超级玛丽（单机）」并列双入口）
- [x] NetworkManager WEB 下 `connectToServer` 放行（N2 已随异步化一并解除一票否决）
- [x] 连接地址走 config.json：`serverIp` 支持完整 `ws(s)://` URL 直连，否则按
      「ws://serverIp:webBridgePort」拼桥地址；新增 `webBridgePort` 键（默认 8081），
      `port` 键语义不变（桌面 TCP 直连用）；Step 8 已决策设置不持久，手工编辑 config 重打包即可
- [x] 顺带修复断线重连三处隐患（WEB）：connect 前复位 holder 状态位/删除旧句柄注销旧回调/
      清空三层缓冲（staged + m_recvBuf 半帧 + m_sendBuf/m_outgoing 待发帧）——否则二次
      连接因 closed 残留立即被判 Disconnected，且旧流半帧混入新流会拆帧错位越界读
- [x] 断线时清 verifyPending（防验证中途断线的标志跨连接残留）
- **验证**：网页经桥联机完整对局；桌面版三按钮布局不变；关服→重开服→网页重连成功
- **改动文件**：`src/Scene/MenuScene.cpp`、`src/Network/NetworkManager.cpp`、
  `src/Network/TcpClient.h`、`src/Manager/ConfigManager.h/.cpp`、`src/Asset/config.json`

### Step N4 — 断线与体验打磨（进行中）
- [ ] **待查证**：用户报告「关闭服务端后网页客户端像刷新一样自动重启」——代码层全栈
      排查（shell.html / 引擎 / SDL web 后端 / Emscripten JS 胶水）均无 reload 调用；
      主流怀疑：WASM 未捕获异常 → 进程 abort → 浏览器崩溃恢复自动重载标签页。
      **下一步取证**：F12 Console 开着复现，抓异常栈（CppException/abort 字样）；
      同时确认看到的是「GAME ENGINE 加载进度条重现」（真页面刷新）还是「直接回菜单」
      （场景切换）——二者根因完全不同
- [ ] Disconnected/Error 时玩家侧明确反馈（LOG + 场景内提示或退回菜单），杜绝静默假死
      （当前行为：静默转 None，本地玩家可继续单机式游玩，远端玩家冻结）
- [ ] 记录操作延迟体感基线；若明显劣化再议输入预测（不在本期承诺）
- **改动文件**：`src/Scene/SuperMarioScene.cpp`（或 NetworkManager 回调）

### Step N5 — 部署拓扑文档化
- [ ] S1 手册：朋友开服 = 启动服务端 exe + 一键脚本拉起 websockify（写 .ps1/.bat）
- [ ] S2 拓扑：VPS 上 `nginx(wss 终结) → websockify → GameEngineServer(systemd)` 配置样例
- **产出**：`docs/web-multiplayer-deploy.md`（或本文附录）

## 六、运营决策（2026-08-24 已全部拍板）

- **Q1 桥进程**：✅ 用户接受 websockify 桥作为本期基础设施
- **Q2 目标场景**：✅ 已澄清——保持与桌面版完全一致的「Client 填 IP 加入 Server」模式，
  联机体验不变；先局域网（S1）打通属自然开发顺序，公网（S2）仅是后续部署事项
- **Q3 测试拓扑**：✅ 默认本机三件套同机跑（http.server + websockify + 桌面服务端）

## 七、分支策略

从 `wasm-emscripten` 拉新分支 **`websocket-net`** 实施（N2 起）。wasm 分支建议先行合并 master，
避免联机工作叠在未合流的长分支上；合并时机由用户定。
