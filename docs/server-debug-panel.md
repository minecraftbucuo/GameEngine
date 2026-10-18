# 服务端 Web 调试面板 — 使用说明

> 适用构建：`BUILD_FOR_SERVER=ON`（产物 `build-server/server/GameEngineServer.exe`）
> 客户端与 WASM 构建不含此功能，零改动、零开销
> 设计与实施记录见 `server-debug-panel-plan.md`

---

## 一、它是什么

服务端除了跑游戏服务，额外起一个 HTTP 服务，用浏览器就能看到**内存里真实的游戏对象状态**。

解决的是「改完代码不知道运行时对不对」：不用加 printf、不用断点、不用等复现，
打开页面就能看到每个对象的位置、速度、组件、以及 Mario/Goomba/Koopa 的私有状态。

**只读**：面板不提供任何修改游戏状态或下发指令的接口，看到的就是当时的真实快照。

---

## 二、快速开始

### 2.1 启动

正常启动服务端即可，配置里 `debugPanel.enabled` 为 `true` 时面板自动起来：

```bash
cd build-server/server
./GameEngineServer.exe
```

控制台会打印两行：

```
Debug panel: http://0.0.0.0:8090/  token=4073203954180a2a
Debug panel: 10Hz, maxObjects=2000, idleSkip=5s (skip building while nobody watches)
```

### 2.2 打开页面

浏览器访问 `http://<服务器IP>:8090/`，把控制台打印的 token 粘进去。

token 只存在 `sessionStorage`，关标签页即失效，不进 URL、不进浏览器历史。

### 2.3 只想用 curl

```bash
# 健康检查
curl -H "X-Panel-Token: <token>" http://127.0.0.1:8090/api/health

# 全量快照（人眼看加 pretty=1）
curl -H "X-Panel-Token: <token>" "http://127.0.0.1:8090/api/snapshot?pretty=1" | less

# 只看某类对象
curl -s -H "X-Panel-Token: <token>" http://127.0.0.1:8090/api/snapshot \
  | jq '.objects[] | select(.class=="Koopa") | {id, x, y, typeData}'
```

`?token=` 查询参数也认，方便临时用，但**不推荐**——token 会留在 URL 里。

---

## 三、配置

### 3.1 `Asset/config.json`

```json
"debugPanel": {
    "enabled": true,
    "host": "0.0.0.0",
    "port": 8090,
    "token": "",
    "snapshotHz": 10,
    "maxObjects": 2000,
    "idleSkipSeconds": 5,
    "webRoot": ""
}
```

| 字段 | 说明 |
|---|---|
| `enabled` | 默认 `false`。只有显式写 `true` 才起 HTTP 服务 |
| `host` | `0.0.0.0` 表示局域网可访问；只想本机看就填 `127.0.0.1` |
| `port` | 默认 8090。被占用时面板启动失败但**游戏服务照常运行** |
| `token` | 留空则每次启动随机生成并打印；填了就固定 |
| `snapshotHz` | 快照采样频率，夹到 1–60 |
| `maxObjects` | 单次快照最多导出多少个对象，夹到 1–100000 |
| `idleSkipSeconds` | 超过这么久没有 HTTP 请求就完全停止构建快照；`0` 表示不跳采 |
| `webRoot` | 前端目录，留空按约定顺序自动解析 |

### 3.2 环境变量（优先级高于 config.json）

改配置再重编译有时不生效（`Asset/config.json` 的拷贝挂在 `PRE_BUILD`，
MinGW 下目标已最新时不执行），临时调试用环境变量更省事：

| 变量 | 等价字段 |
|---|---|
| `MARIO_PANEL_ENABLE` | `enabled`，接受 `1/0/true/false/on/off/yes/no` |
| `MARIO_PANEL_HOST` | `host` |
| `MARIO_PANEL_PORT` | `port` |
| `MARIO_PANEL_TOKEN` | `token` |
| `MARIO_PANEL_HZ` | `snapshotHz` |
| `MARIO_PANEL_IDLE_SKIP` | `idleSkipSeconds` |
| `MARIO_PANEL_WEBROOT` | `webRoot` |

```bash
# 临时换个端口、降到 2Hz，不动配置文件
MARIO_PANEL_PORT=8091 MARIO_PANEL_HZ=2 ./GameEngineServer.exe
```

非法值（`MARIO_PANEL_HZ=abc`）只打一条 WARN 然后忽略，保留原值，**不会导致服务起不来**。

---

## 四、页面怎么看

### 4.1 顶部卡片

| 卡片 | 含义 |
|---|---|
| 对象总数 | 当前场景对象数（受 `maxObjects` 截断，截断时 `meta.truncated=true`） |
| 可疑 | 命中下面「可疑判定」任一条件的对象数 |
| 非有限值 | 坐标/速度/旋转里出现 NaN 或 Inf 的对象数。**非 0 就是出 bug 了** |
| 平均帧耗时 | 主循环的指数平均帧时长 |
| 快照频率 | 实际生效的采样频率（可能被自动降级改写，见 §7.2） |
| 物理 | 物理子系统未接入时显示「未启用」 |
| 类型分布 | 按 class 统计的对象数 |

### 4.2 客户端区块

有客户端连接时展开成小表：

| 列 | 说明 |
|---|---|
| 地址 | `IP:端口` |
| 状态 | 已连接/已断开 · 已验证/未验证 |
| playerId | 未通过验证时显示 `—` |
| 角色 | 该连接对应的游戏对象类名 |
| 上次收包 | 距上一次收到完整帧多久。`从未` = 一个包都没收到 |
| 收包数 / 字节数 | 累计值 |

**`上次收包` 是最有用的信号**：TCP 连接还活着但对方进程卡死/暂停时，
这个值会一直涨，而行还是「已连接」。超过 2 秒会标黄。

### 4.3 对象表

| 列 | 说明 |
|---|---|
| 状态 | `ok` 或异常标记组合：`已销毁` `未start` `inactive` `卡住` `NaN` |
| 类型状态 | Mario 血量/大小/是否变身中；Goomba 活着/被踩扁/炮弹击毙；Koopa 行走/静止壳/滑行壳 |
| x / y | 左上角坐标（与 `Asset/maps/*.json` 里的坐标系一致） |
| vx / vy | 当前速度 |
| 组件 | Move / Gravity / HP / Box / Circle |
| net | 网络 id，静态对象（砖块/地面/箱子）为 `—` |

### 4.4 可疑判定

一个对象命中以下任一条件就进「只看可疑」：

| 条件 | 含义 |
|---|---|
| `!started` | 对象已创建但 `start()` 没跑过 |
| `!active` | 被置为 inactive，不参与更新 |
| `!finite` | 数值里出现 NaN/Inf |
| `destroyed` | 已标记销毁但还留在列表里 |
| **卡住** | 速度绝对值 > 1，但两次快照（500ms）之间位移 < 0.01 |

「卡住」是排查「敌人不动了」最直接的一条：物理上它想走，实际上没走。

---

## 五、API 参考

所有 `/api/*` 接口都要求 `X-Panel-Token` 请求头（或 `?token=`），否则返回 `401`。

### 5.1 `GET /api/health`

```json
{
  "ok": true,
  "panelVersion": "1.0.0",
  "schemaVersion": 1,
  "uptimeSec": 41.94,
  "frames": 2682,
  "port": 8091,
  "snapshotHz": 2,
  "effectiveHz": 2,
  "idleSkips": 67,
  "idleSkipSeconds": 3,
  "buildCostUs": 497,
  "snapshotAgeMs": 31
}
```

`snapshotHz` 是配置值，`effectiveHz` 是实际生效值（自动降级后两者会不一致）。
`idleSkips` 累计因空闲跳过的构建次数，`snapshotAgeMs` 是当前快照的年龄。

### 5.2 `GET /api/snapshot`

四段结构：

```json
{
  "meta":    { "seq": 70, "frame": 5551, "objectCount": 91, "truncated": false, ... },
  "objects": [ { "id": 82, "class": "Goomba", "x": 1373.0, "typeData": {...}, ... } ],
  "physics": { "enabled": false, "bodyCount": 0, ... },
  "clients": { "count": 0, "unverified": 0, "list": [] }
}
```

加 `?pretty=1` 返回缩进版（约 1.6 倍体积，只给人眼看）。

**`503 snapshot_not_ready`** 表示第一份快照还没产出，正常启动后立刻就有。

### 5.3 `typeData` 字段

只输出对应类型的字段，不认识的类为 `null`。

| kind | 字段 |
|---|---|
| `Mario` | `health` `isBig` `isPlayer` `transforming` |
| `Goomba` | `squashed` `killedByFireball` |
| `Koopa` | `state`（`Walking`/`ShellIdle`/`ShellMoving`）`stateCode` `shellMoving` `killed` |

### 5.4 `physics` 字段

`enabled` 为 `false` 表示场景没有创建 `b2World`（当前工程 `usePhysics` 从未置 `true`，
所以正常情况下恒为 `false`，其余字段全 0 —— 这是**真实状态，不是采集失败**）。
接入物理后会自动出现 body 分类、唤醒/睡眠数、接触对数和坐标非有限的 body 数。

---

## 六、常见排查路径

| 现象 | 看哪里 |
|---|---|
| 敌人不动 | 「只看可疑」勾上 → 命中「卡住」的对象就是它 |
| 对象凭空消失 | 按 class 过滤，看是否 `destroyed=true` 还留在列表 |
| 位置明显不对 | 对照 `Asset/maps/level_1.json` 里的原始坐标 |
| 坐标变 NaN | 「非有限值」卡片 > 0，表格里洋红高亮 |
| 客户端连上但没反应 | 客户端区块看 `上次收包` 是否一直涨 |
| 客户端列表是空的 | 确认 `clients.unverified` 是否 > 0（握手没过） |
| 改前端没生效 | 面板日志里有实际解析到的 `web root`；静态资源全部 `no-store`，刷新即可 |
| 面板起不来 | 日志有 `bind failed on ...`，多半端口被占。游戏服务不受影响 |
| 忘了 token | 重启服务端看日志；或设 `MARIO_PANEL_TOKEN` 固定下来 |

---

## 七、行为细节

### 7.1 空闲跳采

`idleSkipSeconds` 内没有任何 HTTP 请求时，主循环**完全不构建快照**，
省掉遍历全部对象 + 序列化的开销（默认约 0.5ms / 次）。

有人访问时最多等一个采样周期（10Hz 下 100ms）就能拿到新数据，
所以不会出现「打开页面看到很久以前的数据」。

第一份快照一定会构建，因此首次访问不会拿到 503。

### 7.2 开销过高自动降级

当单次构建耗时**超过半个帧预算**（`avgFrameMs × 0.5`）并连续发生 3 次，
采样频率自动减半，最低降到 2Hz，并在日志里打一条 WARN：

```
Debug panel: snapshot build takes 9.30ms on a 16.00ms frame, auto-degraded to 5Hz (was 10Hz)
```

判据用「占帧预算的比例」而不是绝对毫秒数——机器慢、帧长的时候，同样 3ms 并不算贵。

**只降不升**：升回去会在临界点来回抖，收益不抵复杂度。要恢复就重启。

### 7.3 端口冲突

Windows 上 `SO_REUSEADDR` 的语义与 Linux 相反：它允许绑定到已被监听的端口，
于是第二个实例能「绑定成功」但连接仍然路由到第一个实例——冲突被静默吞掉，
表现为「面板启动了但数据是旧的」。

面板已显式关闭该选项，所以端口被占时会明确报 `bind failed` 并放弃启动。

> ⚠️ 注意：**游戏服务本身的 6666 端口仍有这个问题**，第二个实例会打印
> `Server started successfully` 但抢不到连接。属既有缺陷，不在面板范围内。

### 7.4 前端目录解析顺序

基准是 exe 所在目录（`GameEngine::init` 已把 cwd 设过去）：

1. `debugPanel.webRoot` 配置值（存在才用）
2. `../../web/debug` —— 源码树，开发时命中，改完刷新即生效，不用重编译
3. `./web/debug` —— CMake 构建时拷贝的部署副本

都找不到时 HTTP 服务仍然起来，但 `/` 返回一个说明页。

---

## 八、已知限制

- **只读**：不能改游戏状态、不能下发指令
- **只保留当前帧**：没有历史曲线，看不到趋势；要对比就靠「卡住」检测的两帧比对
- **对象表最多 `maxObjects` 条**：超出时 `meta.truncated=true`，前端会显示实际总数
- **`HealthBar` 没有 `getMaxHealth()`**：Mario 只显示当前血量，不显示上限
- **物理统计当前恒为空**：物理子系统未接入，见 §5.4
- **客户端列表只在服务端有意义**：客户端构建不含此功能
