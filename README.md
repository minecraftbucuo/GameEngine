# GameEngine

> 🎮 [点击观看 B 站演示视频](https://www.bilibili.com/video/BV1MBAezKEai/?spm_id_from=333.1387.homepage.video_card.click&vd_source=3a4ba49672dbd243312160a0bd307621)
>
> 📚 [项目详细文档](https://minecraftbucuo.github.io/website/%E6%8A%80%E6%9C%AF%E7%9B%B8%E5%85%B3/%E4%B8%8D%E7%9F%A5%E5%8F%AB%E4%BB%80%E4%B9%88/GameEngine%E9%A1%B9%E7%9B%AE%E6%96%87%E6%A1%A3.html)

## 项目简介

GameEngine 是一个基于 C++20 和 SDL3 的个人学习型游戏框架原型。项目参考 Unity 的组件化思想组织游戏对象，包含场景管理、资源管理、碰撞检测、事件总线、双物理引擎（手写物理 + Box2D）、相机、动画帧管理、简单 3D 渲染示例和 TCP 联机同步示例。

它还不是完整意义上的通用游戏框架，更适合作为学习项目，用来理解一个小型游戏框架如何把对象、组件、场景、资源、输入、碰撞、物理和网络同步组织在一起。

## 功能特性

- 组件化 GameObject 系统：通过 `GameObject` + `Component` 组合对象行为。
- 场景管理：支持菜单场景、2D Demo 场景、3D Demo 场景、Box2D 物理测试场景和 SuperMario 场景切换。
- 碰撞系统：支持矩形、圆形碰撞体和碰撞处理器。
- 双物理引擎：手写物理（重力/移动/碰撞组件）与 Box2D 2.4 刚体物理按场景共存，互不干扰。
- Box2D 物理引擎：质量、冲量、摩擦、弹性、旋转、堆叠，固定时间步 60Hz，碰撞事件桥接到统一事件总线，附调试可视化（形状轮廓/质心轴/速度箭头）。
- 事件总线：用于组件和系统之间解耦通信。
- 基础物理与控制：移动、重力、跳跃、相机跟随等组件。
- 资源管理：统一加载纹理、音效、音乐、字体、动画帧和 OBJ 模型（句柄式资源 API）。
- SuperMario 示例：包含地图、角色动画、音效、火球、血条、死亡与重生逻辑。
- TCP 联机示例：支持服务端、客户端连接、对象生成、输入同步和状态同步（自研 Packet 序列化 + SDL_net 传输）。
- 服务端构建模式：通过 `SERVER_BUILD` 编译宏构建无渲染循环的服务端程序。

## 技术栈

- 语言：C++20
- 窗口 / 渲染 / 输入 / 音频 / 网络：SDL3（本体 3.4.x + SDL_image / SDL_ttf / SDL_mixer / SDL_net，FetchContent 自动拉取）
- 物理引擎：Box2D 2.4.1
- 配置解析：nlohmann/json
- 构建系统：CMake 3.20+

## 目录结构

```text
.
├── CMakeLists.txt
├── docs/                    # 迁移计划等工程文档
├── lib/
│   └── nlohmann/            # json 头文件库
└── src/
    ├── Asset/               # 配置、贴图、音效、字体、OBJ 模型和 Mario 示例资源
    ├── Components/          # 组件、碰撞体和碰撞处理器
    ├── Core/                # 引擎自有基础类型（Vec2/Rect/Color/Time）、事件与键码定义
    ├── GameObjects/         # 玩家、Mario、砖块、按钮、3D 对象等
    ├── Manager/             # 资源、配置、场景、帧动画、日志管理
    ├── Network/             # TCP 客户端、自研 Packet 序列化、协议和同步逻辑
    ├── Physics/             # Box2D 封装：世界、刚体组件、碰撞监听、调试绘制
    ├── Render/              # 渲染抽象（Renderer.h 契约 + SDL3 实现）
    ├── Scene/               # 菜单、2D、3D、物理测试、SuperMario 场景
    ├── State/               # Mario 状态机
    ├── GameEngine.h         # 引擎初始化与主循环
    └── main.cpp             # 程序入口
```

## 构建与运行

### 依赖要求

- 支持 C++20 的编译器（MSVC / MinGW GCC / Clang）
- CMake 3.20 或更高版本
- git 和网络连接（首次配置时 FetchContent 自动下载 SDL3 全家与 Box2D 并静态编译，之后离线可用）

常用 CMake 开关（均可通过 `-D` 覆盖）：

| 开关 | 默认 | 说明 |
|------|------|------|
| `BUILD_FOR_SERVER` | `OFF` | `ON` 构建无渲染/音频的服务端程序 |
| `BUILD_STATIC` | `ON` | 依赖全部静态链入 exe（单文件发布）；`OFF` 为动态链接 |

### Windows / Linux

```bash
git clone https://github.com/minecraftbucuo/GameEngine.git
cd GameEngine

cmake -S . -B build
cmake --build build
```

运行客户端 / 图形程序：

```bash
./build/bin/GameEngine
```

Windows 下通常是：

```powershell
.\build\bin\GameEngine.exe
```

构建时 CMake 会把 `src/Asset/` 同步到可执行文件目录下，因此程序运行时会从 `build/bin/Asset/` 读取资源和配置。

## 服务端模式

项目支持编译 SuperMario 的服务端程序，推荐使用独立构建目录（与客户端互不干扰）：

```bash
cmake -S . -B build-server -DBUILD_FOR_SERVER=ON
cmake --build build-server
./build-server/server/GameEngineServer
```

服务端模式会定义 `SERVER_BUILD`，不创建渲染窗口，只加载 `SuperMarioScene` 并按配置帧率更新网络和场景逻辑。

## 运行示例

启动图形程序后会进入菜单场景，可选择：

- 超级玛丽 Client：连接 `config.json` 中配置的服务器地址。
- 超级玛丽 Server：在当前进程内启动服务器。
- 3D 渲染：打开一个简单的 3D 对象渲染与控制示例。
- Demo：打开基础 2D 物理、碰撞、相机和对象示例。
- 设置：修改窗口宽高与帧率上限。
- 物理测试：Box2D 刚体物理示例（下落、堆叠、斜面、弹性球、可控玩家）。

SuperMario 默认网络配置位于 `src/Asset/config.json`：

```json
{
  "network": {
    "serverIp": "127.0.0.1",
    "port": 6666,
    "tickRate": 128,
    "timeout": 5.0
  }
}
```

## 操作说明

### SuperMario 场景

- `A` / `D`：左右移动
- `W` 或 `空格`：跳跃
- `J`：发射火球
- `R`：死亡后重生
- `Esc`：返回菜单

### 2D Demo 场景

- 鼠标点击：生成圆形对象
- `A` / `D`：控制玩家左右移动
- `W`：跳跃
- `Esc`：返回菜单

### Box2D 物理测试场景

- `A` / `D`：控制玩家方块左右移动
- `空格`：跳跃
- 鼠标左键：在点击处生成弹性球
- `R`：重置场景
- `Esc`：返回菜单
- `config.json` 的 `game.debug` 为 true 时，叠加显示 Box2D 调试图（形状轮廓、质心/旋转轴、速度箭头）

### 相机 / 3D 示例

- 方向键：移动相机
- 鼠标滚轮：缩放
- `Esc`：返回菜单

具体按键行为以对应场景和对象的 `handleEvent()` 实现为准。

## 配置文件

主配置文件为 `src/Asset/config.json`，运行时会被复制到可执行文件目录下的 `Asset/config.json`。常用配置包括：

- `window`：窗口宽高、标题和帧率上限。
- `assets`：纹理、音效、音乐、字体、动画帧、OBJ 模型路径。
- `network`：服务器 IP、端口、tickRate 和超时时间。
- `game`：重力、玩家速度、跳跃力度、火球速度、方块尺寸、射击冷却、物理步长/迭代次数（`physicsFixedStep` / `physicsVelocityIterations` / `physicsPositionIterations`）等参数。

如果程序找不到配置文件，会使用代码中的默认配置。

## 核心模块

### GameObject 与 Component

`GameObject` 是所有游戏对象的基类，`Component` 负责把移动、重力、控制、碰撞、相机、血条等能力挂到对象上。

相关文件：

- `src/GameObjects/GameObject.h`
- `src/Components/Component.h`
- `src/Components/MoveComponent.h`
- `src/Components/GravityComponent.h`
- `src/Components/Controller.h`
- `src/Components/MarioController.h`

### 场景系统

`SceneManager` 管理场景注册、加载、更新和渲染。图形模式下默认先进入 `MenuScene`，服务端模式下直接进入 `SuperMarioScene`。

相关文件：

- `src/Manager/SceneManager.h`
- `src/Scene/Scene.h`
- `src/Scene/MenuScene.h`
- `src/Scene/GameScene.h`
- `src/Scene/GameScene3D.h`
- `src/Scene/SuperMarioScene.h`

### 碰撞系统

碰撞体分为矩形和圆形，碰撞检测由 `CollisionSystem` 统一驱动，对象添加到场景时会被注册到碰撞系统。

相关文件：

- `src/CollisionSystem.h`
- `src/Components/Collisions/Collision.h`
- `src/Components/Collisions/BoxCollision.h`
- `src/Components/Collisions/CircleCollision.h`
- `src/Components/CollisionHandles/CollisionHandle.h`

### 物理系统（双引擎可选）

引擎同时保留两套物理，按场景选择，互不干扰：

- **手写物理**：`GravityComponent` / `MoveComponent` + `CollisionSystem`，速度积分 + AABB 检测，现有玩法（SuperMario 等）使用。
- **Box2D 2.4**：真实刚体物理（质量/冲量/摩擦/弹性/旋转/堆叠），固定时间步 60Hz，像素↔米换算内置，碰撞事件经 `b2ContactListener` 桥接为统一的 `"onCollision"+tag` 事件总线事件，玩法代码无感切换。

选择方式：场景级——构造 Scene 时设 `usePhysics = true` 即启用 Box2D（默认 false 用手写物理）。

注意：同一场景不混用两套物理（避免位置双重积分），同一对象不双挂物理组件。

相关文件：

- `src/Physics/PhysicsTypes.h` — PPM 坐标换算、BodyType、碰撞分组
- `src/Physics/PhysicsWorld.h` — b2World 封装、固定步累加器、调试绘制入口
- `src/Physics/PhysicsBodyComponent.h` — 刚体组件（形状/密度/摩擦/弹性/冲量 API）
- `src/Physics/PhysicsContactListener.h` — 碰撞事件桥接 EventBus
- `src/Physics/PhysicsDebugDraw.h` — 调试绘制（形状/质心轴/速度箭头，走引擎绘制命令）
- `src/Scene/PhysicsTestScene.h` — 物理测试场景（新场景接入参照）
- `docs/box2d-integration-plan.md` — 接入设计与决策记录

### 资源与配置

资源由 `AssetManager` 统一加载，配置由 `ConfigManager` 读取。引擎启动时会加载 SuperMario 场景需要的纹理、声音和动画帧。

相关文件：

- `src/Manager/AssetManager.h`
- `src/Manager/ConfigManager.h`
- `src/Manager/FrameManager.h`
- `src/Animation.h`

### 渲染抽象

引擎通过 `eng::Renderer` 接口隔离第三方渲染库：场景与组件只依赖绘制命令（`drawTexture` / `drawRect` / `drawText` 等）与自研类型（`Core/Types.h`），SDL3 只出现在 `RendererSDL3.cpp` / `AssetManager.cpp` 实现内部——替换渲染后端不需要改动游戏层代码。

相关文件：

- `src/Render/Renderer.h` — 渲染契约（窗口/事件泵/绘制命令/相机/文字测量）
- `src/Render/RendererSDL3.cpp` — SDL3 实现
- `src/Core/Types.h` — Vec2/Vec3/Rect/Color/Time 等引擎自有类型
- `src/Core/Event.h` / `src/Core/KeyCodes.h` — 统一事件与键码定义

### 网络同步

网络层基于 SDL_net 的 TCP 流式套接字，序列化使用自研 `eng::Packet`（运算符风格与线格式兼容经典 sf::Packet），实现了客户端验证、玩家生成、输入上传、对象状态同步、断线删除和重生逻辑。

相关文件：

- `src/Network/NetworkManager.h`
- `src/Network/NetworkProtocol.h`
- `src/Network/TcpClient.h` — 连接/收发/组帧封装
- `src/Network/Packet.h` — 自研序列化容器
- `src/Network/ISerializable.h`

## 如何添加一个新场景

1. 新建一个类继承 `Scene`。
2. 在 `init()` 中创建并添加游戏对象。
3. 如需碰撞检测，创建 `CollisionSystem` 并在 `addObject()` 中注册带碰撞组件的对象。
4. 如需 Box2D 物理，构造时设 `usePhysics = true`，对象挂 `PhysicsBodyComponent`（参照 `PhysicsTestScene`）。
5. 在 `GameEngine::init()` 中通过 `scene_manager->addScene<YourScene>(&renderer)` 注册场景。
6. 通过 `SceneManager::loadScene("YourSceneName")` 切换场景。

## 如何添加一个新对象

1. 新建一个类继承 `GameObject`。
2. 在构造函数中设置位置、尺寸、贴图或模型。
3. 用 `addComponent<T>()` 添加移动、碰撞、重力、控制等组件（Box2D 场景用 `PhysicsBodyComponent` 替代移动/重力组件）。
4. 如需网络同步，实现或复用 `ISerializable` 的序列化逻辑。
5. 在场景中调用 `addObject()` 或 `addObjectWithNetwork()` 添加对象。

## 设计模式

- 组件模式：用组件组合对象行为。
- 单例模式：用于配置、资源、日志、事件总线等全局管理器。
- 观察者模式：事件总线负责发布和订阅事件。
- 状态模式：Mario 的站立、奔跑、跳跃、死亡等状态由状态机管理。

## 说明

该项目主要用于学习和实验。代码中仍保留了一些 Demo、调试逻辑和可继续整理的工程化细节，适合在阅读源码时配合运行示例逐步理解。

## License

本项目使用 [MIT License](LICENSE)。
