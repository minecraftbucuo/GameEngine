# 浏览器版移植计划（WASM / Emscripten）

> 分支：`wasm-emscripten`（自 master 切出）
> 状态：计划阶段，未动代码
> 参照文档风格：[box2d-integration-plan.md](box2d-integration-plan.md)

---

## 一、目标

用 Emscripten 把客户端编译为 WebAssembly，在浏览器中直接运行：

- **第一阶段（本计划主体）**：离线玩法全量可用——菜单、2D Demo、3D 渲染示例、Box2D 物理测试、SuperMario 单机模式。
- **明确排除**：TCP 联机（浏览器不允许裸 socket，见技术决策 7）。网页版先隐藏联机入口，Web 联机作为后续可选阶段另立计划。
- 桌面版行为零变化：所有改动以 `__EMSCRIPTEN__` / `EMSCRIPTEN` 条件隔离，桌面构建路径不受影响。

---

## 二、现状分析（平台耦合点盘点）

### 有利条件（为什么这个项目适合上浏览器）

| # | 条件 | 依据 |
|---|------|------|
| 1 | SDL3 官方支持 Emscripten 后端（窗口→canvas、音频→WebAudio、事件/输入原生桥接） | [RendererSDL3.cpp](../src/Render/RendererSDL3.cpp) 全部走 `SDL_Renderer` 2D API |
| 2 | 3D 场景是**软件光栅化**（CPU 投影后走 2D renderer 画线/三角形），无 OpenGL/GPU API 依赖 | [NewModel3D.cpp](../src/GameObjects/NewModel3D.cpp) 的 `drawFaces`，全库 grep 无任何 `gl*` 调用 |
| 3 | 无线程依赖：全库仅 Logger 用 `std::mutex`（单线程 WASM 下合法） | [Logger.h](../src/Manager/Logger.h) |
| 4 | 计时全部基于 dt 累加（`std::chrono` + `eng::Time`），无阻塞 sleep | [Timer.h](../src/Timer.h)、[GameEngine.cpp](../src/GameEngine.cpp#L79-L102) |
| 5 | Box2D 2.4 与 nlohmann/json 均为纯 C++，无平台依赖 | CMakeLists.txt FetchContent |
| 6 | 已有 `SERVER_BUILD` 宏条件编译先例，WEB 构建可复用同一套模式 | [CMakeLists.txt](../CMakeLists.txt#L14-L22)、GameEngine.h 的 `#ifndef SERVER_BUILD` |
| 7 | 联机是菜单里的**可选入口**（按钮触发），离线玩法不触碰网络层 | [MenuScene.cpp](../src/Scene/MenuScene.cpp#L44-L53) |

### 平台耦合点清单（必须处理的）

| # | 耦合点 | 位置 | 浏览器影响 | 处理策略 |
|---|--------|------|-----------|---------|
| A1 | **阻塞式主循环** `while (isWindowOpen())` | [GameEngine.cpp](../src/GameEngine.cpp#L79-L102) | 浏览器主线程不允许死循环，页面假死 | **必须改造**：拆帧函数 + `emscripten_set_main_loop_arg`（决策 2） |
| A2 | **限帧睡眠** `SDL_DelayNS`（present 内） | [RendererSDL3.cpp](../src/Render/RendererSDL3.cpp#L597-L614) | 主线程忙等会卡死页面；rAF 本身节流 | WEB 下跳过限帧，交给 requestAnimationFrame（决策 2） |
| A3 | **exe 目录探测** Win32 API / `/proc/self/exe` + `current_path()` | [GameEngine.cpp](../src/GameEngine.cpp#L22-L37) | 两者在 WASM 均不存在 | `__EMSCRIPTEN__` 分支返回 `"/"`（决策 3） |
| A4 | **目录扫描加载资源** `recursive_directory_iterator` 扫 PNG/OGG | [AssetManager.cpp](../src/Manager/AssetManager.cpp#L51-L78) | MEMFS 虚拟盘对 `std::filesystem` 支持历史上不完整 | 先验证；失败备选 `SDL_GlobDirectory`（决策 4） |
| A5 | 文件读取：config.json / 地图 / 动画帧 / OBJ / 字体（ifstream） | ConfigManager、FrameManager、ModelManager | 需文件先存在于虚拟盘 | `--preload-file` 打包进 MEMFS 后 ifstream 可用（决策 3） |
| A6 | **设置保存** ofstream 写 config.json | SettingsScene / ConfigManager::save | MEMFS 可写但**刷新即失**（内存文件系统） | 低优先级：localStorage 或提示语（决策 8） |
| A7 | **SDL_net TCP** 全套 | NetworkManager / TcpClient | 浏览器无裸 socket，SDL_net 在 WEB 构建不可用 | WEB 构建不编不链，菜单裁剪入口（决策 7） |
| A8 | **C++ 异常** try/catch（json 解析、filesystem、Packet） | 多处 | Emscripten 默认禁异常捕获，抛出即 abort | 链接 `-fexceptions`（决策 5） |
| A9 | 默认 fps=165、窗口 1200×960 | config.json 默认值 | rAF 通常 60Hz；canvas 尺寸应自适应页面 | fpsLimit WEB 强制 0；index.html 自适应样式（决策 2/9） |

### 无需处理（已确认兼容）

- 输入轮询 `SDL_PollEvent` / `SDL_GetKeyboardState`：SDL3 Emscripten 后端自动桥接浏览器事件。
- 日志 `log.txt` 写入 MEMFS：合法，随页面丢弃，可接受。
- OBJ 模型解析、动画 FrameManager、碰撞系统、Box2D 世界：纯 CPU 计算。

---

## 三、技术决策

### 1. 工具链：emsdk + emcmake

- 安装 [emsdk](https://emscripten.org/docs/getting_started/downloads.html) 最新稳定版（≥ 3.1.x）。
- 构建命令：`emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web`。
- SDL 全家继续走 FetchContent：SDL3 / SDL_image / SDL_ttf / SDL_mixer 官方支持 Emscripten 配置（vendored 后端 libpng/freetype/vorbis 为纯源码，可交叉编译）。**SDL_net 不参与 WEB 构建**。
- 预期首次配置+编译耗时较长（SDL 全家 + wasm 编译），属正常现象。

### 2. 主循环模型：拆帧函数 + 回调驱动

```cpp
// GameEngine 私有：执行一帧迭代，返回 false 表示退出
bool GameEngine::frameStep();

void GameEngine::start() {
#ifdef __EMSCRIPTEN__
    renderer.setFramerateLimit(0);   // rAF 天然节流，禁用内部限帧
    emscripten_set_main_loop_arg(
        [](void* arg) { static_cast<GameEngine*>(arg)->frameStep(); },
        this, /*fps=0 → requestAnimationFrame*/ 0, /*simulate_infinite_loop*/ 1);
#else
    while (frameStep()) {}
#endif
}
```

- `frameStep()` 内容 = 现 while 循环体：pollEvent → update → clear → render → present。
- 关窗时：桌面版返回 false；WEB 版调 `emscripten_cancel_main_loop()`。
- `Renderer::present()` 中 `SDL_DelayNS` 限帧段加 `#ifndef __EMSCRIPTEN__`；`setFramerateLimit(0)` 即可完全绕过。
- 附带修复（两平台共同受益）：`frameStep` 里对 deltaTime 做**上限钳制**（如 ≤ 50ms）。浏览器切后台再切回会产生巨大 dt，导致 Mario 瞬移穿墙——现有代码无钳制，属于潜在 bug。

### 3. 资源分发与路径：--preload-file 进 MEMFS

- 链接参数：`--preload-file src/Asset@/Asset -sFORCE_FILESYSTEM=1`。
- 打包到根路径 `/Asset`，配合 A3 的 `current_path("/")`，现有相对路径 `"./Asset/config.json"` 等全部无需改写即生效。
- `.data` 包由浏览器缓存；自定义 index.html 显示加载进度条（决策 9）。
- `getExeDir()` 加分支：

```cpp
#ifdef __EMSCRIPTEN__
    return std::filesystem::path("/");   // MEMFS 根
#elif defined(_WIN32)
    ...现有代码...
```

### 4. 目录枚举：验证优先，备选 SDL_GlobDirectory

- 新版 Emscripten 对 MEMFS 支持 `opendir/readdir/stat`，`std::filesystem::recursive_directory_iterator` 大概率可用。
- 计划：阶段二第一个 Step 就是**专项验证**。若失败，把 [AssetManager.cpp](../src/Manager/AssetManager.cpp) 两处扫描替换为 `SDL_GlobDirectory`（SDL3 统一 API，三平台行为一致，顺带消除 `std::filesystem` 异常面）。
- 不预先切换：避免为"可能不存在的问题"引入改动。

### 5. 异常支持：-fexceptions

ConfigManager/json、filesystem 迭代、Packet 反序列化都有 try/catch。不开异常捕获，任何抛出都会直接 abort 且无日志。链接选项统一加 `-fexceptions`（WASM EH 机制，体积代价可接受）。

### 6. 内存与并发

- `-sALLOW_MEMORY_GROWTH=1`：资源全量解码进内存（音效预解码、纹理），初始内存不够时自动扩容。
- **不开 pthread**（`-pthread`）：避免部署端强制 COOP/COEP 响应头的负担，GitHub Pages 等静态托管开箱即用。本项目无线程需求，代价为零。

### 7. 网络：第一阶段整体禁用

- WEB 构建分两步走：**Step 1 先保留 SDL_net 参与编译**（emscripten 提供全套 socket 头文件，能编过；运行期无裸 socket 自然不可用）；**Step 5 再加条件编译护栏后从构建移除**——照 `BUILD_FOR_SERVER` 先例，`NetworkManager::startServer/connectToServer` 在 `__EMSCRIPTEN__` 下直接返回 false 并 LOG_WARN，菜单裁剪两个联网按钮。
- [MenuScene.cpp](../src/Scene/MenuScene.cpp#L44-L53)：`__EMSCRIPTEN__` 下不注册两个联网按钮（后续按钮序号顺延或置灰显示"网页版暂不支持"）。
- 后续可选方向（届时另立文档）：
  - A. 服务端架 websockify 之类的 TCP↔WebSocket 桥 + Emscripten `-sPROXY_POSIX_SOCKETS`，SDL_net 代码几乎不动；
  - B. `TcpClient` 接口化，新增 WebSocket 实现（`-lwebsocket.js`），协议层 Packet 复用。

### 8. 设置保存：localStorage 方案（低优先级）

MEMFS 写入不持久。方案：`ConfigManager::save` 在 `__EMSCRIPTEN__` 下经 `EM_JS` 把 JSON 写入 localStorage，`load` 时优先读 localStorage、回退 preload 文件。若嫌复杂可先接受"设置仅本次会话有效"，在设置场景加一行提示文字。**放在打磨阶段，不阻塞主线。**

### 9. 页面壳与发布

- 自定义 shell HTML（Emscripten `--shell-file`）：居中 canvas、CSS 自适应缩放（保持宽高比）、`.data` 加载进度条、点击 canvas 提示聚焦。
- 发布目标：GitHub Pages（`application/wasm` MIME 已支持，无 COOP/COEP 要求——见决策 6）。提供 `scripts/build_web.ps1` 一键脚本：emcmake configure → build → 产物归集到 `build-web/web/`。

---

## 四、分阶段实施计划

### 原子性原则（每一步必须满足）

> **每一步 = 一次 git commit = 一个原子操作**，必须同时满足：

1. **可编译**：该步完成后桌面版 `cmake --build` 通过（WEB 版视工具链是否就绪）。
2. **可运行**：桌面程序启动后所有现有功能行为不变。
3. **可回滚**：`git revert <commit>` 后回到上一步状态，无残留依赖。
4. **单一职责**：一步只做一件事。
5. **平台隔离**：所有 WEB 特殊化都在 `#ifdef __EMSCRIPTEN__` / `if(EMSCRIPTEN)` 内，桌面代码路径不动。

完成后将 `[ ]` 改为 `[x]`。

---

### 阶段〇：环境准备（本地一次性）

#### Step 0 — 安装 emsdk 并冒烟测试 ✅（2026-08-24）
- [x] emsdk 已就绪：`E:\Project\GitHub\emsdk`（build_web.ps1 默认指向该路径，可用 `-EmsdkEnv` 参数覆盖）
- [x] `emcc` / `emcmake` 可用性由 Step 1 首次构建冒烟验证
- **验证**：`scripts/build_web.ps1` 全流程跑通即视为通过

---

### 阶段一：最小闭环（浏览器里跑出菜单窗口）

#### Step 1 — CMake 平台分支与 WEB 构建产出
> **2026-08-24 调整**：SDL_net 本步**暂保留参与编译**——彻底移除需要先给
> Network / TcpClient / SuperMarioScene / MarioController 加条件编译护栏（即 Step 5 的活），
> 提前拆会破坏原子性。浏览器无裸 socket，运行期联网本就不可用，编译期能过即可。
- [x] `CMakeLists.txt` 增加 `if(EMSCRIPTEN)` 分支（代码已就位，待构建验证）：
  - [x] 目标产物输出到 `build-web/web/`，`CMAKE_EXECUTABLE_SUFFIX=.html` 直接产出入口页（html + js + wasm + data 四件套）
  - [x] 链接选项：`-sALLOW_MEMORY_GROWTH=1`、`-sFORCE_FILESYSTEM=1`、`-fexceptions`、`--preload-file src/Asset@/Asset`、`-sMAX_WEBGL_VERSION=2`（SDL3 GLES2 后端需 WebGL2）
- [x] 新增 `scripts/build_web.ps1`（激活 emsdk → emcmake 配置 → 编译）
- [ ] **验证（由用户执行）**：脚本全流程通过、产物四件套齐全；此时打开页面**预期卡死**属正常现象——阻塞式主循环要到 Step 3 才改造
- **改动文件**：`CMakeLists.txt`、新增 `scripts/build_web.ps1`
- **原子性保证**：非 EMSCRIPTEN 分支零改动，桌面/服务端构建不受影响

#### Step 2 — 入口路径适配
- [ ] [getExeDir()](../src/GameEngine.cpp#L22-L30) 增加 `__EMSCRIPTEN__` 分支返回 `"/"`
- [ ] `Logger::setLogFile("log.txt")` 保持不变（写 MEMFS 合法）
- **验证**：WEB 构建 configure/build 通过；桌面版行为不变
- **改动文件**：`src/GameEngine.cpp`
- **原子性保证**：单一 ifdef 分支，不影响既有两平台

#### Step 3 — 主循环回调化（核心改造）
- [ ] [GameEngine](../src/GameEngine.h) 增加私有 `bool frameStep()`，提取现 while 循环体
- [ ] 桌面版 `start()` 改为 `while (frameStep()) {}`
- [ ] WEB 版用 `emscripten_set_main_loop_arg(..., fps=0, simulate_infinite_loop=1)`
- [ ] WindowClose 分支：WEB 版追加 `emscripten_cancel_main_loop()`
- [ ] deltaTime 上限钳制（≤50ms，两平台共同生效，修切后台瞬移隐患）
- [ ] [Renderer::present](../src/Render/RendererSDL3.cpp#L597-L614) 限帧段加 `#ifndef __EMSCRIPTEN__`；`setFramerateLimit` WEB 下强制归零
- **验证**：浏览器打开 `index.html` 出现菜单场景（粒子背景 + 按钮）；点击按钮能进出场景；Esc 行为正常；桌面版回归测试通过
- **改动文件**：`src/GameEngine.h`、`src/GameEngine.cpp`、`src/Render/RendererSDL3.cpp`
- **合并原因**：frameStep 提取与回调注册必须同一 commit 才能编译通过
- **风险预案**：若 SDL_DelayNS 在 Emscripten 报未实现/忙等，靠 setFramerateLimit(0) 已绕开；若 pollEvent 在回调外轮询异常，检查 SDL 事件循环与 rAF 的配合（SDL3 官方 emscripten 后端已适配）

✅ **到此为止最小闭环达成：浏览器可玩离线内容**

---

### 阶段二：资源与音频专项验证

#### Step 4 — 目录扫描兼容性验证
- [ ] WEB 版启动观察 `loadTexture` / `loadSoundBuffer` 是否枚举到文件
- [ ] 若失败：[AssetManager.cpp](../src/Manager/AssetManager.cpp) 两处扫描替换为 `SDL_GlobDirectory`（布局模式递归）
- **验证**：Demo / 物理测试 / SuperMario 场景贴图齐全无 "Path does not exist"
- **改动文件**：视结果而定（可能零改动）
- **原子性保证**：SDL_GlobDirectory 替换为等价枚举语义，桌面版同样编译验证

#### Step 5 — 联网入口裁剪
- [ ] [MenuScene::initScene](../src/Scene/MenuScene.cpp#L29) `__EMSCRIPTEN__` 下跳过两个联网按钮注册（其余序号顺延）
- [ ] [NetworkManager::startServer/connectToServer](../src/Network/NetworkManager.cpp) `__EMSCRIPTEN__` 下返回 false + LOG_WARN（双保险）
- **验证**：菜单无联网按钮；其余四个入口功能正常
- **改动文件**：`src/Scene/MenuScene.cpp`、`src/Network/NetworkManager.cpp`
- **原子性保证**：纯裁剪，不新增行为；桌面版按钮不受影响

#### Step 6 — 音频手势解锁验证
- [ ] 验证 SDL3 Emscripten 音频后端的自动恢复：首键/首击后 BGM 与音效正常
- [ ] 若 MIX 设备在手势前创建导致静默：把 `ensureMixer` 首次创建延迟到首个输入事件之后（或监听 SDL_EVENT_AUDIO_DEVICE_ADDED）
- **验证**：Mario 场景跳跃/金币音效、BGM 循环正常；刷新页面后依然正常
- **改动文件**：预期 `src/Manager/AssetManager.cpp`（如需延迟初始化）
- **风险说明**：浏览器 autoplay policy 是硬约束，SDL3 后端通常已处理 resume，此步以实测为准

---

### 阶段三：体验打磨

#### Step 7 — index.html 页面壳
- [ ] 自定义 shell HTML：深色底、居中 canvas、按容器尺寸等比缩放（保持 1200:960 设计比例）、加载进度条（Module.setStatus）、"点击画面获得焦点"提示
- **验证**：手机竖屏/桌面窄窗口下 canvas 不变形不溢出；加载期间有进度反馈
- **新增文件**：`cmake/shell_web.html`（或 `web/` 目录）

#### Step 8 — 设置保存的持久化取舍
- [ ] 实现 localStorage 方案（决策 8）或在设置场景标注"网页版设置不持久"
- **验证**：改分辨率/帧率 → 刷新 → 行为符合所选方案
- **改动文件**：`src/Manager/ConfigManager.h/.cpp`（或 `src/Scene/SettingsScene.cpp`）

#### Step 9 — 体积与性能基线
- [ ] Release `-O3` 下记录 wasm/js/data 体积基线（写入本文档附录）
- [ ] 评估音乐 ogg 码率是否需要裁剪（当前 Asset 总量决定 .data 包大小）
- [ ] 帧率实测：60Hz rAF 下 SuperMario 场景稳定满帧
- **验证**：基线数据入档；明显超标项列出优化建议（不强行优化）

---

### 阶段四（可选，另行立项）：Web 联机

> 仅登记方向，不在本计划内实施。启动前另立 `docs/websocket-net-plan.md`。

- 方向 A：服务端 websockify TCP 桥 + `-sPROXY_POSIX_SOCKETS`（SDL_net 代码近乎零改动，运维多一个桥进程）
- 方向 B：`TcpClient` 接口化 + Emscripten WebSocket 后端（`-lwebsocket.js`），Packet 协议复用（工程量大，长期干净）
- 方向 C：维持离线（成本最低）

---

## 五、新增 / 改动文件清单

| 路径 | 动作 | 对应 Step |
|------|------|-----------|
| `CMakeLists.txt` | 改：`if(EMSCRIPTEN)` 分支（依赖裁剪/链接选项/preload/产出目录） | 1 |
| `scripts/build_web.ps1` | 新增：一键 WEB 构建脚本 | 1 |
| `src/GameEngine.h/.cpp` | 改：getExeDir 分支、frameStep 提取、回调驱动、dt 钳制 | 2, 3 |
| `src/Render/RendererSDL3.cpp` | 改：present 限帧的平台隔离 | 3 |
| `src/Manager/AssetManager.cpp` | 视情况改：SDL_GlobDirectory 替代、mixer 延迟初始化 | 4, 6 |
| `src/Scene/MenuScene.cpp` | 改：联网按钮裁剪 | 5 |
| `src/Network/NetworkManager.cpp` | 改：连接/监听入口 WEB 直接拒绝 | 5 |
| `cmake/shell_web.html` | 新增：页面壳模板 | 7 |
| `src/Manager/ConfigManager.*` 或 `SettingsScene.*` | 改：持久化方案 | 8 |
| `docs/wasm-web-port-plan.md` | 本文档，随进展勾选 | 全程 |

## 六、风险与注意事项

1. **`std::filesystem` 于 MEMFS 的兼容性不确定**（最大变数）：阶段二 Step 4 专门验证，备选 SDL_GlobDirectory 已就位，不会卡死主线。
2. **音频 autoplay policy**：硬约束，SDL3 后端大概率已处理，Step 6 实测兜底。
3. **dt 尖峰穿墙**：浏览器切标签页常见，Step 3 的 dt 钳制同时修复桌面版同类隐患（属顺手修 bug，注意单独说明）。
4. **异常开关体积代价**：`-fexceptions` 会增大 wasm 体积，换取 try/catch 正常工作；不做函数粒度裁剪，保持简单。
5. **FetchContent × emcmake 构建时长**：SDL 全家交叉编译比桌面慢，CI 化之前接受本地慢速构建；`CCACHE` 不适用于 emcc 缓存语义，不折腾。
6. **不要开启 pthread**：一旦开了，部署端必须带 COOP/COEP 头，GitHub Pages 无法满足；本项目无线程需求，坚决不开。
7. **`simulate_infinite_loop=1` 语义**：`main` 返回控制权给浏览器事件循环，此后不得再访问栈上对象——engine 实例必须静态/堆分配存活（现 main.cpp 栈对象需改为 static 或 new，Step 3 一并处理）。
8. **窗口尺寸语义变化**：`setSize`（设置场景用）在浏览器里是调整 canvas 内部分辨率而非窗口，行为可接受但要实测确认不崩。
9. **资源更新后的 .data 缓存**：发布迭代时浏览器可能缓存旧 .data，发布脚本里对产物文件名附加 hash（或提示强刷）留待 Step 9 评估。
10. **SDL_net 在 emscripten 下编译失败的风险（低）**：emscripten 提供全套 BSD socket 头，预期能编过；若 Step 1 首次构建在 SDL_net 处报错，则把 Step 5 的条件编译护栏提前实施（单独 commit）。

## 七、执行顺序与依赖图

```
Step 0 (emsdk 环境)
  └─ Step 1 (CMake WEB 分支)
       └─ Step 2 (路径适配)
            └─ Step 3 (主循环回调化) ← 最小闭环 ✅ 浏览器出菜单
                 ├─ Step 4 (目录扫描验证)
                 │    └─ Step 6 (音频解锁)
                 ├─ Step 5 (联网入口裁剪)
                 │    └─ Step 7~9 (页面壳 / 持久化 / 基线) ← 第一阶段完成 ✅
                 └─ 阶段四 (Web 联机，另行立项)
```

**关键路径**：0 → 1 → 2 → 3 ✅ 之后各 Step 相互独立，可按兴趣顺序推进。
