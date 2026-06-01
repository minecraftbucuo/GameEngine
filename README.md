# GameEngine

> 🎮 [点击观看 B 站演示视频](https://www.bilibili.com/video/BV1MBAezKEai/?spm_id_from=333.1387.homepage.video_card.click&vd_source=3a4ba49672dbd243312160a0bd307621)
>
> 📚 [项目详细文档](https://minecraftbucuo.github.io/website/%E6%8A%80%E6%9C%AF%E7%9B%B8%E5%85%B3/%E4%B8%8D%E7%9F%A5%E5%8F%AB%E4%BB%80%E4%B9%88/GameEngine%E9%A1%B9%E7%9B%AE%E6%96%87%E6%A1%A3.html)

## 项目简介

GameEngine 是一个基于 C++20 和 SFML 2.6.1 的个人学习型游戏框架原型。项目参考 Unity 的组件化思想组织游戏对象，包含场景管理、资源管理、碰撞检测、事件总线、基础物理、相机、动画帧管理、简单 3D 渲染示例和 TCP 联机同步示例。

它还不是完整意义上的通用游戏引擎，更适合作为学习项目，用来理解一个小型游戏框架如何把对象、组件、场景、资源、输入、碰撞和网络同步组织在一起。

## 功能特性

- 组件化 GameObject 系统：通过 `GameObject` + `Component` 组合对象行为。
- 场景管理：支持菜单场景、2D Demo 场景、3D Demo 场景和 SuperMario 场景切换。
- 碰撞系统：支持矩形、圆形碰撞体和碰撞处理器。
- 事件总线：用于组件和系统之间解耦通信。
- 基础物理与控制：移动、重力、跳跃、相机跟随等组件。
- 资源管理：统一加载纹理、音效、音乐、字体、动画帧和 OBJ 模型。
- SuperMario 示例：包含地图、角色动画、音效、火球、血条、死亡与重生逻辑。
- TCP 联机示例：支持服务端、客户端连接、对象生成、输入同步和状态同步。
- 服务端构建模式：通过 `SERVER_BUILD` 编译宏构建无渲染循环的服务端程序。

## 技术栈

- 语言：C++20
- 图形 / 音频 / 网络：SFML 2.6.1
- 配置解析：nlohmann/json
- 构建系统：CMake 3.20+
- 可选开发环境：CLion、Nix Flakes

## 目录结构

```text
.
├── CMakeLists.txt
├── flake.nix
├── lib/
│   ├── SFML-2.6.1-gcc/
│   └── nlohmann/
└── src/
    ├── Asset/                 # 配置、贴图、音效、字体、OBJ 模型和 Mario 示例资源
    ├── Components/            # 组件、碰撞体和碰撞处理器
    ├── GameObjects/           # 玩家、Mario、砖块、按钮、3D 对象等
    ├── Manager/               # 资源、配置、场景、帧动画、日志管理
    ├── Network/               # TCP 客户端、协议、序列化和同步逻辑
    ├── Scene/                 # 菜单、2D、3D、SuperMario 场景
    ├── State/                 # Mario 状态机
    ├── GameEngine.h           # 引擎初始化与主循环
    └── main.cpp               # 程序入口
```

## 构建与运行

### 依赖要求

- 支持 C++20 的编译器
- CMake 3.20 或更高版本
- SFML 2.6.1

项目会优先查找系统安装的 SFML；如果没有找到，会尝试使用 `lib/` 目录下随项目提供的 SFML。

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

构建完成后，CMake 会把 `src/Asset/` 复制到可执行文件目录下，因此程序运行时会从 `build/bin/Asset/` 读取资源和配置。

### Nix 开发环境

项目提供了 `flake.nix`：

```bash
nix develop
cmake -S . -B build
cmake --build build
./build/bin/GameEngine
```

如果你在非 NixOS 的 Linux 发行版上使用 `nix develop`，运行时遇到 `Failed to create an OpenGL context` 或段错误，通常是 Nix 环境无法直接加载宿主机显卡驱动。可以用 nixGL 包装运行：

```bash
# Intel / AMD Mesa 驱动
nix run --impure github:nix-community/nixGL -- ./build/bin/GameEngine

# NVIDIA 闭源驱动
nix run --impure github:nix-community/nixGL#nixGLNvidia -- ./build/bin/GameEngine
```

也可以安装到本地 profile：

```bash
nix profile install --impure github:nix-community/nixGL
nixGL ./build/bin/GameEngine
```

## 服务端模式

项目支持编译 SuperMario 的服务端程序。当前 `CMakeLists.txt` 顶部默认设置为：

```cmake
set(BUILD_FOR_SERVER OFF)
```

如需构建服务端，请先把它改为：

```cmake
set(BUILD_FOR_SERVER ON)
```

然后重新配置并构建：

```bash
cmake -S . -B build-server
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
- `W`：跳跃
- `J`：发射火球
- `R`：死亡后重生
- `Esc`：返回菜单

### 2D Demo 场景

- 鼠标点击：生成圆形对象
- `A` / `D`：控制玩家左右移动
- `W`：跳跃
- `Esc`：返回菜单

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
- `game`：重力、玩家速度、跳跃力度、火球速度、方块尺寸、射击冷却等参数。

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

### 资源与配置

资源由 `AssetManager` 统一加载，配置由 `ConfigManager` 读取。引擎启动时会加载 SuperMario 场景需要的纹理、声音和动画帧。

相关文件：

- `src/Manager/AssetManager.h`
- `src/Manager/ConfigManager.h`
- `src/Manager/FrameManager.h`
- `src/Animation.h`

### 网络同步

网络层基于 SFML Network 的 TCP Socket，实现了客户端验证、玩家生成、输入上传、对象状态同步、断线删除和重生逻辑。

相关文件：

- `src/Network/NetworkManager.h`
- `src/Network/NetworkProtocol.h`
- `src/Network/TcpClient.h`
- `src/Network/ISerializable.h`

## 如何添加一个新场景

1. 新建一个类继承 `Scene`。
2. 在 `init()` 中创建并添加游戏对象。
3. 如需碰撞检测，创建 `CollisionSystem` 并在 `addObject()` 中注册带碰撞组件的对象。
4. 在 `GameEngine::init()` 中通过 `scene_manager->addScene<YourScene>(window)` 注册场景。
5. 通过 `SceneManager::loadScene("YourSceneName")` 切换场景。

## 如何添加一个新对象

1. 新建一个类继承 `GameObject`。
2. 在构造函数中设置位置、尺寸、贴图或模型。
3. 用 `addComponent<T>()` 添加移动、碰撞、重力、控制等组件。
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
