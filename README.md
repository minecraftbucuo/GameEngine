# GameEngine

> 🎮 [点击观看 B 站演示视频](https://www.bilibili.com/video/BV1MBAezKEai/?spm_id_from=333.1387.homepage.video_card.click&vd_source=3a4ba49672dbd243312160a0bd307621)

> 📚 [项目详细文档](https://minecraftbucuo.github.io/website/%E6%8A%80%E6%9C%AF%E7%9B%B8%E5%85%B3/%E4%B8%8D%E7%9F%A5%E5%8F%AB%E4%BB%80%E4%B9%88/GameEngine%E9%A1%B9%E7%9B%AE%E6%96%87%E6%A1%A3.html)

## 项目简介

这是一个基于 C++ 和 SFML 开发的一个个人学习用的游戏框架原型，采用类似于 Unity 的组件化架构设计，
实现了引擎中最基础的几个管理模块。它离“真正的游戏引擎”还很远，但适合用来理解游戏对象是如何被组织和管理的。

## 功能特性

- 组件化的游戏对象系统（类似 Unity）
- 碰撞检测系统（支持圆形和矩形碰撞体）
- 事件总线机制用于组件间通信
- 基础物理系统（重力、移动控制）
- 相机跟随功能
- 可扩展的场景管理功能

## 技术栈

- **编程语言**: C++20
- **图形库**: SFML 2.6.1
- **构建系统**: CMake
- **开发环境**: CLion

## 安装与编译

### 依赖要求

- C++20编译器
- CMake 3.20或更高版本
- SFML 2.6.1库

### 编译步骤

1. 克隆项目:
   ```bash
   git clone https://github.com/minecraftbucuo/GameEngine.git
   cd GameEngine
   ```

2. 配置SFML路径:
   默认使用系统 SFML，否则使用项目自带 SFML。

3. 编译项目:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

4. 启动示例：
   ```bash
   cd build/bin
   ./GameEngine
   ```

5. 🐧 Nix 环境特别说明 (非 NixOS 系统)

   如果你的系统是非 NixOS 的 Linux 发行版（如 Arch Linux、Ubuntu 等），并且使用 `nix develop` 或 `Nix Flakes` 提供了构建环境，直接运行编译后的二进制文件可能会遇到 `Failed to create an OpenGL context` 报错或段错误。

   这是由于 Nix 严格的环境隔离机制导致程序无法加载宿主机的显卡驱动。**解决方案是使用 `nixGL` 包装运行**，可选下面两种方案之一：

   1. **直接运行**:
      ```bash
      # 如果你使用 Intel/AMD 等开源 Mesa 驱动:
      nix run --impure github:nix-community/nixGL -- ./GameEngine

      # 如果你使用 NVIDIA 闭源驱动:
      nix run --impure github:nix-community/nixGL#nixGLNvidia -- ./GameEngine
      ```

   2. **全局安装**:

      你可以将 nixGL 安装到本地 profile 以简化命令：
      ```bash
      nix profile install --impure github:nix-community/nixGL
      ```

      安装后，使用命令启动游戏：

      ```bash
      nixGL ./GameEngine
      ```

      *(注：如果你的终端是 Fish shell，请确保 Nix profile 路径已加入环境变量：`fish_add_path ~/.nix-profile/bin`)*

## 运行示例

项目包含基本示例，展示了以下内容：

- SuperMario client
- SuperMario server
- 3D图形渲染
- 初开发时的测试 Demo

## 使用说明

**1. 建场景**
新建场景类继承 `Scene`，在 `init()` 里放初始化代码。

**2. 放对象**
在场景里用 `addObject()` 添加对象（游戏对象需继承 `GameObject`），游戏对象调用 `addComponent()` 挂载组件（组件需继承 `Component`）。

**3. 挂场景**
在 `GameEngine.cpp` 里把做好的场景注册到引擎。

**4. 运行**
编译执行，看到场景启动。

更多细节请参阅源代码。

## 核心组件说明

### GameObject系统

游戏对象采用组件化设计，通过组合不同的组件实现不同的功能：

- [GameObject](): 游戏对象基类
- [Component](): 组件基类
- [BoxGameObject](): 矩形游戏对象

### 物理系统

- [Collision](): 碰撞基类
- [BoxCollision](): 矩形碰撞
- [CircleCollision](): 圆形碰撞
- [CollisionSystem](): 碰撞检测系统

### 事件系统

- [EventBus](): 事件总线
- [CollisionEvent](): 碰撞事件

## 设计模式

- **单例模式**: [SceneContext](): 场景上下文和[EventBus](): 事件总线使用单例模式
- **组件模式**: 游戏对象通过组合不同组件实现功能
- **观察者模式**: 事件系统实现观察者模式

---

*该项目仅供学习和参考用途*