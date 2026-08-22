# Box2D 物理引擎接入计划

## 一、目标

为本引擎接入 Box2D 物理引擎，提供真实的刚体物理（质量、冲量、摩擦、弹性、堆叠），
与现有手写物理系统（Gravity/Move/CollisionSystem）**长期并存、按场景可选**。

> **2026-08-18 方向调整**：原计划是渐进迁移现有玩法到 Box2D 后删除旧物理。
> 实际验证后决定：现有玩法（Mario 等）保留旧物理（够用且手感已调好），
> Box2D 作为**第二套可选物理引擎**供新场景使用。
> 原 Step 9~13（玩法迁移）与 Step 15~17（旧物理清理）已取消。

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
- [x] 新建 `src/Physics/PhysicsWorld.h/.cpp`，封装 `b2World`、重力、`step(dt)` 固定步累加器
- [x] 提供 `createBody(def)` / `destroyBody(body)` 接口
- [x] 读取 `CONFIG.game.*` 物理参数（ConfigManager 扩展 physics 字段）
- **验证**：构造 PhysicsWorld，step 100 次不崩溃
- **新增文件**：`src/Physics/PhysicsWorld.h`、`src/Physics/PhysicsWorld.cpp`
- **改动文件**：`src/Manager/ConfigManager.h`（加 physics 参数）、`src/Manager/ConfigManager.cpp`（parse/save）
- **原子性保证**：纯新增类，未接入任何 Scene；ConfigManager 新字段有安全默认值，现有 config.json 无对应字段时用默认值

---

### 阶段二：组件层（新增 + 一次基类扩展）

#### Step 4+5 — PhysicsBodyComponent + Scene 集成（合并为一个原子提交）
- [x] 新建 `src/Physics/PhysicsBodyComponent.h/.cpp`，继承 `Component`
- [x] `start()` 时向所属场景的 PhysicsWorld 创建 `b2Body`，挂 fixture（支持矩形/圆形）
- [x] `update()` 时把 `b2Body` 位置回写到 `owner->position`（像素）
- [x] 析构时销毁 body，避免悬挂指针
- [x] 提供 `applyLinearImpulse`/`applyForceToCenter`/`setLinearVelocity`/`setTransform` 等 API
- [x] `Scene` 增加 `physics_world` 成员（默认 `nullptr`）与 `getPhysicsWorld()`
- [x] `Scene` 增加 `bool usePhysics = false` 开关
- [x] `Scene::init()` 当 `usePhysics==true` 才创建 PhysicsWorld
- [x] `Scene::update()` 当 `physics_world` 非空才调用 `step(dt)`（在对象 update 前）
- [x] `GameObject` 加 `friend PhysicsBodyComponent`（允许写 position）
- **验证**：所有现有场景 `usePhysics` 默认 false，行为完全不变；新组件未被任何对象使用
- **新增文件**：`src/Physics/PhysicsBodyComponent.h/.cpp`
- **改动文件**：`src/Scene/Scene.h`、`src/Scene/Scene.cpp`、`src/GameObjects/GameObject.h`
- **合并原因**：PhysicsBodyComponent::start() 依赖 `Scene::getPhysicsWorld()`，两者必须同 commit 才能编译通过
- **原子性保证**：Scene 新成员默认 `nullptr`/`false`，所有现有 Scene 子类零改动；PhysicsBodyComponent 未被任何对象使用

---

### 阶段三：碰撞桥接（纯新增 + API 扩展）

#### Step 6 — ContactListener → EventBus 桥接
- [x] 新建 `src/Physics/PhysicsContactListener.h/.cpp`，继承 `b2ContactListener`
- [x] `BeginContact`：从 `fixture->GetUserData()` 取 `GameObject*`，组装 `CollisionEvent`，发 EventBus
- [x] `EndContact`：发 `"onCollisionEnd"+tag`（新事件，旧代码不订阅则无影响）
- [x] `PhysicsWorld` 持有该 listener 并在构造时 `SetContactListener`
- [x] `PhysicsBodyComponent` 创建 fixture 时存 `userData = GameObject*`
- **验证**：两个 body 相撞，能触发现有 `"onCollision"+tag` 订阅者
- **新增文件**：`src/Physics/PhysicsContactListener.h/.cpp`
- **改动文件**：`src/Physics/PhysicsWorld.h/.cpp`（加 listener 成员）、`src/Physics/PhysicsBodyComponent.cpp`（存 userData）
- **原子性保证**：listener 仅在 `usePhysics=true` 的场景里生效，现有场景不受影响

#### Step 7 — 碰撞过滤与分组
- [x] 利用 `b2Filter`（categoryBits/maskBits/groupIndex）定义分组（Player/Enemy/Ground/Brick/Projectile/Trigger）
- [x] 在 `PhysicsBodyComponent` 支持 `setCollisionFilter`/`setCollisionGroup`（默认全通过）
- [x] `PhysicsContactListener` 加 `PreSolve` 钩子（默认空，单向碰撞逻辑留待具体对象迁移时实现）
- **验证**：两个 body 设互斥 mask，不再触发碰撞事件
- **改动文件**：`src/Physics/PhysicsTypes.h`（加 Category 常量）、`src/Physics/PhysicsBodyComponent.h/.cpp`、`src/Physics/PhysicsContactListener.h/.cpp`
- **原子性保证**：filter 默认全通过（categoryBits=0x0001, maskBits=0xFFFF），PreSolve 默认空实现，现有行为不变

---

### 阶段四：最小闭环验证（一次新增场景）

#### Step 8 — 新建 Box2D 测试场景
- [x] 新建 `src/Scene/PhysicsTestScene.h/.cpp`，构造时 `usePhysics = true`
- [x] 放两个动态方块（不同密度/弹性）+ 一块静态地面，验证下落、弹起
- [x] 在 `GameEngine::init` 注册该场景，MenuScene 加"物理测试"入口
- [x] ESC 返回菜单
- **验证**：进入测试场景，方块从空中落下，撞地弹起后静止
- **新增文件**：`src/Scene/PhysicsTestScene.h/.cpp`（内含 PhysicsBox/PhysicsGround 类）
- **改动文件**：`src/GameEngine.cpp`（注册场景）、`src/Scene/MenuScene.cpp`（加按钮）
- **原子性保证**：新增场景独立，仅在用户主动进入时才激活物理；现有场景未受影响

---

### 阶段五：玩法迁移（已取消）

> **2026-08-18 决定不迁移**：现有玩法（Mario/Ground/Brick/FireBall）的手写物理够用且手感已调好，
> 保留旧物理。Box2D 供新场景选用。若未来某玩法需要真实刚体物理（堆叠、滚动、弹性），
> 可参照 PhysicsTestScene 的模式单独迁移该场景。

#### Step 9~12 — ~~迁移 Mario/Ground/Brick/FireBall~~（已取消）
- [x] ~~迁移决定~~ → 改为保留旧物理，双引擎并存

#### Step 13 — ~~服务端权威物理同步~~（已取消）
- [x] ~~网络物理同步~~ → 随迁移取消；旧物理的网络同步维持现状

---

### 阶段六：可视化（可选增强）

#### Step 14 — Box2D Debug Draw
- [ ] 实现 `b2Draw` 子类，用 SFML 绘制 body 边界/质心/接触点
- [ ] 由 `CONFIG.game.debug` 控制开关（复用现有 debug 习惯）
- [ ] `PhysicsWorld` 持有 debugDraw，`Scene::render` 后调用
- **验证**：开启 debug 后能看到所有 body 的形状与接触法线
- **新增文件**：`src/Physics/PhysicsDebugDraw.h/.cpp`
- **改动文件**：`src/Physics/PhysicsWorld.h/.cpp`、`src/Scene/Scene.cpp`
- **原子性保证**：debugDraw 默认关闭，仅 `CONFIG.game.debug==true` 才绘制，关闭时零开销

#### Step 15~17 — ~~移除旧物理代码~~（已取消）
- [x] ~~删除 GravityComponent / CollisionSystem / 精简 MoveComponent~~ → 旧物理长期保留，不删除

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
            │         └─ Step 8 (测试场景) ← 最小闭环完成 ✅ 接入目标达成
            │              └─ Step 14 (DebugDraw，可选)
            │              └─ Step 9~13 (玩法迁移) ← 已取消
            │              └─ Step 15~17 (旧物理清理) ← 已取消
            └─ Step 7 (碰撞过滤)
```

**关键路径**：1 → 2 → 3 → 4+5 → 6 → 7 → 8 ✅ **已全部完成**（双引擎并存目标达成）

---

## 九、最终架构：双物理引擎可选

### 选择方式

| 层级 | 开关 | 说明 |
|------|------|------|
| 编译期 | `BUILD_WITH_BOX2D`（CMake option，默认 ON） | OFF 时 Box2D 不编译，二进制不含 Box2D |
| 场景级 | `Scene::usePhysics`（默认 false） | `true` → Box2D；`false` → 旧物理 |

### 两套系统的边界

| | 旧物理（手写） | 新物理（Box2D） |
|---|---|---|
| 组件 | GravityComponent / MoveComponent / BoxCollision | PhysicsBodyComponent |
| 世界 | CollisionSystem（O(n²) AABB） | PhysicsWorld（b2World + 固定步） |
| 碰撞事件 | `"onCollision"+tag` | 同样发 `"onCollision"+tag`（ContactListener 桥接） |
| 能力 | 速度积分 + AABB 检测，手感已调好 | 质量/冲量/摩擦/弹性/旋转/堆叠 |
| 使用场景 | SuperMarioScene 等现有玩法 | PhysicsTestScene 及未来新场景 |

### 使用 Box2D 的新场景写法（参照 PhysicsTestScene）

1. Scene 构造时 `usePhysics = true`
2. GameObject 构造时 `addComponent(make_shared<PhysicsBodyComponent>(...))`，先 `addObjectWithMap` 再 `start()`
3. 需要碰撞回调时订阅 `"onCollision"+tag`（与旧物理事件契约一致）
4. 手感调参：密度/摩擦/弹性/阻尼（见 PhysicsBodyComponent 的 setter）

### 注意事项（双引擎共存的纪律）

- **同一场景不混用**：一个场景要么全 Box2D（`usePhysics=true`），要么全旧物理，否则位置双重积分
- **同一对象不双挂**：挂了 PhysicsBodyComponent 就不要再挂 GravityComponent/MoveComponent
- 碰撞事件契约统一（`"onCollision"+tag`），玩法代码可无感切换订阅来源
