# 设置界面方案

## 目标
新建 `SettingsScene`，让用户在游戏内查看和修改 `config.json` 的配置项，修改后保存到文件并生效。

## 可编辑配置项

从 `ConfigManager` 中选取用户真正关心的字段：

| 分组 | 字段 | 类型 | 说明 |
|------|------|------|------|
| **窗口** | window.width | int | 窗口宽度 |
| | window.height | int | 窗口高度 |
| | window.fps | int | 帧率上限 |
| **网络** | network.serverIp | string | 服务器 IP |
| | network.port | int | 服务器端口 |
| | network.tickRate | int | 网络帧率 |
| **游戏** | game.gravity | float | 重力 |
| | game.playerSpeed | float | 玩家速度 |
| | game.jumpForce | float | 跳跃力度 |
| | game.debug | bool | 调试模式 |

> assets 路径类配置不暴露给用户，避免改错导致崩溃。

## UI 布局

```
+------------------------------------------+
|              设置 (标题)                   |
|                                          |
|  ── 窗口 ──────────────────────────────  |
|  窗口宽度   [  1200  ]                    |
|  窗口高度   [   960  ]                    |
|  帧率上限   [   165  ]                    |
|                                          |
|  ── 网络 ──────────────────────────────  |
|  服务器 IP  [ 127.0.0.1 ]                 |
|  端口      [  6666  ]                    |
|  网络帧率   [   128  ]                    |
|                                          |
|  ── 游戏 ──────────────────────────────  |
|  重力      [ 3200.0 ]                    |
|  玩家速度   [ 500.0 ]                    |
|  跳跃力度   [ 900.0 ]                    |
|  调试模式   [ 开/关 ]                     |
|                                          |
|       [ 保存 ]    [ 返回 ]               |
+------------------------------------------+
```

- 左侧标签，右侧可编辑输入框，分组显示
- 底部两个按钮：保存 / 返回
- 背景复用 MenuScene 的粒子效果

## 需要新建的 UI 组件

### 1. TextInput（文本输入框）
- 点击激活，键盘输入字符
- 支持数字、小数点、IP 地址字符
- 有光标闪烁效果
- 失焦时结束编辑

### 2. Toggle（开关）
- 点击切换开/关
- 用于 bool 类型配置项（如 debug）

### 3. SettingRow（配置行）
- 一行 = 标签 + 输入控件（TextInput 或 Toggle）
- 内部持有字段路径和当前值

## 实现步骤

### Step 1：ConfigManager 增加 save() 方法
- 把当前 CONFIG 的各字段序列化为 JSON 写回 `config.json`
- 目前只有 `load()`，需要补 `save()`

### Step 2：实现 TextInput 组件
- 继承 GameObject
- 状态：idle / focused
- 绘制：圆角矩形背景 + 文字 + 光标
- handleEvent：点击聚焦、键盘输入、退格删除、回车确认

### Step 3：实现 Toggle 组件
- 继承 GameObject
- 绘制：滑块轨道 + 圆形滑块
- 点击切换状态

### Step 4：实现 SettingsScene
- 继承 Scene
- initScene：按分组创建 SettingRow
- 保存按钮：调用 CONFIG.save()
- 返回按钮：回到 MenuScene
- 背景粒子效果

### Step 5：接入 MenuScene
- 设置按钮的 onClick 改为 `getSceneManager()->loadScene("SettingsScene")`
- 在 GameEngine 初始化时注册 SettingsScene

## 文件清单

| 文件 | 操作 |
|------|------|
| `src/GameObjects/TextInput.h/.cpp` | 新建 |
| `src/GameObjects/Toggle.h/.cpp` | 新建 |
| `src/Scene/SettingsScene.h/.cpp` | 新建 |
| `src/Manager/ConfigManager.h/.cpp` | 修改，加 save() |
| `src/Scene/MenuScene.cpp` | 修改，设置按钮接入 |
| `src/GameEngine.cpp` | 修改，注册 SettingsScene |
