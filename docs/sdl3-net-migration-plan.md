# 网络层 SDL_net 3 迁移计划 — SFML 全仓移除（终章）

## 一、目标与终态

**终态**：`Network/` 模块与全部游戏层序列化代码基于 SDL_net 3；SFML 在本仓**完全移除**
（源码零引用、CMake 零链接、lib/ 目录删除）。完成后 SFML 在依赖图里不存在。

**附带收益**：SDL_net 3 main 分支已带 Emscripten 后端（Maelstrom 官方验证），
将来 Web 版客户端网络层零改动。

**范围**：
- `Network/` 5 文件 + 游戏层 sf::Packet 触点（Mario / FireBall / MarioController /
  Scene / SuperMarioScene）+ CMake；
- 客户端（C/S 同体 exe）与专用服务端（SERVER_BUILD）同步迁移——网络是两端共用模块；
- 线格式**与 sf::Packet 逐字节保持一致**（uint32 长度前缀 + host 字节序字段编码），
  每一步切换后新旧 exe 可互通，天然具备回滚验证手段。

**分工约定**：文件修改由助手完成；git 提交、编译、运行验证（含双端联机回归）由用户执行。

---

## 二、现状分析

### 1. 耦合面盘点（grep 全量核实，2026-08-23）

| 耦合点 | 位置 | 内容 |
|--------|------|------|
| Socket 封装 | [TcpClient.h](file:///e:/Projects/GameEngine/src/Network/TcpClient.h) | sf::TcpSocket + sf::Packet 聚合发送；`sf::Socket::Status` 返回值 |
| 监听器 | [NetworkManager.h:70](file:///e:/Projects/GameEngine/src/Network/NetworkManager.h) | sf::TcpListener listen/accept |
| 计时 | NetworkManager.h:72/65 | sf::Clock（未验证超时）、sf::seconds(10) |
| 序列化契约 | [ISerializable.h](file:///e:/Projects/GameEngine/src/Network/ISerializable.h) | serialize/deserialize(sf::Packet&) 纯虚签名 |
| 枚举运算符 | [NetworkProtocol.h](file:///e:/Projects/GameEngine/src/Network/NetworkProtocol.h) | 枚举 << >> 模板（sf::Packet） |
| 游戏层触点 | Mario.h/.cpp、FireBall.h/.cpp、MarioController.cpp(5处)、Scene.h:44、SuperMarioScene.h/.cpp | sf::Packet 局部变量 + << >> 读写 |
| CMake | [CMakeLists.txt](file:///e:/Projects/GameEngine/CMakeLists.txt) L17-47/L169/L184-202 | 三套 SFML 链接分支 + bundled lib/ 目录 |

Packet 实际流经的数据类型（全量归纳，运算符覆盖以此为准）：
`bool`、`int`、`unsigned int`、`float`、`std::string`、枚举（uint8 底层）。
无 double / 64 位整型 / 自定义二进制块。

### 2. SDL_net 3 关键差异（迁移设计依据）

| sf 语义 | SDL_net 3 对应 | 差异处理 |
|---------|----------------|----------|
| 阻塞 connect(addr, port, timeout) | NET_CreateClient（异步）+ NET_WaitUntilConnected(timeout) | TcpClient::connect 内部封装，对外保持阻塞语义 |
| setBlocking(false) + NotReady | 全 API 天然非阻塞（内部线程 I/O） | setBlocking 删除（调用点一并删） |
| send(Packet)（含长度前缀，同步发完） | NET_WriteToStreamSocket（异步入队，线程冲刷） | 组帧逻辑搬到 TcpClient（自研长度前缀），发送语义差异无碍（每帧聚合小包） |
| receive(Packet)（Done/NotReady/Disconnected/Error） | NET_ReadFromStreamSocket（返回字节数/-1） | 接收缓冲 + 组帧解析在 TcpClient，状态枚举复刻 sf 四态 |
| TcpListener::accept | NET_AcceptClient（bool） | 直接映射 |
| getRemoteAddress/Port | NET_GetStreamSocketAddress + NET_GetAddressString | 日志用；端口取不到则只记地址 |
| —— | NET_Init / NET_Quit | 新增生命周期管理（惰性初始化于 startServer/connectToServer） |

### 3. 已核实的依赖事实

- SDL_net 3 **无 release tag**（GitHub 最新 release 仍是 2.2.0），main 分支 3.3.0-dev
  活跃维护（2026-06 仍有提交）→ FetchContent **pin 具体 commit**（N2 实现时锁定）；
- SDL_net 依赖 SDL3 核心 → **SDL 本体须从客户端块提升为两端共用**（服务端目前零 SDL）；
- Maelstrom（SDL 作者官方项目）已用 main 分支 SDL_net 同时构建原生 + wasm，可行性背书。

---

## 三、迁移策略

延续渲染迁移的原子步进打法：每步可编译、可运行、可回滚；先立新（纯新增）、
再切类型层（SFML 之上复刻组帧，行为等价）、最后换传输层（一步到位）、收官删除。

**两段式切换是本计划的核心设计**：
- 类型层先切（N3）：`sf::Packet → eng::Packet`，但传输层仍是 sf::TcpSocket 裸字节。
  组帧由我们自研，与 sf::Packet 线格式逐字节一致 → 行为等价，且可与旧 exe 互通验证；
- 传输层后切（N4）：TcpClient 内部 sf::TcpSocket → NET_StreamSocket，游戏层零感知。

```
N1  eng::Packet 落盘（纯新增，零引用）
N2  CMake 接入 SDL_net + SDL 核心提升两端共用（只拉取，不切源码）
N3  类型层切换：Packet/签名/Status/Clock 全换，传输层仍 SFML ── 行为等价，可联机回归
N4  传输层切换：TcpClient/listener 内部换 SDL_net，CMake 链 SDL3_net
N5  SFML 全仓移除：CMake 三套分支清零 + lib/ 目录删除 + 验收
```

---

## 四、步骤明细

### N1：eng::Packet 自研序列化容器（纯新增）

**新增** `src/Network/Packet.h` + `Packet.cpp`，命名空间 `eng`：

- 线格式**逐字节复刻 sf::Packet**：
  - 流帧：`uint32 长度前缀 + payload`（host 字节序，memcpy 原始内存——与 sf 一致）；
  - 字段编码：`bool`=1B、`int32/uint32`=4B、`float`=4B、`std::string`=uint32 长度+字节流；
- 运算符：`<<`/`>>` 覆盖 Packet 实际流经的全部类型（见二.1 归纳）+ 通用枚举模板
  （NetworkProtocol.h 的枚举模板届时改指向 eng::Packet，避免重复）；
- `operator bool` 失效语义（读越界/类型错位 → false），支撑 `while (packet >> x)` 惯用法；
- `append(data, size)` / `getDataSize()` / `clear()` 供 TcpClient 组帧使用。

**验证**：双版本编译通过（纯新增零引用）。**回滚**：删两个文件。

### N2：SDL_net 依赖接入 + SDL 核心提升两端共用

**CMake**：
- SDL 本体 FetchContent 从 `if (NOT BUILD_FOR_SERVER)` 块中**拆出**，提升为无条件
  （客户端与服务端共用；image/ttf/mixer 仍留客户端块）；
- 新增 SDL_net FetchContent（pin main 分支具体 commit，GIT_SHALLOW）；
- 本步**不链接** SDL3_net（对齐渲染迁移 Step 8"只拉不链"惯例）。

**验证**：双版本编译通过；服务端构建首次拉取 SDL 本体（编译时间增加属预期）。
**回滚**：还原 CMake。

### N3：类型层切换（游戏层 sf::Packet 清零，传输层仍 SFML）

| 文件 | 改动 |
|------|------|
| TcpClient.h | **重写**：对外接口保持（connect/append/receive/send/disconnect），内部改「sf::TcpSocket **裸字节** receive + 自研 uint32 长度前缀组帧 → eng::Packet」；新增嵌套 `enum class Status {Done, NotReady, Disconnected, Error}` 复刻 sf 四态；`getSocket()` 收窄为私有实现细节；`setBlocking` 删除 |
| ISerializable.h | 签名 `sf::Packet&` → `eng::Packet&`；include 换 Packet.h |
| NetworkProtocol.h | 枚举模板 `sf::Packet&` → `eng::Packet&`（若 N1 已内置枚举支持则删模板） |
| NetworkManager.h/.cpp | 全部 `sf::Packet` → `eng::Packet`；`sf::Socket::Status` → `TcpClient::Status`；`sf::Clock` → `std::chrono::steady_clock`；`VERIFY_TIMEOUT` → `eng::Time::seconds(10)`；listener 仍 sf::TcpListener |
| Scene.h、SuperMarioScene.h/.cpp、Mario.h/.cpp、FireBall.h/.cpp、MarioController.cpp | `sf::Packet` 类型名替换（逻辑零改动——运算符语义已复刻） |

**验证**：双版本编译；**联机回归**（服务端 + ≥2 客户端：连接/验证/生成/同步/重生/断开）；
新旧 exe 互通测试（N3 服务端 + 旧客户端，线格式一致应当互通）。
**回滚**：git revert 本步，SFML 路径原样保留。

### N4：传输层切换（Network/ 内部 sf 清零）

- TcpClient 内部：`sf::TcpSocket` → `NET_StreamSocket`；
  - connect：NET_ResolveHostname（解析等待）→ NET_CreateClient → NET_WaitUntilConnected(timeout)；
  - receive：NET_ReadFromStreamSocket → 内部接收缓冲 → 长度前缀组帧 → eng::Packet；断连映射 Disconnected；
  - send：长度前缀 + NET_WriteToStreamSocket（异步入队）；
- NetworkManager：`sf::TcpListener` → `NET_Server`（listen → NET_CreateServer；
  accept → NET_AcpectClient 轮询）；远端地址日志 → NET_GetAddressString；
  - NET_Init 惰性调用（startServer / connectToServer 入口），析构 NET_Quit；
- CMake：两端目标链接 `SDL3_net::SDL3_net`（服务端同时获得 SDL3::SDL3）。

**验证**：双版本编译；联机回归全项；确认无 SFML 运行时残留（进程模块表）。
**回滚**：git revert 本步回到 N3（SFML 传输层）。

### N5：SFML 全仓移除（收官）

- CMake：删 BUILD_SFML_FROM_SOURCE 分支、find_package(SFML)、bundled 路径、
  SFML_ROOT include、ws2_32/winmm 链接、`-DSFML_STATIC`；
- 删除 `lib/SFML-2.6.1-gcc`、`lib/SFML-2.6.1-linux` 目录；
- grep 验收：全仓（src/ + CMake）`sf::` 与 `<SFML` 零命中；
- 本计划验收清单勾选 + sdl3-migration-plan.md 交叉引用更新。

**验证**：双版本编译 + 联机回归全项。

---

## 五、风险与待查证点（实现时逐一落实）

| # | 风险/疑点 | 处置 |
|---|-----------|------|
| 1 | SDL_net 3 API 签名以本地拉取的头文件为准（main 分支无 release，文档可能滞后） | N2 拉取后通读 `SDL_net.h`，N4 实现前出 API 映射表 |
| 2 | 版本 pin：无 tag → commit hash，将来升级需手动 | commit 写死在 CMake 并注释日期 |
| 3 | 服务端此前零 SDL 依赖，N2 起拉 SDL 本体 | 仅静态库；编译时间增加属预期，一次到位 |
| 4 | SERVER_BUILD 进程无 SDL_Init（无窗口）→ NET_Init 是否需先 SDL_Init(0) | 查 SDL_net 源码初始化路径；必要时 startServer 前补 SDL_Init(0) |
| 5 | 断连检测语义差异（sf::Disconnected vs SDL_net 读 -1/连接状态） | N4 对头文件核，映射进 TcpClient::Status |
| 6 | SDL_net 发送为异步队列（send 返回 ≠ 已发出） | 我们本就每帧聚合小包、帧末统一 send，语义无碍；记录在案 |
| 7 | NET_ResolveHostname 异步解析需等待 | 封装在 connect 内部轮询（带 CONFIG.network.timeout），对外保持阻塞语义 |

---

## 六、验收标准

- [ ] `src/` 全仓与 CMake 零 SFML 引用；lib/ 目录已删除
- [ ] 双版本（客户端 / SERVER_BUILD）编译通过
- [ ] 联机回归全项通过：连接验证 / 玩家生成与同步 / 输入转发 / 重生 / 火球 / 断开清理
- [ ] 客户端网络延迟手感与迁移前无可感知差异（同为 TCP + 同 tick 节奏）
- [ ] SDL_net pin commit 固化，构建可复现

---

## 七、进度记录

### N1 完成（2026-08-23）

- 新增 [Packet.h](file:///e:/Projects/GameEngine/src/Network/Packet.h) / [Packet.cpp](file:///e:/Projects/GameEngine/src/Network/Packet.cpp)（`eng::Packet`，纯新增零引用）
- **实测修正了计划里的线格式假设**：sf::Packet 整型走 htonl（**大端**），
  float 却是原始内存直拷（小端原样），string 长度前缀为大端 uint32。
  已用项目自带 SFML 静态库做逐字节对比测试：
  `bool×2 + int32 + uint32 + float + string(20字符) + uint8枚举` 共 39 字节
  **BYTE-IDENTICAL**，往返读取、流读尽终止（`while(packet>>x)`）、越界置无效全部通过。
  （测试程序用后已删；如需复测，重新拼一个 main 引 Packet.h 与 NetworkProtocol.h 即可）
- 设计细节：枚举按底层类型分派（1B 直拷 / 4B 大端）；`operator bool` 失效语义
  复刻 sf 惯用法；`append/getDataSize/clear` 供 N3 TcpClient 组帧使用。
- 待用户验证：双版本编译通过（Packet.cpp 会被 GLOB 自动收编，纯新增不触碰现有代码）。

### N2 完成（2026-08-23）

- [CMakeLists.txt](file:///e:/Projects/GameEngine/CMakeLists.txt)：SDL 本体（release-3.4.14）
  从 `NOT BUILD_FOR_SERVER` 块提升为**两端无条件拉取**；新增 SDL_net FetchContent，
  pin main 分支 commit `4dd9d84`（2026-06-17，查 GitHub API 核实为最新）；
  客户端块只保留 image/ttf/mixer。
- **选项名实测纠正**（吸取 Step 8 教训，先拉源码 grep 再写）：
  SDL_net 3 无 `SDLNET_STATIC` 专有开关，走标准 `BUILD_SHARED_LIBS`
  （已设 `OFF` 缓存 FORCE，防子项目 `cmake_dependent_option` 默认值覆盖）；
  子项目模式下检测到 `SDL3::SDL3` 目标即跳过 `find_package(SDL3)`
  （CMakeLists.txt L79-81 原文核实），故 `MakeAvailable(SDL SDL_net)` 同批顺序调用即可。
- 对齐渲染迁移 Step 8 惯例：**只拉取构建，不链接**——主目标（客户端/服务端）均未
  `target_link_libraries(SDL3_net)`，N4 才链。
- 影响：服务端构建首次拉取并编译 SDL 本体 + SDL_net 静态库（编译时间增加属预期，
  计划风险 #3 兑现，一次到位）；客户端行为零变化（SDL 本体同 tag 不会重编）。

### N3 完成（2026-08-23）

**类型层切换：游戏层 sf::Packet/sf::Socket::Status/sf::Clock 全部清零，传输层仍 SFML。**

- [TcpClient.h](file:///e:/Projects/GameEngine/src/Network/TcpClient.h) 重写：
  - 对外契约 `eng::Packet` + 自有 `Status` 四态枚举；`getSocket()` 移除，
    改暴露 `sendImmediate`（握手单发）/ `acceptFrom` / `getRemoteAddress` / `getRemotePort`
  - 传输层 sf::TcpSocket **裸字节** + 自研组帧：
    发送 = uint32 大端长度前缀 + payload（已核 SFML 2.6.1 TcpSocket.cpp L267 htonl 源码），
    内核缓冲满时暂存 m_sendBuf 待下帧冲刷（复刻 sf Partial 语义）；
    接收 = 4096B 裸收 → m_recvBuf 拆帧状态机 → 完整包弹出
- [NetworkProtocol.h](file:///e:/Projects/GameEngine/src/Network/NetworkProtocol.h)：
  枚举 operator<</>> 模板删除（eng::Packet 已内置同语义模板），只留协议定义
- [ISerializable.h](file:///e:/Projects/GameEngine/src/Network/ISerializable.h)：契约签名切 eng::Packet
- [NetworkManager.h](file:///e:/Projects/GameEngine/src/Network/NetworkManager.h)/.cpp：
  全部 sf::Packet→eng::Packet、sf::Socket::Status→TcpClient::Status（listener 除外）、
  sf::Clock→std::chrono::steady_clock、VERIFY_TIMEOUT→chrono::seconds(10)
- 游戏层 7 文件纯类型替换：MarioController.cpp(6处)、Mario.h/.cpp、FireBall.h/.cpp、
  Scene.h、SuperMarioScene.h/.cpp（逻辑零改动）
- **真实 socket 回环互通测试 6/6 全过**（测试程序用后已删）：聚合发送→sf::Packet 解包、
  sf::Packet 发送→自研拆帧、背靠背三帧粘包拆分、对端关闭→Disconnected。
  等价于实测"新旧 exe 可互联"。
- 残留 sf（计划内，N4 清）：TcpClient.h 传输层、NetworkManager 的 listener 两处。
- 待用户验证：双版本编译 + 双客户端联机回归（连接/验证/生成/同步/火球/重生/断开）。

### N4 完成（2026-08-23）

**传输层切换：src/ 代码层 SFML 引用彻底清零（仅余历史注释），SDL_net 上线。**

- [TcpClient.h](file:///e:/Projects/GameEngine/src/Network/TcpClient.h) 传输层重写
  （对外接口与 N3 完全一致，游戏层零感知）：
  - sf::TcpSocket → NET_StreamSocket；**SocketHolder 共享持有**：拷贝共享连接
    （对齐旧 shared_ptr 语义）、disconnect 幂等、析构自动 NET_DestroyStreamSocket
  - 连接：NET_ResolveHostname → NET_WaitUntilResolved → NET_CreateClient →
    NET_WaitUntilConnected（timeoutSeconds 毫秒化，0=无限等）
  - 发送：NET_WriteToStreamSocket 入内部异步队列（false=队列满 → 暂存 m_sendBuf
    下帧冲刷）；接收：NET_ReadFromStreamSocket >0/0/-1 → 数据/NotReady/Disconnected
  - 自研组帧不动：uint32 大端长度前缀，线格式与 SFML 时代逐字节兼容
- **API 签名全部核本地 sdl_net-src 头文件，纠正计划 4 处假设**：
  无 NET_WaitForResolvedAddress（实为 NET_WaitUntilResolved）；NET_AcceptClient 是
  bool+出参式（非返回 socket）；销毁函数叫 NET_DestroyStreamSocket（非 Close）；
  NET_Status 枚举 -1/0/1
- [NetworkManager.h](file:///e:/Projects/GameEngine/src/Network/NetworkManager.h)/.cpp：
  sf::TcpListener → NET_Server*（nullptr=监听全部接口）；NET_Init/NET_Quit 生命周期
  （startServer/connectToServer 初始化，析构配对清理；服务端路径补 SDL_Init(0)）；
  删 setBlocking 全部调用（SDL_net 全异步无此概念）；新连接日志去端口
  （SDL_net 无远端端口查询 API）
- [CMakeLists.txt](file:///e:/Projects/GameEngine/CMakeLists.txt)：客户端链
  SDL3_net::SDL3_net，服务端链 SDL3_net::SDL3_net（传递链 SDL3 核心）；
  **SFML 链接块三套分支整体删除**
- **真实 socket 回环测试 10/10 全过**（测后即删）：init/listen/connect/accept/
  远端地址校验/聚合帧双向解码/粘包拆分/断连检测——自研组帧在 SDL_net 传输上等价性实测
- 待用户验证：双版本编译 + 双客户端联机回归。

### N5 完成（2026-08-23）——SDL_net 迁移收官，SFML 退出项目

- [CMakeLists.txt](file:///e:/Projects/GameEngine/CMakeLists.txt)：删除 SFML 全部痕迹——
  `BUILD_SFML_FROM_SOURCE` 选项、FetchContent SFML 块、find_package 兜底、
  bundled 路径分支、`SFML_STATIC` 定义、include 路径 `${SFML_ROOT}/include`
- **lib/SFML-2.6.1-gcc、lib/SFML-2.6.1-linux 目录删除**（未被 git 跟踪，文件系统删除）；
  lib/ 仅剩 nlohmann
- src/ 过时注释修正 3 处（Types.h"Network 继续用 sf"已失实、KeyCodes.h"两个后端"
  已失实、GameEngine.cpp"仍可取 sf::RenderWindow*"已失实）；其余 50 处
  "SDL3 迁移 xx：原 sf::xxx"历史注释保留（迁移记录，有价值）
- 终态验收（grep 实测）：CMake SFML 代码零引用（仅 2 处历史对照注释）、
  src 零代码引用、lib 目录不存在
- 待用户验证：`cmake -S . -B build` 重新配置（SFML 移除后 include 路径变化）+
  双版本编译 + 联机回归 + 全场景渲染回归（确认删 lib 无隐性影响）。

### N4 修复（2026-08-23 晚）：服务端假连接 spam + 客户端握手失败

用户实测反馈两个 N4 引入的 bug，根因均已核 sdl_net-src 源码定位：

1. **服务端无连接时每帧刷 "New TCP connection established with ?"**
   - 根因：本版 `NET_AcceptClient` **无连接时也返回 true**（SDL_net.c L1863
     `return true; // nothing new.`，true=无错误而非有客户端），旧代码拿返回值
     当"有连接"判断 → 每帧接受一个 NULL socket → 地址 `?`、垃圾连接堆积
   - 修复：acceptFrom 改为**验出参非空**（`*client_stream == NULL` → false）
2. **客户端连不上（token 验证必失败）**
   - 根因：SFML 时代握手期 socket 阻塞，`receive()` 天然等待服务端回应；
     SDL_net 全异步，回应不可能 0ms 到达 → 单次 receive 立即 NotReady → 放弃
   - 修复：connectToServer 握手改**轮询等待**（5ms 间隔，上限 CONFIG.network.timeout，
     NotReady 之外的状态立即退出循环）
3. 顺带核实读写语义无误：NET_ReadFromStreamSocket WouldBlock→0/EOF→-1（映射正确）；
   NET_WriteToStreamSocket 内核满时余量进内部 pending 队列且返回 true（false=真错误），
   头部注释同步修正
4. **针对性回归测试 13/13 全过**（测后即删）：无连接 accept 必须 false（原坑1）、
   真实时序完整握手——token 先发、服务端后 accept、双端轮询收发（原坑2）。
   上轮回环测试没抓住这两 bug 的原因：测试时序"太顺"（accept 前客户端已就绪、
   收发前先 sleep），真实时序的竞态被掩盖
- 待用户验证：双端编译 + 联机回归（开服无 spam；两客户端可连入同步）。
