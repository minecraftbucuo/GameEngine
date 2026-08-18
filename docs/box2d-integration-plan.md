# Box2D 物理引擎接入计划

## 一、目标

为本引擎接入 Box2D 物理引擎，提供真实的刚体物理（质量、冲量、摩擦、弹性、堆叠），
替代目前手写的重力/运动/碰撞系统。采用**渐进式迁移**策略：新物理系统与旧系统共存，
逐步把现有玩法（Mario、地面、砖块、火球）迁到 Box2D，最后清理旧代码。

---

## 二、现状分析

### 现有架构（ECS-like 组件模式）

| 模块 | 位置 | 职责 |
|------|------|------|
| `GameObject` | [GameObject.h](file:///e:/Projects/GameEngine/src/GameObjects/GameObject.h) | 对象基类，持有 position/speed/size + 组件表 |
| `Component` | [Component.h](file:///e:/Projects/GameEngine/src/Components/Component.h) | 组件基类，start/update/render/handleEvent |
| `Scene` | [Scene.h](file:///e:/Projects/GameEngine/src/Scene/Scene.h) | 场景，update 遍历所有对象，持有 game_objects |
| `GameEngine` | [GameEngine.cpp](file:///e:/Projects/GameEngine/src/GameEngine.cpp#L78-L97) | 主循环：pollEvent → handleEvent → update → render（可变 dt） |

### 现有物理/碰撞系统（手写，将被替代）

| 模块 | 位置 | 现状 / 痛点 |
|------|------|------|
| `GravityComponent` | [GravityComponent.cpp](file:///e:/Projects/GameEngine/src/Components/GravityComponent.cpp) | 手动 `speed.y += gravity*dt`，无质量/冲量 |
| `MoveComponent` | [MoveComponent.cpp](file:///e:/Projects/GameEngine/src/Components/MoveComponent.cpp#L12-L14) | 手动积分 `position += speed*dt`，无真实物理响应 |
| `CollisionSystem` | [CollisionSystem.cpp](file:///e:/Projects/GameEngine/src/CollisionSystem.cpp) | O(n²) AABB 暴力检测，无穿透修正 |
| `BoxCollision`/`CircleCollision` | [Collisions/](file:///e:/Projects/GameEngine/src/Components/Collisions) | 手写形状检测 |
| `CollisionHandle` | [CollisionHandles/](file:///e:/Projects/GameEngine/src/Components/CollisionHandles) | 碰撞响应，通过 EventBus 订阅 `"onCollision"+tag` |
| 跳跃 | [MarioController.cpp](file:///e:/Projects/GameEngine/src/Components/MarioController.cpp#L61-L72) | `setSpeedY(-jumpForce)` + 持续 `addSpeed(0,-1815*dt)` 模拟持续上升力 |

**关键观察**：碰撞结果通过 `EventBus::publish("onCollision"+tag, CollisionEvent{...})` 分发，
玩法逻辑（如 `Mario::handleCollision`）订阅这些事件。迁移时**必须保留这套事件契约**，
否则现有玩法代码会全部失效。

---

## 三、技术决策

### 1. Box2D 版本：v2.4.x（经典 C++ API）

- **选 2.4**：面向对象 C++ API（`b2World`/`b2Body`/`b2Fixture`），与本引擎的组件风格贴合，
  社区迁移资料丰富，CMake `FetchContent` 友好。
- **不选 3.x**：C API 重写，学习成本高，对本项目无明显收益。
- 决策可调整：若后续需要 v3 特性再升级。

### 2. 坐标系：直接复用 SFML 坐标系（Y 向下）

- SFML：原点左上，Y 向下，单位像素。
- Box2D：原点任意，Y 方向的"上下"由**重力向量**决定，并非硬编码。
- **方案**：让 Box2D 直接使用 SFML 坐标系，重力向量设为 `(0, +g)`（g>0 表示向屏幕下方加速），
  渲染层无需翻转 Y。
- 注意点：接触法线方向在某些计算里与"Y 向上"约定相反，但对重力/跳跃/地面碰撞无影响。

### 3. 单位换算：像素 ↔ 米

- Box2D 推荐 0.1~10 米的物体，需用 pixels-per-meter (PPM) 缩放。
- **取 PPM = 64**（即 1 米 = 64 像素，与 `CONFIG.game.defaultBlockSize` 一致）。
- 转换在 `PhysicsWorld` 的同步层统一处理，玩法代码仍用像素。

### 4. 集成方式：场景级 PhysicsWorld + PhysicsBodyComponent

- 新建 `PhysicsWorld`（场景级，类似 `CollisionSystem`）：持有 `b2World`，负责 `Step` 与固定时间步。
- 新建 `PhysicsBodyComponent`（类似 `Collision`/`MoveComponent`）：封装 `b2Body` 创建、参数、每帧同步。
- 旧组件（`GravityComponent`/`MoveComponent`/`Collision`）保持不动，迁到 Box2D 的对象停用旧组件。
- `Scene` 增加一个 `PhysicsWorld` 成员与 `getPhysicsWorld()`，在 `update` 里调用 `physicsWorld->step(dt)`。

### 5. 固定时间步（必须）

- Box2D 要求固定时间步保证稳定。现有主循环用 `clock.restart()` 可变 dt。
- 在 `PhysicsWorld::step` 内用**累加器**实现：每帧累加 dt，每 1/60s 调用一次 `b2World::Step`，
  剩余时间留到下一帧。最多每帧 5 步防止螺旋死亡。

### 6. 碰撞事件桥接：ContactListener → EventBus

- 实现 `b2ContactListener` 子类，在 `BeginContact`/`EndContact` 中：
  从 fixture 的 `userData` 取出 `GameObject*`，组装 `CollisionEvent`，
  发到 `EventBus::publish("onCollision"+tag, ...)`。
- 这样现有 `"onCollision"+tag` 订阅者零改动。

### 7. 物理材质映射（来自 config）

| 配置项 | 含义 |
|--------|------|
| `game.gravity` | 重力加速度（像素/s²），转成 m/s² 喂给 Box2D |
| 新增 `game.physicsVelocityIterations` | 速度求解迭代次数（默认 8） |
| 新增 `game.physicsPositionIterations` | 位置求解迭代次数（默认 3） |
| 新增 `game.physicsFixedStep` | 固定步长秒数（默认 1/60） |

---

## 四、分阶段实施计划

### 原子性原则（每一步必须满足）

> **每一步 = 一次 git commit = 一个原子操作**，必须同时满足：

1. **可编译**：该步完成后 `cmake --build` 通过，无编译错误。
2. **可运行**：程序能启动，所有**未迁移**的现有功能行为不变。
3. **可回滚**：`git revert <commit>` 后系统回到上一步状态，无残留依赖。
4. **单一职责**：一步只做一件事（新增一个模块 / 迁移一个对象 / 删除一组废弃代码）。
5. **不破坏契约**：修改公共 API 时用默认值保持向后兼容，旧调用方零改动。

**实施纪律**：
- 纯新增文件（无人引用）天然原子，是最安全的步骤。
- 修改基类时，新成员必须有安全默认值（指针给 `nullptr`、开关给 `false`）。
- 迁移玩法时，**一个对象一个 commit**，不批量迁移。
- 删除代码必须是该文件链路上的最后一个 commit，且删前已确认无引用。

完成后将 `[ ]` 改为 `[x]`。

---

### 阶段一：基础设施（纯新增，最安全）

#### Step 1 — CMake 接入 Box2D
- [x] 用 `FetchContent` 拉取 Box2D 2.4.x
- [x] 新增 CMake option `BUILD_WITH_BOX2D`（默认 ON）
- [x] 链接到主 target（`target_link_libraries` 附加 `box2d`）
- **验证**：`cmake --build` 通过，Box2D 被编译进项目
- **改动文件**：`CMakeLists.txt`
- **原子性保证**：仅新增依赖与链接，不改任何现有源码逻辑；若 Box2D 拉取失败，关 `BUILD_WITH_BOX2D` 即恢复

#### Step 2 — 坐标/单位转换工具
- [x] 新建 `src/Physics/PhysicsTypes.h`，定义 `PPM`、`toMeters()`、`toPixels()`、向量转换
- [x] 定义 `BodyType` 枚举映射 `b2BodyType`
- [x] CMake 里把 `src/Physics` 加入 include path
- **验证**：`toPixels(toMeters(128))` ≈ 128（浮点精度内）
- **新增文件**：`src/Physics/PhysicsTypes.h`
- **原子性保证**：纯新增头文件，无任何现有代码引用它，编译必然通过

#### Step 3 — PhysicsWorld（场景级 b2World 管理）
- [ ] 新建 `src/Physics/PhysicsWorld.h/.cpp`，封装 `b2World`、重力、`step(dt)` 固定步累加器
- [ ] 提供 `createBody(def)` / `destroyBody(body)` 接口
- [ ] 读取 `CONFIG.game.*` 物理参数
- **验证**：构造 PhysicsWorld，step 100 次不崩溃
- **新增文件**：`src/Physics/PhysicsWorld.h`、`src/Physics/PhysicsWorld.cpp`
- **原子性保证**：纯新增类，未接入任何 Scene，编译通过且对运行时零影响

---

### 阶段二：组件层（新增 + 一次基类扩展）

#### Step 4 — PhysicsBodyComponent（封装 b2Body）
- [ ] 新建 `src/Physics/PhysicsBodyComponent.h/.cpp`，继承 `Component`
- [ ] `start()` 时向所属场景的 PhysicsWorld 创建 `b2Body`，挂 fixture
- [ ] `update()` 时把 `b2Body` 位置/角度回写到 `owner->position`（像素）
- [ ] 析构时销毁 body
- **验证**：手动构造 Component 并 attach 到对象，能创建/销毁 body 不崩溃
- **新增文件**：`src/Physics/PhysicsBodyComponent.h/.cpp`
- **原子性保证**：纯新增组件，没有任何 GameObject 使用它，运行时零影响

#### Step 5 — Scene 集成 PhysicsWorld（向后兼容扩展）
- [ ] `Scene` 增加 `std::unique_ptr<PhysicsWorld> physics_world` 成员（默认 `nullptr`）
- [ ] 增加 `bool usePhysics = false` 开关
- [ ] 增加 `getPhysicsWorld()` 访问器
- [ ] `Scene::init()` 中当 `usePhysics==true` 才创建 PhysicsWorld
- [ ] `Scene::update()` 中当 `physics_world` 非空才调用 `step(dt)`
- [ ] `Scene::exit()` 中 reset
- **验证**：所有现有场景 `usePhysics` 默认 false，行为与之前完全一致
- **改动文件**：`src/Scene/Scene.h`、`src/Scene/Scene.cpp`
- **原子性保证**：新成员默认 `nullptr`/`false`，所有现有 Scene 子类无需改动，`step()` 不被调用，运行时行为不变

---

### 阶段三：碰撞桥接（纯新增 + API 扩展）

#### Step 6 — ContactListener → EventBus 桥接
- [ ] 新建 `src/Physics/PhysicsContactListener.h/.cpp`，继承 `b2ContactListener`
- [ ] `BeginContact`：从 `fixture->GetUserData()` 取 `GameObject*`，组装 `CollisionEvent`，发 EventBus
- [ ] `EndContact`：发 `"onCollisionEnd"+tag`（新事件，旧代码不订阅则无影响）
- [ ] `PhysicsWorld` 持有该 listener 并在构造时 `SetContactListener`
- **验证**：构造 PhysicsWorld + 两个 body 相撞，能触发 `"onCollision"+tag`
- **新增文件**：`src/Physics/PhysicsContactListener.h/.cpp`
- **改动文件**：`src/Physics/PhysicsWorld.h/.cpp`（仅加 listener 成员与 SetContactListener 调用）
- **原子性保证**：listener 默认空操作，未接入任何场景的 PhysicsWorld 实例（因 `usePhysics` 默认 false），运行时零影响

#### Step 7 — 碰撞过滤与分组
- [ ] 利用 `b2Filter`（categoryBits/maskBits/groupIndex）定义分组（玩家、敌人、地面、抛体、触发器）
- [ ] 在 `PhysicsBodyComponent` 创建参数里支持设置 filter（默认全通过）
- [ ] 单向碰撞（如玩家从下方撞砖块）用 `b2Contact` 的 `SetEnabled` 在 PreSolve 处理
- **验证**：两个 body 设互斥 mask，不再触发碰撞事件
- **改动文件**：`src/Physics/PhysicsBodyComponent.h/.cpp`、`src/Physics/PhysicsContactListener.cpp`（加 PreSolve）
- **原子性保证**：filter 默认全通过（0xFFFF/0xFFFF），旧调用方式行为不变

---

### 阶段四：最小闭环验证（一次新增场景）

#### Step 8 — 新建 Box2D 测试场景
- [ ] 新建 `src/Scene/PhysicsTestScene.h/.cpp`，构造时 `usePhysics = true`
- [ ] 放一个动态方块 + 一块静态地面，验证下落、堆叠、弹性
- [ ] 在 `GameEngine::init` 注册该场景，MenuScene 加入口
- **验证**：进入测试场景，方块落到地面弹起后静止；其他场景行为不变
- **新增文件**：`src/Scene/PhysicsTestScene.h/.cpp`
- **改动文件**：`src/GameEngine.cpp`（注册场景）、`src/Scene/MenuScene.cpp`（加按钮）
- **原子性保证**：新增场景独立，仅在用户主动进入时才激活物理；现有场景未受影响

---

### 阶段五：迁移现有玩法（一个对象一个 commit）

> 每个迁移步骤必须**一次性完成该对象的所有改动**（GameObject + Controller + 相关组件），
> 保证中间不存在"半迁移"状态。迁移完成后该对象停用旧物理组件。

#### Step 9 — 迁移 Mario（动态体）
- [ ] `Mario` 增加 `PhysicsBodyComponent`（动态 body）
- [ ] `Mario` 停用 `GravityComponent`（移除或 setActive(false)）
- [ ] `MarioController::jump()` 改为 `ApplyLinearImpulse`（向上冲量）
- [ ] `MarioController::runLeft()/runRight()` 改为 `SetLinearVelocity`（保留 x，保留 y）
- [ ] `MarioController::update()` 中持续上升力改为冲量一次性施加
- [ ] 远端玩家（非本地）用 kinematic body，`SetTransform` 同步服务端位置
- [ ] `SuperMarioScene` 设 `usePhysics = true`
- **验证**：Mario 能跑、能跳、落地停止，手感与旧版相近；其他场景不受影响
- **改动文件**：`src/GameObjects/Mario.h/.cpp`、`src/Components/MarioController.cpp`、`src/Scene/SuperMarioScene.cpp`
- **原子性保证**：Mario 的所有物理改动在一个 commit 内完成，SuperMarioScene 是唯一受影响场景；其他场景 `usePhysics` 仍为 false
- **回滚**：`git revert` 后 Mario 恢复旧物理，SuperMarioScene 恢复 `usePhysics=false`

#### Step 10 — 迁移 Ground（静态体）
- [ ] `Ground` 增加 `PhysicsBodyComponent`（静态 body）
- [ ] `Ground` 停用 `BoxCollision`（Box2D 已检测），保留渲染
- [ ] 确认 Mario 踩在 Ground 上不穿模
- **验证**：SuperMarioScene 中 Mario 落地停在 Ground 上
- **改动文件**：`src/GameObjects/Ground.h/.cpp`
- **原子性保证**：仅改 Ground 一个类；若回滚，Ground 恢复旧 BoxCollision，Mario 仍能用旧碰撞

#### Step 11 — 迁移 Brick（静态体 + PreSolve）
- [ ] `Brick` 增加 `PhysicsBodyComponent`（静态 body）
- [ ] `Brick` 停用 `BoxCollision`
- [ ] "被撞顶"逻辑改为监听 PreSolve 接触法线判断（向上法线 = 从下方撞）
- [ ] 确认 Mario 从下方撞砖块触发既有砖块逻辑
- **验证**：Mario 从下方撞砖块，砖块按既有逻辑响应（弹起/破碎）
- **改动文件**：`src/GameObjects/Brick.h/.cpp`
- **原子性保证**：仅改 Brick 一个类，独立 commit，可单独回滚

#### Step 12 — 迁移 FireBall（抛体）
- [ ] `FireBall` 增加 `PhysicsBodyComponent`（动态体，忽略重力或自定义重力缩放）
- [ ] 初速度改为 `SetLinearVelocity`
- [ ] TTL 销毁逻辑保留，destroy 时由 PhysicsBodyComponent 析构 body
- [ ] 碰撞事件仍走 EventBus（ContactListener 已桥接）
- **验证**：火球按抛物线飞行，撞墙/敌人触发既有事件
- **改动文件**：`src/GameObjects/FireBall.h/.cpp`
- **原子性保证**：仅改 FireBall 一个类，独立 commit

---

### 阶段六：网络同步

#### Step 13 — 服务端权威物理同步
- [ ] 服务端 `SuperMarioScene` 设 `usePhysics = true`，跑 Box2D
- [ ] 客户端预测 + 服务端校正（沿用现有 `reconcileLocalPlayer` 思路）
- [ ] 远端玩家用 kinematic body，`SetTransform` 直接同步服务端位置
- [ ] 网络包只传位置/速度/朝向，不传 Box2D 内部状态
- **验证**：双人联机，远端玩家移动平滑，碰撞一致
- **改动文件**：`src/GameObjects/Mario.cpp`、`src/Network/NetworkManager.cpp`、`src/Scene/SuperMarioScene.cpp`
- **原子性保证**：仅在 `BUILD_FOR_SERVER` 或 `isClient()` 分支内改动，单机模式行为不变

---

### 阶段七：清理与可视化

#### Step 14 — Box2D Debug Draw
- [ ] 实现 `b2Draw` 子类，用 SFML 绘制 body 边界/质心/接触点
- [ ] 由 `CONFIG.game.debug` 控制开关（复用现有 debug 习惯）
- [ ] `PhysicsWorld` 持有 debugDraw，`Scene::render` 后调用
- **验证**：开启 debug 后能看到所有 body 的形状与接触法线
- **新增文件**：`src/Physics/PhysicsDebugDraw.h/.cpp`
- **改动文件**：`src/Physics/PhysicsWorld.h/.cpp`、`src/Scene/Scene.cpp`
- **原子性保证**：debugDraw 默认关闭，仅 `CONFIG.game.debug==true` 才绘制，关闭时零开销

#### Step 15 — 移除 GravityComponent
- [ ] 全局搜索确认无 GameObject 添加 `GravityComponent`（Mario 已迁）
- [ ] 删除 `src/Components/GravityComponent.h/.cpp`
- [ ] 删除 CMake glob 自动收录的引用
- **验证**：全量编译通过，所有场景运行正常
- **删除文件**：`src/Components/GravityComponent.h`、`src/Components/GravityComponent.cpp`
- **原子性保证**：删除前已确认无引用（Step 9 已停用），删除后编译必然通过
- **回滚**：`git revert` 恢复文件

#### Step 16 — 移除 CollisionSystem（可选）
- [ ] 确认所有场景已迁到 Box2D，`CollisionSystem` 不再被调用
- [ ] 若有非物理场景仍需 AABB 检测，保留；否则删除
- **验证**：全量编译通过
- **删除文件**：`src/CollisionSystem.h/.cpp`（视情况）
- **原子性保证**：删除前已确认 `Scene::getCollisionSystem()` 无调用方，或调用方已迁移

#### Step 17 — 精简 MoveComponent
- [ ] 移除 `MoveComponent` 中已被 PhysicsBodyComponent 取代的 `update()` 积分逻辑
- [ ] 保留 `setSpeed`/`addSpeed` 等纯速度访问器（网络同步仍需）
- [ ] 保留 `drawArrow` debug 工具
- **验证**：全量编译通过，已迁移对象由 Box2D 驱动位置
- **改动文件**：`src/Components/MoveComponent.cpp`
- **原子性保证**：仅删除已被取代的积分代码，保留访问器；未迁移对象（若有）仍可调用

---

## 五、新增文件清单

| 路径 | 说明 | 对应 Step |
|------|------|-----------|
| `src/Physics/PhysicsTypes.h` | PPM/坐标转换、BodyType | 2 |
| `src/Physics/PhysicsWorld.h/.cpp` | b2World 封装 + 固定步 | 3 |
| `src/Physics/PhysicsBodyComponent.h/.cpp` | b2Body 组件封装 | 4 |
| `src/Physics/PhysicsContactListener.h/.cpp` | 碰撞事件桥接 | 6 |
| `src/Physics/PhysicsDebugDraw.h/.cpp` | Debug 可视化 | 14 |
| `src/Scene/PhysicsTestScene.h/.cpp` | 验证场景 | 8 |

## 六、改动文件清单

| 路径 | 改动 | 对应 Step |
|------|------|-----------|
| `CMakeLists.txt` | FetchContent 拉 Box2D，新增 include path | 1, 2 |
| `src/Manager/ConfigManager.h` | 新增物理参数字段 | 3 |
| `src/Asset/config.json` | 新增物理参数默认值 | 3 |
| `src/Scene/Scene.h/.cpp` | 持有 PhysicsWorld，step | 5, 14 |
| `src/GameObjects/GameObject.h` | 暴露 setPosition 给物理同步（friend） | 4 |
| `src/GameObjects/Mario.*` | 动态体迁移 | 9, 13 |
| `src/Components/MarioController.cpp` | 输入改为施力/冲量 | 9 |
| `src/Scene/SuperMarioScene.cpp` | usePhysics=true | 9, 13 |
| `src/GameObjects/Ground.cpp` | 静态体迁移 | 10 |
| `src/GameObjects/Brick.cpp` | 静态体 + PreSolve | 11 |
| `src/GameObjects/FireBall.*` | 抛体迁移 | 12 |
| `src/GameEngine.cpp` | 注册 PhysicsTestScene | 8 |
| `src/Scene/MenuScene.cpp` | 加测试场景入口 | 8 |
| `src/Network/NetworkManager.cpp` | 物理同步 | 13 |
| `src/Components/MoveComponent.cpp` | 精简积分逻辑 | 17 |

---

## 七、风险与注意事项

1. **手感回退**：从"直接设速度"迁到"施力/冲量"会有惯性，需调参（密度、摩擦、线性阻尼）逼近旧手感。
2. **固定步与可变帧率**：渲染帧率（165fps）与物理步（60Hz）解耦，插值由 PhysicsBodyComponent 同步处理。
3. **网络同步**：服务端必须跑相同的物理步，否则客户端预测会发散；建议固定步锁 60Hz。
4. **坐标系陷阱**：Box2D 角度方向、接触法线在 Y 向下系下方向相反，写 PreSolve 逻辑时务必验证方向。
5. **迁移期间两套物理共存**：同一场景内不要混用 Box2D body 与旧 `GravityComponent`，否则位置会被双重积分。
   → 原子性保证：Step 9 迁移 Mario 时**同一 commit 内**停用 GravityComponent，不存在半迁移状态。
6. **userData 生命周期**：`fixture->SetUserData` 存裸指针 `GameObject*`，对象销毁前必须先销毁 body，避免悬挂指针。
   → PhysicsBodyComponent 析构顺序：先 `DestroyBody` 再让 GameObject 析构。

---

## 八、执行顺序与依赖图

```
Step 1 (CMake)
  └─ Step 2 (PhysicsTypes)
       └─ Step 3 (PhysicsWorld)
            ├─ Step 4 (PhysicsBodyComponent)
            │    └─ Step 5 (Scene 集成)
            │         └─ Step 8 (测试场景) ← 最小闭环完成
            │              ├─ Step 9 (Mario)
            │              │    ├─ Step 10 (Ground)
            │              │    ├─ Step 11 (Brick)
            │              │    └─ Step 12 (FireBall)
            │              │         └─ Step 13 (网络同步)
            │              │              ├─ Step 14 (DebugDraw)
            │              │              ├─ Step 15 (删 GravityComponent)
            │              │              ├─ Step 16 (删 CollisionSystem)
            │              │              └─ Step 17 (精简 MoveComponent)
            │              └─ Step 6 (ContactListener) ← 可在 Step 8 前做
            └─ Step 7 (碰撞过滤)
```

**关键路径**：1 → 2 → 3 → 4 → 5 → 8（最小闭环）→ 9 → 10/11/12（并行可）→ 13 → 14/15/16/17（并行可）

每完成一步，提交一次 git，便于回滚。
