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
| | A4 位置修正限幅（比例修正） | ✅ |
| | A5 统一重力/静止判定（移除事件开关） | ⬜ |
| | A6 编译并验证 demo 叠球 | ⬜ |
| 阶段 B 重构 | B1 Contact 结构 + 迭代求解器 | ⬜ |
| | B2 逆质量 + 动量守恒冲量 | ⬜ |
| | B3 统一碰撞体坐标语义 | ⬜ |
| | B4 broad-phase（空间哈希） | ⬜ |
| | B5 物理参数配置化 | ⬜ |
| | B6 删除 Mario 重复逻辑并回归 | ⬜ |
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

- [ ] **改动**
  - `src/GameObjects/Circle.cpp:48`：删除碰撞事件订阅者里的 `this->getComponent<GravityComponent>()->setActive(false);`，重力是否“生效”由 A2 的接触法向速度清零来抵消，避免三套逻辑打架。
  - 验证 `Circle::needGravity()` 在删除后行为正常（球静止时探针探测到下方物体 → 重力关闭；若 A2 已保证静止，重力开着也不会积累速度）。
- [ ] **完成标准**：球静止在地面/其它球上时速度保持为 0，无重力累加导致的缓慢下沉或弹跳。

#### A6 编译并验证 demo 叠球

- [ ] **改动**
  - 无代码改动；编译运行，在 `GameScene` 快速点击生成 10~20 个球堆叠，观察 30s。
- [ ] **完成标准**：无乱跳/瞬移/穿地/持续抖动；球最终静止堆叠；开启 `game.debug` 可看到碰撞框与速度箭头正常。

### 阶段 B：中期重构（统一物理求解）

#### B1 Contact 结构 + 迭代求解器

- [ ] **改动**
  - 新增 `Contact { a, b, normal, penetration, relativeVelocity }` 结构（可放 `CollisionSystem.h` 或新建 `PhysicsSolver.h`）。
  - `CollisionSystem::checkCollisions()` 检测阶段只负责产出 `std::vector<Contact>`（检测与响应分离）；响应交给求解器对全部 Contact 做 **2~4 次迭代**的“速度冲量 → 位置修正”两步求解，消除快照过期与顺序敏感问题。
  - 保留/简化事件发布（游戏逻辑如 Mario 受伤仍可用事件，物理响应不再走事件）。
- [ ] **完成标准**：demo 与 SuperMarioScene 行为与阶段 A 一致或更好；`checkCollisions` 不再直接调 handle。

#### B2 逆质量 + 动量守恒冲量

- [ ] **改动**
  - `src/GameObjects/GameObject.h`：用 `float invMass`（`moveAble=false` → `invMass=0`）替代/补充 bool `moveAble`。
  - 求解器冲量公式：`j = -(1 + e) * v_rel_n / (invMassA + invMassB)`，速度按质量分配更新；位置修正按 `invMass` 比例分配，加 `slop`（如 `0.05f`）与修正上限。
- [ ] **完成标准**：碰撞后速度变化符合动量守恒直觉；静态物体纹丝不动；两球碰撞速度交换合理。

#### B3 统一碰撞体坐标语义

- [ ] **改动**
  - 明确 `getCollisionPosition()` 统一返回碰撞体**左上角**（保留 `CircleCollision` 减半径行为，基类与派生类语义一致并在注释注明）。
  - 或提供 `getCenter()`/`getShape()` 供求解器使用；`Contact` 里直接携带 center + normal + penetration，事件不再靠“左上角 + GameObject size”拼。
  - 检查 `CollisionHandle.cpp:33-38`、`CircleCollisionHandle.cpp:33-34` 的推算逻辑改为使用统一语义。
- [ ] **完成标准**：带 offset 的碰撞体也能正确检测与解析（可加一个带 offset 的测试对象验证）。

#### B4 broad-phase（空间哈希）

- [ ] **改动**
  - 在 `CollisionSystem` 内按对象中心分桶（cell 尺寸可取最大对象直径），每帧只检测同桶及邻桶对象，替代全对遍历。
  - `needGravity` 探测复用同一份 broad-phase 查询（或阶段 C 删除探针后由接触集合替代）。
- [ ] **完成标准**：100 个球时帧率不低于改造前 30 个球的帧率。

#### B5 物理参数配置化

- [ ] **改动**
  - `ConfigManager` 增加字段：`restitution`（恢复系数）、`damping`（接触阻尼）、`slop`、`maxCorrection`、`restingSpeedThreshold`。
  - 替换各 handle 与求解器中的魔法数字（`0.28f`、`2.f`、`150.f`、`0.1f`、`20.f`、`2.f`）。
- [ ] **完成标准**：改 `config.json` 中物理参数能直接改变 demo 中球的回弹/静止行为。

#### B6 删除 Mario 重复逻辑并回归

- [ ] **改动**
  - `src/GameObjects/Mario.cpp:133-205`：删除 `handleCollision` 里重复的 box 解析逻辑，统一走 B1/B2 的求解器；Mario 只保留游戏逻辑（FireBall 伤害、状态切换、落地时重力关闭等）。
  - `CollisionHandle::handle` 与 `CircleCollisionHandle` 的 box 路径同样收敛到求解器。
- [ ] **完成标准**：SuperMarioScene 完整回归：移动、跳跃、落地、撞墙、踩怪/被怪碰手感与修复前一致或更好。

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
