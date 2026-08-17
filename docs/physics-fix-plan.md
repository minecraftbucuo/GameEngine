# 碰撞检测与碰撞处理修复方案

> 排查日期：2026-06（基于 `physics-fix` 分支，`9fa0f42`）
> 现象：demo 场景（`GameScene` 鼠标点击生成球）中，多个球叠在一起时会有几率乱跳/瞬移。
> 用法：每完成一步，把该步的 `[ ]` 勾选为 `[x]`。

---

## 一、修复进度总览

| 阶段 | 步骤 | 状态 |
|------|------|------|
| 阶段 A 短期 | A1 位置修正分摊（双方相互作用） | ✅ |
| | A2 圆-圆接触静止检测与法向速度清零 | ✅ |
| | A3 dir_len==0 除零保护 | ✅ |
| | A4 位置修正（全量，最快收敛） | ✅ |
| | A5 统一重力/静止判定（移除事件开关） | ✅ |
| | A6 编译并验证 demo 叠球 | ✅ |
| 阶段 B 重构 | B1 Contact 结构 + 迭代求解器 | ✅ |
| | B2 逆质量 + 动量守恒冲量 | ✅ |
| | B3 统一碰撞体坐标语义 | ✅ |
| | B4 broad-phase（空间哈希） | ✅ |
| | B5 物理参数配置化 | ✅ |
| | B6 删除重复逻辑并回归 | ✅ |
| 阶段 C 收尾 | C1 b 侧空指针判空 | ⬜ |
| | C2 typeid hash 分发改显式分发 | ⬜ |
| | C3 删除 needGravity 探针写法 | ⬜ |
| | C4 CircleCollision::setPosition 参数问题 | ⬜ |

---

## 二、现状梳理（一次完整帧内碰撞的流程）

```
GameEngine::start (165fps)
└─ scene_manager->update(dt)
   ├─ Scene::update(dt)                      // 每个对象 update
   │   ├─ Circle::update
   │   │   ├─ needGravity()                  // 探针：碰撞体往下挪 1px 看是否碰到东西，决定开关重力
   │   │   └─ GameObject::update
   │   │       ├─ GravityComponent::update   // 只对"窗口底部"做静止判定，其余情况永远累加重力
   │   │       └─ MoveComponent::update      // position += speed * dt，同步碰撞体位置
   │   └─ ...（Mario / Ground / Box 等）
   └─ GameScene::update
      └─ CollisionSystem::checkCollisions()  // 收集碰撞对 → 每个 pair 发两个事件
         └─ EventBus publish("onCollision"+tagA)  → Circle 订阅者 → CollisionHandle::handleCollision
         └─ EventBus publish("onCollision"+tagB)  → 同上（同一个 pair 处理两次！）
            └─ CircleCollisionHandle::handleCollisionWithCircle
               ├─ addSpeed(dir * relativeSpeed_len * 0.28f)   // 注入回弹冲量
               └─ addPosition(move_dis * dir)                 // 把整个重叠距离推出去
```

---

## 三、问题定位

### 3.1 demo“球叠在一起乱跳”的直接根因（按严重程度排序）

1. **每对碰撞被处理两次，且都施加“完整分离 + 完整冲量”**
   `CollisionSystem::checkCollisions()`（`src/CollisionSystem.cpp:54-60`）对 pair (A,B) 发布两个事件：A 的订阅者把 A 推开（完整重叠距离 + 0.28×相对速度冲量），B 的订阅者把 B 也推开。净效果是分离距离是需要的 2 倍、冲量注入 2 次。且第二次处理（B）用的是收集 pair 时的**过期快照**（`event.a_position` 是 A 处理前的旧位置），A 已被移走，B 却按旧位置计算 → 被推错方向/推过头，陷入其它球里 → 下一帧重叠更大 → 修正更大 → 抖动放大成乱跳。

2. **圆-圆接触从不把速度归零/阻尼到静止**
   `CircleCollisionHandle::handleCollisionWithCircle`（`src/Components/CollisionHandles/CircleCollisionHandle.cpp:25-49`）只有 `addSpeed(dir * relativeSpeed_len * 0.28f)` + `addPosition(move_dis * dir)`，没有任何静止检测，也不清法向速度。对比 box 路径（`CollisionHandle.cpp:44-46, 60-62, 68-70`）有 `|speed| <= 2` / `<= 150` 的归零逻辑，圆-圆路径完全没有。重力每帧累加 + 每次接触注能 → 叠着的球永远微弹跳，偶尔累积成大跳。

3. **`dir_len == 0` 时除零产生 NaN**
   堆叠时两圆圆心可能精确重合（float 相等），`dir /= dir_len` 得 NaN → `addSpeed(NaN)` / `addPosition(NaN)` → 位置变 NaN → 球瞬间消失/随机位置（“乱跳”最极端的概率性表现）。

4. **位置修正基于过期快照、无迭代**
   `move_dis = (r_a + r_b - dir_len)` 用的是收集 pair 时的旧位置。同一帧内先处理的 pair 已把球移走，后处理的 pair 仍用旧位置 → 修正互相矛盾，把球推回别的球里，下一帧出现大重叠 → 大修正 → 跳。

5. **重力开关逻辑互相打架**
   `Circle::needGravity()`（`src/GameObjects/Circle.cpp:61-79`）用“碰撞体下移 1px 探测”判断；碰撞事件订阅者里 `setActive(false)`（`Circle.cpp:48`）一旦碰到任何东西就关重力；`GravityComponent::update` 的静止判定只针对窗口底部（`GravityComponent.cpp:13-14`）。三套逻辑对“是否静止”各说各话，每帧开关抖动。

### 3.2 设计层面的问题清单（不直接引发 demo bug，但影响可维护性与正确性）

| # | 问题 | 位置 |
|---|------|------|
| 1 | **响应逻辑分散、重复**：box 解析逻辑在 `CollisionHandle::handle` 和 `Mario::handleCollision` 各写一份且行为不一致（Mario 版有落地归零、通用版没有） | `CollisionHandle.cpp` / `Mario.cpp:133-205` |
| 2 | **没有质量/逆质量概念**：两物体各自移动完整重叠距离（等效双无穷质量）；静态靠 `if (!getMoveAble()) return` 硬跳过，不是按质量分摊 | `CollisionHandle.cpp:28`、`CircleCollisionHandle.cpp:29` |
| 3 | **冲量模型不守恒、魔法数字遍地**：`0.28f`、`2.f`、`150.f`、`0.1f`；“相对速度大小×系数直接 add/set”不是按法向分解的动量冲量，接触时不消除法向相对速度 → 能量注入 | 各 handle / `GravityComponent.cpp` |
| 4 | **事件快照语义脆弱**：`CollisionEvent.a_position` 是碰撞体左上角（`getCollisionPosition()`），处理逻辑却把它当 GameObject 左上角并用 `getSize()` 推算；`CircleCollision::getCollisionPosition()`（减半径）与基类 `Collision::getCollisionPosition()`（不减）语义不一致 | `CircleCollision.cpp:70-72` / `Collision.cpp:23-25` |
| 5 | **无 broad-phase，O(n²)**：`checkCollisions` 全对遍历；`needGravity` 每球每帧再全量扫一遍 → 两重 O(n²) | `CollisionSystem.cpp:34-52` / `Circle.cpp:66-77` |
| 6 | **潜在空指针崩溃**：`if (auto b_c = ...; b_c->getActive() && ...)` 中 b 无 Collision 组件时 `b_c` 为 nullptr 直接解引用（a 侧有判空，b 侧没有） | `CollisionSystem.cpp:44` |
| 7 | **typeid hash 分发脆弱**：`typeid(...).hash_code()` 查表，理论上有 hash 冲突；且 b 无 Collision 时 `getComponent` 返回 nullptr → 崩溃 | `CollisionHandle.cpp:15-22` |
| 8 | **box 路径方向判定粗糙**：用“A 的哪条边离 B 中心更近”判断推哪边，AABB 重心偏移时判错；没有法向速度分离，接触后法向速度保留 → 反复穿透 | `CollisionHandle.cpp:47-55` |
| 9 | **接口自相矛盾**：`CircleCollision::setPosition(const sf::Vector2f&)` 忽略参数，用 owner 位置硬算；`setPosition` / `setCollisionPosition` / `getPosition` / `getCollisionPosition` 四套坐标语义容易混用 | `CircleCollision.cpp:35-37` |
| 10 | **修正与速度不同步**：位置修正直接 `addPosition` 但速度没变（圆路径），下一帧 `MoveComponent::update` 又把球移回穿透状态 → 每帧“穿透-修正-穿透”循环 | `MoveComponent.cpp:12-15` |
| 11 | **`needGravity` 副作用探测**：修改碰撞体位置做检测再改回来，既有浮点误差又每帧全量遍历；且与事件里的重力开关逻辑重复 | `Circle.cpp:61-79` / `Mario.cpp:112-131` |
| 12 | **addSpeed vs setSpeed 不一致**：圆路径 add、box 路径 set，行为不统一 | `CircleCollisionHandle.cpp:44` / `CollisionHandle.cpp:43` |
| 13 | **响应顺序敏感**：pair 按收集顺序逐个处理，先处理的结果改变后续 pair 的判定基准（快照过期），结果对顺序敏感 → 非确定性抖动 | `CollisionSystem.cpp:55-60` |

---

## 四、分步修复清单

> 约定：每步含【改动】【完成标准】。完成标准满足后，把该步 `[ ]` 改为 `[x]`。

### 阶段 A：短期修复（消除 demo 乱跳）

#### A1 位置修正分摊（保留双方相互作用）

- [x] **改动**（已实测通过）
  - 修正思路：碰撞是相互的，**双方都要收到事件并各自响应**（不能只让一方响应，否则"撞不动"、物理是假的）；真正的双倍修正问题是**双方各自移动了完整重叠距离**（总分离 2 倍），所以把修正距离按比例分摊即可。
  - `src/CollisionSystem.cpp:54-60`：**保留**每对碰撞双方各发布一次事件（A 与 B 都收到 `onCollision`）。
  - `src/Components/CollisionHandles/CircleCollisionHandle.cpp`：圆-圆分离时，对方可移动则本方只推 `move_dis * 0.5f`（双方各分摊一半），对方静止（如地面）则推完整距离。
  - `src/Components/CollisionHandles/CollisionHandle.cpp`（box 路径）：同样分摊 —— `moveCollisionXTo/YTo` 改为推到"当前位置与目标的中点"（对方可动时各分一半；对方静止时推到位）。
- [x] **完成标准**：两球碰撞时双方都会动（各分摊一半），总分离≈重叠深度（不再是 2 倍）；球撞地面/墙时只有球动、行为不变；叠球抖动明显减弱。

#### A2 圆-圆接触静止检测与法向速度清零

- [x] **改动**
  - `src/Components/CollisionHandles/CircleCollisionHandle.cpp`（`handleCollisionWithCircle`）：
    1. 先算法向相对速度 `v_rel_n = dot(a_speed - b_speed, dir)`（dir 已归一化，方向为从 b 指向 a）。
    2. 若 `v_rel_n < 0`（正在接近）且 `|v_rel_n| < 阈值`（`RESTING_SPEED_THRESHOLD = 40.f`）→ **接触静止**：把本方法向速度清零（`addSpeed(-dir * dot(a_speed, dir))`），不再注入回弹冲量。
    3. 若接近速度较大，才保留回弹冲量（`BOUNCE_FACTOR = 0.1f`，阶段 B 换恢复系数模型）。
  - 阈值与回弹系数定义为文件内匿名 namespace 常量，阶段 B 再配置化。
  - 调优记录：堆叠振荡（"弹簧感"）修复 —— 新球砸在堆顶时撞击速度大，0.28 回弹系数把撞击能量注入堆内并逐层传播反射；将球-球回弹系数降为 0.1、静止阈值升为 40，堆叠快速吸收撞击并静止。球-地面弹跳走 box 路径（0.28 + 150 阈值）不受影响。
- [x] **完成标准**：两个球叠在一起（或一个球静止在另一个球顶上）能趋于静止，不再持续微弹跳；在静止堆叠上再放一个球时，堆不会像弹簧一样长时间振荡。

#### A3 dir_len == 0 除零保护

- [x] **改动**
  - `src/Components/CollisionHandles/CircleCollisionHandle.cpp`（`handleCollisionWithCircle`）：`dir_len < 1e-6f` 时：
    - 不执行 `dir /= dir_len`；
    - 取安全分离方向：能分辨符号时按位置差取轴对齐方向（优先水平），完全重合时固定 `(0,-1)` 向上；
    - 跳过冲量注入（`degenerate` 时法向无意义），只做位置分离，`move_dis ≈ 两球半径和`。
- [x] **完成标准**：两球圆心完全重合（如点击生成在同一坐标）时不再产生 NaN，球正常分开。

#### A4 位置修正（全量，最快收敛）

- [x] **改动**
  - `src/Components/CollisionHandles/CircleCollisionHandle.cpp`：位置修正改为**全量** —— `move_dis = penetration`，一帧内完全分开（收敛最快）。双移动物体分摊已在 A1 完成（双方各推一半，总分离恰好=穿透，不过度分离）。
  - `src/Components/CollisionHandles/CollisionHandle.cpp`（box 路径）：`moveToHalfX/Y` 同样全量修正。
  - 调优记录：曾尝试固定 2px 限幅（大穿透分开过慢）、比例修正（30%/50% + 上限，实测仍偏慢），均被否决，最终恢复全量修正。
- [x] **完成标准**：快速生成多个球堆叠时，穿透一帧内完全分开，无"粘在一起"现象。

#### A5 统一重力/静止判定（移除事件开关）

- [x] **改动**
  - `src/GameObjects/Circle.cpp`：删除碰撞事件订阅者里的 `this->getComponent<GravityComponent>()->setActive(false);`，重力不再因"碰到任何东西"被关闭。
  - `Circle::update` 里重力开关**统一由 `needGravity()` 探针双向决定**：下方悬空 → `setActive(true)`；下方有支撑 → `setActive(false)`。静止堆叠中的球重力关闭、不累加速度。
  - 调优记录：最初只保留"探针开启"路径，导致 A5 后重力一旦开启就永远施加（静止的球每帧被重力累加速度），已补上探针的关闭路径。
  - 效果：消除"探针 / 事件开关 / GravityComponent 底部判定"三套逻辑互相打架的抖动源；球被侧面碰撞时不再"失重漂浮"，行为更真实。
- [x] **完成标准**：球静止在地面/其它球上时重力关闭、速度保持为 0，无重力累加导致的缓慢下沉或弹跳。

#### A6 编译并验证 demo 叠球

- [ ] **改动**
  - 无代码改动；编译运行，在 `GameScene` 快速点击生成 10~20 个球堆叠，观察 30s。
- [ ] **完成标准**：无乱跳/瞬移/穿地/持续抖动；球最终静止堆叠；开启 `game.debug` 可看到碰撞框与速度箭头正常。

### 阶段 B：中期重构（统一物理求解）

#### B1 Contact 结构 + 迭代求解器

- [x] **改动**
  - `src/CollisionSystem.h`：新增 `Contact { a, b, normal, penetration, a/b_speed, a/b_position }` 结构。
  - `src/CollisionSystem.cpp`：`checkCollisions()` 重构为三段 —— ① 检测产 `std::vector<Contact>`（几何计算集中：circle-circle / circle-box / box-box 的法向与穿透，`normal` 统一"从 b 指向 a"）；② `solveContacts()` 迭代求解（速度 4 轮迭代、每轮用最新速度，位置最后一次全量分摊）；③ 发布事件（仅游戏逻辑）。
  - 物理响应（位置修正/速度冲量）不再走事件：`Circle.cpp`、`Player.cpp`、`BoxGameObject.cpp` 的订阅者改为空；`FireBall.cpp` 的 `handleCollision` 只留水平碰撞爆炸判定。
  - **不参与求解器的对象**：`Mario`（玩家角色，保留自己的事件物理与手感）、`Box`（问号箱，自身有 last_y 复位逻辑）、`FireBall`（火球，有保证最低弹跳速度 `fireballSpeedY` + 水平碰撞爆炸的特殊逻辑）——在各自构造里 `setInvMass(0.f)` 声明"不参与求解器物理"（无穷质量，冲量/位置修正全部分给对方，自己不被求解器动）。不再用类名写死（原 `isSolverExcluded` 已删除）。
  - **回弹区分**：与静态物体（地面/墙）碰撞恢复系数 `BOUNCE_FACTOR_STATIC = 0.28f`（球落地弹跳）；移动物体之间恢复系数 `BOUNCE_FACTOR_MOVING = 0.1f`（抑制堆叠振荡）。冲量模型：分离速度 = e × 接近速度，`impulse = -(1+e) × v_rel_n / (invMa+invMb)`（质量相等假设下双方可动为 2、一方静止为 1；初版漏除分母导致双方可动时实际恢复系数变成 1+2e，球-球"一碰就超级快"；另初版误用 `-e × v_rel_n` 导致无法反向弹起，均已修正）。
  - 调优记录：B1 初版用统一 0.1 回弹导致球/火球弹不起来、求解器介入 Box 导致"慢慢上天"、介入 Mario 导致跳跃擦到水平物体时左右动不了、介入 FireBall 破坏"最低弹跳"设计，均已按上述规则修复（回弹系数 0.5 → 0.28 适配 demo 球弹性）。
  - 网络旁观端修复：`Mario::applyRemoteNetworkSmoothing` 中，远端玩家的**碰撞判定改用服务器权威位置**（`networkTargetPosition`），视觉位置保持平滑——否则旁观端看到其他玩家顶箱子/撞墙时，远端角色因平滑滞后（约 30px+）未到达碰撞位置，本地碰撞响应（如 Box 被顶起）不触发，与服务器判定不一致。Box 本身不需要状态同步（两端各自模拟）。
  - 迭代求解让相互耦合的接触（堆叠）逐步收敛，消除"处理顺序依赖 + 快照过期"导致的振荡。
- [x] **完成标准**：demo 叠球稳定且球落地能弹；SuperMarioScene 中 Mario 手感、问号箱、火球行为与阶段 A 一致或更好；`checkCollisions` 不再直接调 handle（物理响应统一在求解器）。

#### B2 逆质量 + 动量守恒冲量

- [x] **改动**
  - `src/GameObjects/GameObject.h`：新增 `float invMass{1.f}` 字段（0 = 无穷质量）+ `getInvMass()/setInvMass()`；与 `moveAble` 并存（求解器组合判断，`moveAble=false` 一律按 invMass=0 处理）。
  - `src/CollisionSystem.cpp`：`invMassOf()` 只检查 `moveAble=false → 0`；Mario/Box/FireBall 通过构造 `setInvMass(0)` 声明不参与（删除原 `isSolverExcluded` 类名写死，改为 GameObject 属性表达）。
  - 冲量公式升级为动量守恒模型：`j = -(1+e) × v_rel_n / (invMa + invMb)`，速度按逆质量分配（`va += normal·j·invMa`、`vb -= normal·j·invMb`），分离速度 = e × 接近速度。静止接触改为 e=0 冲量消除相对法向速度（完全非弹性）。
  - 位置修正按逆质量比例分摊（`penetration × invM/(invMa+invMb)`）；质量相等时与 A1 的"各推一半"行为一致，不同质量对象自动按质量分摊。
  - 注：slop/修正上限不引入（A4 已决定全量修正最快收敛）。
- [x] **完成标准**：碰撞后速度变化符合动量守恒直觉（恢复系数 e 精确生效，无超弹性）；静态物体纹丝不动（invMass=0）；两球碰撞速度交换合理；demo 叠球与 SuperMarioScene 行为与 B1 一致或更好。

#### B3 统一碰撞体坐标语义

- [x] **改动**
  - `src/Components/Collisions/Collision.h`：明确 `getCollisionPosition()` 统一返回**碰撞体左上角（含 offset）**（Box = 左上角，Circle = 包围盒左上角），并在注释注明；`getPosition()` 标注为"内部存储位置，不建议外部使用"。
  - 新增 `getCenter()` 虚函数（基类默认 = 左上角）：`BoxCollision::getCenter()` = 左上角 + 半尺寸；`CircleCollision::getCenter()` = 圆心。
  - `src/CollisionSystem.cpp`：求解器几何（circle-circle / circle-box / box-box）统一通过 `getCenter()`/`getCollisionPosition()` 取形状几何，不再出现"碰撞位置 + GameObject size"拼接推算。
  - 死代码 handle（`CollisionHandle.cpp`/`CircleCollisionHandle.cpp` 的"左上角 + size×0.5"推算）保留，B6 清理。
- [x] **完成标准**：带 offset 的碰撞体（如跑步/跳跃状态的 Mario，x 偏移 12/16px）也能正确检测与解析——求解器几何直接使用碰撞体自身几何（含 offset），不受 GameObject 位置/尺寸推算影响。

#### B4 broad-phase（空间哈希）

- [x] **改动**
  - `src/CollisionSystem.h`：新增空间哈希网格（`grid`：cell 坐标 → 对象列表）+ `queryAABB()` 空间查询接口；`Collision` 基类新增 `getSize()` 虚函数（Box = (w,h)，Circle = 包围盒 (2r,2r)）。
  - `src/CollisionSystem.cpp`：
    - `buildBroadPhase()`：对象按碰撞体 **AABB 覆盖的所有 cell** 登记（大对象登记多个 cell），O(总覆盖 cell 数)。
    - `collectPairs()`：每个 cell 内 `i<j` 配对 + 规范化 id 对全局去重（`unordered_set`），保证每对恰好检查一次。
    - 检测/求解/事件流程不变，仅候选对来源改为网格。
  - `Circle.cpp` / `Mario.cpp` 的 `needGravity()` 探针改用 `queryAABB()` 查询探针区域 cell 内的候选对象。
  - 调优记录：初版按"中心 cell + 3x3 邻域"登记，导致超大 AABB 对象（地面/墙，如 120000px 宽）与远处对象不在彼此邻域内而**漏检穿墙**；改为"登记到 AABB 覆盖的所有 cell"，任意可能碰撞对必然共享 cell，无邻域漏检。
- [x] **完成标准**：100 个球时帧率不低于改造前 30 个球的帧率；地面/墙等大对象不漏检（不穿墙）。

#### B5 物理参数配置化

- [x] **改动**
  - `src/Manager/ConfigManager.h`：`GameConfig` 新增 `solverIterations`（迭代轮数）、`restingSpeedThreshold`（接触静止阈值）、`restitutionStatic`（撞静态恢复系数）、`restitutionMoving`（移动-移动恢复系数）。
  - `src/Manager/ConfigManager.cpp`：`parseGame()` 读取 + `save()` 写回这 4 个字段。
  - `src/Asset/config.json`：`game` 段新增默认值（4 / 40.0 / 0.28 / 0.1）。
  - `src/CollisionSystem.cpp`：删除 `SOLVER_ITERATIONS`/`RESTING_SPEED_THRESHOLD`/`BOUNCE_FACTOR_STATIC`/`BOUNCE_FACTOR_MOVING` 常量，改用 `CONFIG.game.*`（include `ConfigManager.h`）。`EPSILON` 为数值稳定性阈值，不配置。
  - 注：计划中的 `damping`/`slop`/`maxCorrection` 未引入——当前模型采用"接触静止 e=0 冲量吸收"（等效阻尼）与"A4 全量位置修正"（无 slop/上限），无需独立参数。
- [x] **完成标准**：改 `config.json` 中 `restitutionStatic`/`restingSpeedThreshold` 等能直接改变 demo 中球的回弹/静止行为（无需重编译）。

#### B6 删除重复逻辑并回归

- [x] **改动**
  - **删除 `CollisionHandle` / `CircleCollisionHandle` / `BoxCollisionHandle` 三个类**（`src/Components/CollisionHandles/` 6 个文件）：B1 后物理响应统一到求解器，事件订阅者不再调用它们，全部为死代码（已确认无任何调用者）。
  - 清理 `Circle.cpp` / `Player.cpp` / `BoxGameObject.cpp` 的 `addComponent<CollisionHandle, ...>()` 与相关 include；`Box.cpp` 的 `removeComponent<CollisionHandle>()`；`Mario.cpp` 的注释引用。
  - `Mario::handleCollision` 与 `FireBall::handleCollision` **保留**（各自承载游戏逻辑：伤害/状态/爆炸/弹跳，且 Mario 被求解器排除、物理由其事件处理保持手感）。
- [x] **完成标准**：SuperMarioScene 与 demo 行为与清理前一致；编译无未定义引用。

### 阶段 C：收尾清理

#### C1 b 侧空指针判空

- [ ] **改动**
  - `src/CollisionSystem.cpp:44`：改为 `if (auto b_c = ...; b_c && b_c->getActive() && a_c->checkCollision(*b_c))`。
- [ ] **完成标准**：往 CollisionSystem 加入无 Collision 组件的对象不再崩溃。

#### C2 typeid hash 分发改显式分发

- [ ] **改动**
  - `src/Components/CollisionHandles/CollisionHandle.cpp:15-22`：`collisionHandlers` 的 `typeid().hash_code()` 查表改为显式判断（`dynamic_cast` / 枚举类型标记），或直接用虚函数双分派，去掉 hash 查表。
- [ ] **完成标准**：行为不变，无 hash 冲突隐患；b 无 Collision 组件时安全返回。

#### C3 删除 needGravity 探针写法

- [ ] **改动**
  - `src/GameObjects/Circle.cpp:61-79`、`src/GameObjects/Mario.cpp:112-131`：删除“碰撞体下移 1px 探测”写法，改为查询求解器产出的接触集合（是否有接触 + 是否静止）。
- [ ] **完成标准**：`needGravity` 不再修改碰撞体位置；行为与阶段 B 一致。

#### C4 CircleCollision::setPosition 参数问题

- [ ] **改动**
  - `src/Components/Collisions/CircleCollision.cpp:35-37`：`setPosition` 真正使用传入参数（或删除参数、明确语义），与基类接口一致。
- [ ] **完成标准**：无“忽略参数”的误导接口；代码审查无歧义。

---

## 五、优先级与提交建议

1. **阶段 A（A1→A6）**：按顺序执行，每步独立可测，建议每步一个 commit（如 `fix(collision): 每对碰撞只处理一次`）。
2. **阶段 B（B1→B6）**：结构性重构，建议单独分支/单独提交，逐项验证，避免一次改动过大。
3. **阶段 C（C1→C4）**：低风险清理，可与阶段 B 穿插。

## 六、验证方式

- demo（`GameScene`）：鼠标快速连点生成 10~20 个球堆叠，30s 观察无乱跳/瞬移/穿地；开启 `game.debug` 查看碰撞框与速度箭头（`MoveComponent::render`）。
- SuperMarioScene：回归测试 Mario 落地/跳跃/撞墙手感（Mario 走 box 路径，阶段 A 不受影响，B1/B2/B6 重构后需重点回归）。
- 单元验证（可选）：对 `dir_len==0`、静止接触、单对碰撞只处理一次各写最小测试。
