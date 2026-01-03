# GameEngine

## 项目简介

这是一个基于 C++ 和 SFML 开发的简易 2D 游戏引擎，采用类似于 Unity 的组件化架构设计。

## 功能特性

- 组件化的游戏对象系统（类似 Unity）
- 碰撞检测系统（支持圆形和矩形碰撞体）
- 事件总线机制用于组件间通信
- 基础物理系统（重力、移动控制）
- 相机跟随功能
- 可扩展的场景管理功能

## 项目结构

```
GameEngine/
├── src/
│   ├── Components/           # 组件系统
│   │   ├── Collisions/       # 碰撞组件
│   │   ├── CollisionHandles/ # 碰撞处理
│   │   ├── CameraComponent.h # 摄像机组件
│   │   ├── Component.h       # 组件基类
│   │   ├── Controller.h      # 控制器组件
│   │   ├── GravityComponent.h # 重力组件
│   │   └── MoveComponent.h   # 移动组件
│   ├── GameObjects/          # 游戏对象
│   │   ├── BoxGameObject.h   # 矩形游戏对象
│   │   ├── Cube.h           # 3D立方体
│   │   ├── GameObject.h     # 游戏对象基类
│   │   ├── Ground.h         # 地面对象
│   │   └── Player.h         # 玩家对象
│   ├── Scene/               # 场景系统
│   │   ├── GameScene.h      # 2D游戏场景
│   │   ├── GameScene3D.h    # 3D游戏场景
│   │   ├── Scene.h          # 场景基类
│   │   └── SceneContext.h   # 场景上下文
│   ├── Camera.h             # 摄像机类
│   ├── CollisionSystem.h    # 碰撞系统
│   ├── EventBus.h           # 事件总线
│   ├── Events.h             # 事件定义
│   ├── GameEngine.h         # 游戏引擎主类
│   └── main.cpp             # 主程序入口
├── CMakeLists.txt           # CMake构建文件
└── README.md               # 项目说明文件
```

## 技术栈

- **编程语言**: C++17
- **图形库**: SFML 2.6.1
- **构建系统**: CMake
- **开发环境**: CLion

## 安装与编译

### 依赖要求

- C++17编译器
- CMake 3.30或更高版本
- SFML 2.6.1库

### 编译步骤

1. 克隆项目:
   ```bash
   git clone <repository-url>
   cd GameEngine
   ```

2. 配置SFML路径:
   修改[CMakeLists.txt](file:///E:/Project/C++%20Program/CLion/GameEngine/CMakeLists.txt)中的SFML_ROOT路径为你的SFML安装路径

3. 编译项目:
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

## 运行示例

项目包含一个基本示例，展示了以下内容：

- 玩家控制的角色（圆形）
- 可移动的平台（矩形）
- 四周墙壁和地面
- 碰撞检测与响应
- 相机跟随效果

运行程序后，使用 WASD 键控制玩家角色移动。

## 使用说明

1. 创建自定义游戏对象继承自 [GameObject]()
2. 添加所需组件使用 `addComponent<T>()` 方法
3. 在 [Scene]() 中注册游戏对象
4. 实现相应的碰撞处理器处理交互逻辑

## 核心组件说明

### GameObject系统

游戏对象采用组件化设计，通过组合不同的组件实现不同的功能：

- [GameObject](): 游戏对象基类
- [Component](): 组件基类
- [Player](): 玩家对象
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


## 未来计划

- 添加更多类型的碰撞体
- 增强物理系统功能
- 支持精灵渲染和动画系统
- 添加音频支持

---

*该项目仅供学习和参考用途*