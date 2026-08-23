# SDL3 迁移计划 — 渲染/窗口后端抽象层

## 一、目标

将引擎的**窗口 / 渲染 / 输入 / 音频**从 SFML 迁移到 SDL3，方式是先抽出一层引擎自有的接口，
SFML 与 SDL3 各写一份实现，用 CMake 选项切换：

- **游戏层代码（场景、对象、组件、玩法）迁移时零改动**——只依赖引擎自己的类型和接口；
- 抽象层本身立即落地：SFML 成为第一个后端实现，**做完抽象层后行为与现在完全一致**；
- 顺带解决 Web 移植障碍：SFML 2.6 / 3.x 均不支持 Emscripten，SDL3 官方支持，
  将来可用 Emscripten + SDL3 编译到浏览器（2026-08-22 会话结论）。

**范围边界**：
- 本次只抽象 **Window / Graphics / System / Audio** 四个耦合面；
- **SFML Network 模块（`sf::Packet` / `TcpClient` / `NetworkManager`）不在范围内**——
  `sfml-network` 是独立库，不依赖窗口/图形，SDL3 后端下可继续链接使用，将来如需再单独迁移；
- 与 `SERVER_BUILD` 正交：接口层是纯类型定义，服务端构建不受影响。

---

## 二、现状分析

`sf::` 全仓引用统计：**100 个文件、783 处**（src/ 下，不含 lib/）。按耦合性质分类：

### 1. 耦合面分类

| 耦合面 | sf:: 类型 | 主要位置 | 迁移难度 |
|--------|-----------|----------|----------|
| 数值类型 | `Vector2f/i/u`、`Time`、`Color`、`IntRect` | 几乎所有文件（GameObject 的 position/size/speed 等） | 低（别名过渡） |
| 渲染链签名 | `sf::RenderWindow*` | [Scene.h](file:///e:/Projects/GameEngine/src/Scene/Scene.h)、[GameObject.h](file:///e:/Projects/GameEngine/src/GameObjects/GameObject.h)、[Component.h](file:///e:/Projects/GameEngine/src/Components/Component.h)、[SceneManager.h](file:///e:/Projects/GameEngine/src/Manager/SceneManager.h)、[GameEngine.h](file:///e:/Projects/GameEngine/src/GameEngine.h) | 中（机械替换签名） |
| 事件链签名 | `sf::Event&` | 同上 + [Camera.h](file:///e:/Projects/GameEngine/src/Camera.h) | 中（需定义 EngineEvent） |
| 资源对象 | `Texture`、`Font`、`SoundBuffer` | [AssetManager.h](file:///e:/Projects/GameEngine/src/Manager/AssetManager.h)、[Animation.h](file:///e:/Projects/GameEngine/src/Animation.h) | 中（句柄化） |
| 相机 | `sf::View`、`sf::FloatRect` | [Camera.h](file:///e:/Projects/GameEngine/src/Camera.h) | 低（藏进实现内部） |
| 直接绘制调用 | `window->draw(...)` | 20 个文件、55 处（见下表） | 中（改为 draw 命令） |
| 网络（范围外） | `sf::Packet`、`sf::TcpSocket` | [Network/](file:///e:/Projects/GameEngine/src/Network) | 本次不动 |

### 2. 直接绘制调用点（55 处 / 20 文件，阶段二 Step 6 逐个改造）

| 文件 | draw 调用 | 绘制内容 |
|------|-----------|----------|
| [PhysicsDebugDraw.cpp](file:///e:/Projects/GameEngine/src/Physics/PhysicsDebugDraw.cpp) | 13 | 线/多边形/圆（b2Draw → 绘制命令，天然匹配） |
| [PhysicsTestScene.cpp](file:///e:/Projects/GameEngine/src/Scene/PhysicsTestScene.cpp) | 5 | 矩形（地面/斜面/方块轮廓） |
| [Button.cpp](file:///e:/Projects/GameEngine/src/GameObjects/Button.cpp) | 6 | 文字 + 九宫格矩形 |
| [SuperMarioScene.cpp](file:///e:/Projects/GameEngine/src/Scene/SuperMarioScene.cpp) | 3 | 背景 sprite + 死亡遮罩 + 文字 |
| [Toggle.cpp](file:///e:/Projects/GameEngine/src/GameObjects/Toggle.cpp) | 4 | 圆角矩形拼合 |
| [TextInput.cpp](file:///e:/Projects/GameEngine/src/GameObjects/TextInput.cpp) | 4 | 矩形 + 文字 + 光标线 |
| [MenuScene.cpp](file:///e:/Projects/GameEngine/src/Scene/MenuScene.cpp) | 2 | 粒子矩形 + 标题文字 |
| [SettingsScene.cpp](file:///e:/Projects/GameEngine/src/Scene/SettingsScene.cpp) | 2 | 标题 + 选项文字 |
| Mario 状态机（Idle/Run/Jump/Dead） | 7 | 动画帧 sprite |
| [Animation.cpp](file:///e:/Projects/GameEngine/src/Animation.cpp) | 1 | 动画 sprite |
| [MoveComponent.cpp](file:///e:/Projects/GameEngine/src/Components/MoveComponent.cpp) | 2 | 速度矢量线 + 箭头 |
| [HealthBar.cpp](file:///e:/Projects/GameEngine/src/Components/HealthBar.cpp) | 2 | 背景条 + 前景条 |
| BoxCollision / CircleCollision | 2 | 碰撞盒调试框 |
| [Brick.h](file:///e:/Projects/GameEngine/src/GameObjects/Brick.h)、[Player.h](file:///e:/Projects/GameEngine/src/GameObjects/Player.h)、[Circle.cpp](file:///e:/Projects/GameEngine/src/GameObjects/Circle.cpp) | 3 | sprite / 矩形 / 圆 |
| GameObject3D.cpp | 2 | 3D 投影线框（顶点圆 + 线） |

### 3. 关键观察

- **游戏/玩法逻辑并不真正依赖 SFML**，它们只是在 `update / render / handleEvent` 签名里"路过" sf:: 类型；
- 真正使用 SFML 绘制能力的集中在**叶子代码**（上表 20 个文件）与 4 个基础设施类
  （GameEngine / Camera / AssetManager / Animation）；
- 因此抽象层切在**引擎基类签名**上，叶子代码改为调用绘制命令，其余几十个文件只是机械换类型名。

---

## 三、技术决策

### 1. 抽象"绘制命令"，不模仿 SFML 的类

**不设计 `ISprite` / `IRectangleShape`**。SDL3 没有 Sprite/Shape 对象，只有立即式调用
（`SDL_RenderTexture(renderer, tex, src, dst, angle...)` / `SDL_RenderLine` / `SDL_RenderGeometry`）。
接口按命令式设计，SFML 后端内部用临时 `sf::Sprite`/`sf::RectangleShape` 实现（SFML 的 draw 本来就是即时的，无性能损失），SDL3 后端直接映射。

### 2. 数值类型：先别名、后自研

```cpp
// src/Core/Types.h —— 阶段一：别名，零风险
namespace eng {
    using Vec2f = sf::Vector2f;
    using Vec2i = sf::Vector2i;
    using Vec2u = sf::Vector2u;
    using Time  = sf::Time;
    using Color = sf::Color;
    using IntRect  = sf::IntRect;
    using FloatRect = sf::FloatRect;
}
```

全局把 `sf::Vector2f` 机械替换为 `eng::Vec2f`。到 SDL3 阶段（阶段四）只改这一个头文件，
换成自研 `struct Vec2f { float x, y; }`（保持运算符 API 一致）——因为那时只有后端实现文件接触 sf:: API。

### 3. 事件抽象：EngineEvent + 自有键码枚举

- `enum class Key { A, B, ..., Space, Enter, Escape, Up, Down, Left, Right, LShift, LCtrl, F1... }`；
- `struct EngineEvent`：`type` + 各事件子结构（key / mouseButton / mouseMove / wheel / resize / text / close）；
- 每个后端一张转换表：SFML 后端 `sf::Event → EngineEvent`，SDL3 后端 `SDL_Event → EngineEvent`；
- 转换只发生在 **GameEngine 主循环一个位置**，其余代码只见 EngineEvent。

### 4. 相机必须进渲染接口

SDL3 没有 `sf::View` 等价物。相机进 `IRenderer` 接口后：
- SFML 后端：`setCamera` 映射到 `sf::View`；
- SDL3 后端：保存相机参数，内部给所有绘制命令应用偏移/缩放；
- `screenToWorld`（现用 `mapPixelToCoords`，鼠标放球功能依赖）同样是接口方法。

### 5. 资源句柄化

`TextureHandle` / `FontHandle` 只是 `uint32_t` id，`sf::Texture` 等藏进 AssetManager 内部（pimpl 思想）。
原因：**SDL_Texture 与 SDL_Renderer 绑定**，窗口/渲染器重建时纹理要跟着重建——句柄化后这个差异被 AssetManager 后端吸收，游戏层无感知。

### 6. CMake 双后端开关

```cmake
set(ENGINE_BACKEND "SFML" CACHE STRING "SFML | SDL3")
```

结构上复用现有 `SERVER_BUILD` 条件编译模式：
- `src/Core/`、`src/Render/`（接口层）：无条件编译，不含任何第三方头；
- `src/Platform/SFML/`：`ENGINE_BACKEND=SFML` 时编译；
- `src/Platform/SDL3/`：`ENGINE_BACKEND=SDL3` 时编译（阶段四新增）；
- `sfml-network` 始终链接（范围外）。

### 7. 目录规划

```
src/
  Core/            # Types.h, Event.h, KeyCodes.h —— 纯类型，零第三方依赖
  Render/          # IRenderer.h, Handles.h —— 纯接口
  Platform/
    SFML/          # SFMLRenderer, SFMLEventConverter, SFMLAssetBackend, SFMLAudio
    SDL3/          # （将来）SDL3Renderer, SDL3EventConverter, ...
```

---

## 四、接口草案

```cpp
// src/Render/IRenderer.h
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void clear(eng::Color c) = 0;
    virtual void present() = 0;
    [[nodiscard]] virtual eng::Vec2u getSize() const = 0;

    // ── 绘制命令 ──
    virtual void drawTexture(TextureHandle h, const eng::FloatRect& src, const eng::FloatRect& dst,
                             float rotationDeg = 0.f, eng::Vec2f origin = {},
                             eng::Color tint = eng::Color::White) = 0;
    virtual void drawRect(const eng::FloatRect& r, eng::Color c,
                          bool filled = true, float outlineThickness = 0.f) = 0;
    virtual void drawLine(eng::Vec2f a, eng::Vec2f b, eng::Color c) = 0;
    virtual void drawLines(const std::vector<eng::Vec2f>& points, eng::Color c) = 0;   // 折线/多边形
    virtual void drawCircle(eng::Vec2f center, float radius, eng::Color c, bool filled = true) = 0;
    virtual void drawText(FontHandle h, const std::string& text, eng::Vec2f pos,
                          unsigned size, eng::Color c) = 0;

    // ── 相机 ──
    virtual void setCamera(eng::Vec2f center, eng::Vec2f size, float zoom = 1.f) = 0;
    virtual void resetCamera() = 0;
    [[nodiscard]] virtual eng::Vec2f screenToWorld(eng::Vec2i screenPos) const = 0;
};
```

```cpp
// src/Core/Event.h（节选）
enum class EventType {
    KeyPress, KeyRelease,
    MouseButtonPress, MouseButtonRelease, MouseMove, MouseWheel,
    TextEntered, WindowResize, WindowClose, GainFocus, LostFocus
};
enum class MouseButton { Left, Right, Middle };

struct EngineEvent {
    EventType type;
    Key key = Key::Unknown;              // Key* 事件有效
    MouseButton mouseButton{};           // MouseButton* 事件有效
    eng::Vec2i mousePos;                 // MouseMove / MouseButton* 有效
    float wheelDelta = 0.f;
    char32_t codepoint = 0;              // TextEntered 有效
    eng::Vec2u newSize;                  // WindowResize 有效
};
```

---

## 五、分阶段实施计划

### 原子性原则（每一步必须满足）

> **每一步 = 一次 git commit = 一个原子操作**，必须同时满足：

1. **可编译**：该步完成后 `cmake --build` 通过。
2. **可运行**：程序能启动，所有现有功能行为不变。
3. **可回滚**：`git revert <commit>` 后回到上一步状态，无残留依赖。
4. **单一职责**：一步只做一件事。
5. **不破坏契约**：EventBus `"onCollision"+tag` 等既有契约不变。

完成后将 `[ ]` 改为 `[x]`。

---

### 阶段一：类型与事件抽象（最安全，机械替换为主）

#### Step 1 — Core/Types.h 数值类型别名
- [ ] 新建 `src/Core/Types.h`（内容见技术决策 2）
- [ ] 全局机械替换：`sf::Vector2f → eng::Vec2f` 等 7 个别名
      （**排除**：`src/Network/`、`src/Platform/`——网络范围外；`Physics/` 内 b2Vec2 互转处随替换）
- **验证**：编译通过；所有场景运行表现不变
- **新增文件**：`src/Core/Types.h`
- **改动文件**：约 90 个（纯类型名替换）
- **原子性保证**：别名即原类型，语义零变化，编译器逐处校验

#### Step 2 — EngineEvent + Key 枚举 + SFML 转换器（纯新增）
- [ ] 新建 `src/Core/Event.h`（`EventType` / `MouseButton` / `EngineEvent`）
- [ ] 新建 `src/Core/KeyCodes.h`（`enum class Key`，覆盖现有代码用到的全部按键）
- [ ] 新建 `src/Platform/SFML/SFMLEventConverter.h/.cpp`：
      `toEngineEvent(const sf::Event&) -> std::optional<EngineEvent>`（无关事件返回 nullopt）；
      `sf::Keyboard::Key ↔ Key` 双向映射表
- **验证**：编译通过（尚无人引用）；临时自测转换正确性
- **新增文件**：3 组新文件
- **原子性保证**：纯新增，零引用

#### Step 3 — handleEvent 链路切换
- [ ] `Component / GameObject / Scene / SceneManager / Camera` 的 `handleEvent` 签名：
      `sf::Event& → const EngineEvent&`
- [ ] `GameEngine` 主循环：`pollEvent` 后经转换器分发
- [ ] 所有 `handleEvent` 实现改用 EngineEvent 字段（MarioController、Controller、Camera、
      Button/TextInput/Toggle、各场景，约 15 个文件）
- **验证**：全场景交互测试（A/D 移动、Space 跳、鼠标放球、相机拖动、窗口缩放、文本输入）
- **原子性保证**：类型系统兜底——漏改处必然编译错误，不会静默行为异常

---

### 阶段二：渲染抽象（核心）

#### Step 4 — IRenderer 接口 + 资源句柄类型（纯新增）
- [ ] 新建 `src/Render/Handles.h`（`TextureHandle` / `FontHandle`，`uint32_t` id + `isValid()`）
- [ ] 新建 `src/Render/IRenderer.h`（接口草案见四）
- **验证**：编译通过（无人引用）
- **原子性保证**：纯新增

#### Step 5 — SFMLRenderer 实现（纯新增）
- [ ] 新建 `src/Platform/SFML/SFMLRenderer.h/.cpp`：
      持有 `sf::RenderWindow*`；`drawTexture` 内部用临时 `sf::Sprite`；
      `drawText` 内部用 `sf::Text`；`setCamera` 映射 `sf::View`；
      `screenToWorld` 用 `mapPixelToCoords`
- [ ] `GameEngine` 持有 `std::unique_ptr<IRenderer>`，构造时创建（**暂不接入 render 链路**）
- **验证**：编译通过；现有渲染路径未变
- **原子性保证**：新对象默认闲置

#### Step 6 — render 链路切换（大步骤，按 5 个子 commit 拆分）
- [ ] **6a 基类与主循环**：`Scene/GameObject/Component/SceneManager` 的 render 签名
      `sf::RenderWindow* → IRenderer&`；`GameEngine` 主循环 clear/render/present 走接口；
      `Camera` 内部改用 `IRenderer::setCamera`（去掉 sf::View 成员）；
      `Scene::setCamera/getMousePosition` 同步调整
- [ ] **6b 物理场景**：PhysicsTestScene 5 处 + PhysicsDebugDraw 13 处 → 绘制命令
- [ ] **6c Mario 系**：SuperMarioScene 3 处、4 个 State 共 7 处、Animation、Brick、FireBall、Box
- [ ] **6d UI 场景**：Button 6 处、Toggle 4 处、TextInput 4 处、MenuScene 2 处、SettingsScene 2 处
- [ ] **6e 3D 场景**：GameObject3D 线框（顶点圆 + 线，`drawCircle`/`drawLines` 覆盖）、
      Cube3D/Human3D/Penguin3D/NewModel3D、Player/Circle、碰撞调试框 2 处、MoveComponent 速度箭头、HealthBar
- **验证**：每拆一个子 commit 跑对应场景；全部完成后逐场景核对与迁移前一致
- **原子性保证**：每个子 commit 自洽可编译；同一场景的改动不跨 commit

---

### 阶段三：资源与音频抽象

#### Step 7 — AssetManager 句柄化 + Animation 纯数据化
- [ ] `getTexture/getFont` 返回 `TextureHandle/FontHandle`；sf 对象藏入内部 map
- [ ] `Animation` 去掉 `sf::Sprite` 成员：只存帧矩形序列 + 帧时长（纯数据），
      渲染由持有它的对象调 `drawTexture(handle, frameRect, dstRect, ...)`
- [ ] 各 GameObject 移除 `sf::Sprite` 成员（Mario 状态机里的 `left_sprite/right_sprite` 改为
      每帧计算 src/dst 矩形）
- **验证**：Mario 跑/跳/死动画帧序列与迁移前一致
- **原子性保证**：7a AssetManager（编译器强制改完所有调用点）、7b Animation+使用方

#### Step 8 — IAudio 抽象（可延后，音频面较小）
- [ ] `src/Audio/IAudio.h`：`loadSound/play/stop/setVolume/setLoop` + 流式 Music
- [ ] `src/Platform/SFML/SFMLAudio` 实现（`sf::Sound/SoundBuffer/sf::Music`）
- [ ] 现有音效调用点（SuperMario BGM/音效）改走接口
- **验证**：Mario 场景音乐音效正常

**阶段一~三完成 = 抽象层落地里程碑**：此刻引擎渲染/输入/资源已与 SFML 解耦，
SFML 退缩为一个可整体替换的后端目录，所有场景行为与今天完全一致。

---

### 阶段四：SDL3 后端（将来执行）

#### Step 9 — CMake 双后端 + SDL3 依赖接入
- [ ] `ENGINE_BACKEND` 缓存选项（SFML 默认）；`src/Platform/SDL3/` 源文件按选项加入构建
- [ ] `FetchContent` 拉取 SDL3 + SDL_image + SDL_ttf；`sfml-network` 保持链接
- **验证**：`ENGINE_BACKEND=SFML` 构建行为不变；SDL3 仅拉取不使用

#### Step 10 — SDL3 后端实现
- [ ] `SDL3Renderer`：`SDL_RenderTexture/RenderLine/RenderGeometry`；
      相机 = 内部统一变换（无 View 等价物）；文字 = SDL_ttf + 纹理缓存
- [ ] `SDL3AssetLoader`：SDL_image → SDL_Texture（句柄内部与 renderer 绑定，重建窗口时统一重建）
- [ ] `SDL3EventConverter`：`SDL_Event → EngineEvent` + `SDL_Scancode → Key` 表
- [ ] `GameEngine` 主循环 SDL 化（窗口创建、事件泵、present）；`SDL3Audio`（SDL3 audio stream 模型）

#### Step 11 — 切换验收与收尾
- [ ] `ENGINE_BACKEND=SDL3` 全场景跑通，与 SFML 版逐场景对比（容忍渲染细节的像素级差异）
- [ ] 性能对比（Mario 场景 + PhysicsTestScene 帧率）
- [ ] README 更新；决定 SFML 渲染后端保留（双后端）还是删除（减负）
- [ ] （后续可选）Emscripten 构建目标，验证浏览器运行

---

## 六、SDL3 已知坑（提前记录，阶段四逐条核对）

| # | 坑 | 对策 |
|---|-----|------|
| 1 | `SDL_Texture` 与 `SDL_Renderer` 绑定，渲染器重建纹理全部失效 | 资源句柄化（Step 7），AssetManager 后端集中重建 |
| 2 | SDL_ttf 逐帧 `TTF_Render*` 生成纹理很慢 | 文字纹理缓存（按 font+text+size 做 key，LRU 淘汰） |
| 3 | 无 `sf::View` 等价物 | 相机进 `IRenderer`（Step 4 已定），SDL3 后端内部变换 |
| 4 | 键盘 scancode / keycode 双体系 | 统一用 scancode 映射到 `Key` 枚举（物理键位，游戏习惯） |
| 5 | 鼠标世界坐标（现 `mapPixelToCoords`） | `screenToWorld` 已是接口方法，SDL3 后端自算 |
| 6 | 大量 `RenderLine` 逐条调用慢 | 3D 线框/调试绘制聚合到 `drawLines`，后端用 `SDL_RenderGeometry` 批量 |
| 7 | SDL3 音频是 stream 模型，与 SFML 差异大 | `IAudio` 接口保持薄（play/stop/volume/loop），复杂功能不进接口 |
| 8 | 中文/UTF-8 文字渲染 | SDL_ttf 用 `TTF_Font` + UTF-8 接口，字体文件沿用 Minecraft_AE.ttf 等 |
| 9 | Y-down 坐标系 | SFML/SDL3 一致，Box2D（PPM=64、重力 Y+）完全不受影响 |

---

## 七、验收标准

**阶段一~三（抽象层落地）**：
- [ ] `src/Core/`、`src/Render/` 无任何第三方头文件包含
- [ ] 除 `src/Platform/SFML/`、`src/Network/` 外，全仓 `#include <SFML/...>` 为零
- [ ] 所有场景（Menu / GameScene / GameScene3D / Settings / SuperMario / PhysicsTest）行为与抽象前一致
- [ ] `SERVER_BUILD` 构建不受影响

**阶段四（SDL3 切换）**：
- [ ] `ENGINE_BACKEND=SDL3` 一键切换编译运行，游戏层源码零改动
- [ ] 全场景功能对齐，帧率不低于 SFML 版的 90%
