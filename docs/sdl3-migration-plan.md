# SDL3 迁移计划 — 单后端，SFML 最终完全移除

## 一、目标与终态

**终态**：引擎窗口 / 渲染 / 输入 / 音频全部基于 SDL3，SFML 的 graphics / window / audio / system
四个模块**完全移除**（源码零引用、CMake 零链接），无双后端开关。

**迁移策略（用户已确认）**：临时脚手架——迁移过程中先写一个**很薄的 SFML 临时适配实现**
（纯转发现有 `window->draw`，标记为脚手架），让全部渲染调用渐进地切到引擎自有 API 上，
**每一步都可编译、可运行、可回滚**；全部切完后一次性把实现替换为 SDL3，最后删除脚手架。

> 脚手架不是"双后端"：两个实现从不同时存在于最终代码中，SDL3 落地之日即脚手架删除之时。

**附带收益**：SDL3 官方支持 Emscripten，迁移完成后 Web 版（浏览器运行）成为可能
（2026-08-22 会话结论：SFML 2.6 / 3.x 均不支持 Emscripten）。

**范围边界**：
- 本次迁移 **Window / Graphics / System / Audio** 四个耦合面；
- **`sfml-network`（`sf::Packet` / `TcpClient` / `NetworkManager`）暂不迁移**：它是独立库，
  不依赖窗口/图形，与 SDL3 共存无任何冲突，SDL3 落地后继续链接使用；
  若将来要移除 SFML 全家桶，网络可换 SDL_Net 或 asio（可选后续，另立计划）；
- 与 `SERVER_BUILD` 正交：服务端构建不链渲染库，不受影响。

**分工约定**：文件修改由助手完成；**git 提交、编译、运行验证全部由用户执行**。
每步的"验证"条目即用户跑编译/场景时的检查清单。

---

## 二、现状分析

`sf::` 全仓引用统计：**100 个文件、783 处**（src/ 下，不含 lib/；其中 Network/ 约 60 处属范围外）。按耦合性质分类：

### 1. 耦合面分类

| 耦合面 | sf:: 类型 | 主要位置 | 迁移难度 |
|--------|-----------|----------|----------|
| 数值类型 | `Vector2f/i/u`、`Time`、`Color`、`IntRect` | 几乎所有文件（GameObject 的 position/size/speed 等） | 低（别名过渡） |
| 渲染链签名 | `sf::RenderWindow*` | [Scene.h](file:///e:/Projects/GameEngine/src/Scene/Scene.h)、[GameObject.h](file:///e:/Projects/GameEngine/src/GameObjects/GameObject.h)、[Component.h](file:///e:/Projects/GameEngine/src/Components/Component.h)、[SceneManager.h](file:///e:/Projects/GameEngine/src/Manager/SceneManager.h)、[GameEngine.h](file:///e:/Projects/GameEngine/src/GameEngine.h) | 中（机械替换签名） |
| 事件链签名 | `sf::Event&` | 同上 + [Camera.h](file:///e:/Projects/GameEngine/src/Camera.h) | 中（需定义 EngineEvent） |
| 资源对象 | `Texture`、`Font`、`SoundBuffer` | [AssetManager.h](file:///e:/Projects/GameEngine/src/Manager/AssetManager.h)、[Animation.h](file:///e:/Projects/GameEngine/src/Animation.h) | 中（句柄化） |
| 相机 | `sf::View`、`sf::FloatRect` | [Camera.h](file:///e:/Projects/GameEngine/src/Camera.h) | 低（藏进实现内部） |
| 直接绘制调用 | `window->draw(...)` | 20 个文件、55 处（见下表） | 中（改为绘制命令） |
| 音频 | `sf::Music` / `sf::Sound` | SuperMario 场景 BGM/音效（调用点少） | 低（切换时直改 SDL_mixer） |
| 网络（范围外） | `sf::Packet`、`sf::TcpSocket` | [Network/](file:///e:/Projects/GameEngine/src/Network) | 本次不动 |

### 2. 直接绘制调用点（55 处 / 20 文件，Step 6 逐子项改造）

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

- **游戏/玩法逻辑并不真正依赖 SFML**，只是在 `update / render / handleEvent` 签名里"路过" sf:: 类型；
- 真正使用 SFML 绘制能力的集中在**叶子代码**（上表 20 个文件）与 4 个基础设施类
  （GameEngine / Camera / AssetManager / Animation）；
- 因此抽象层切在**引擎基类签名**上，叶子代码改为调用绘制命令，其余几十个文件只是机械换类型名。

---

## 三、技术决策

### 1. 无后端开关：Renderer 具体类 + 实现文件整体替换

不做 `IRenderer` 纯虚接口、不做 `ENGINE_BACKEND` 选项、不建 Platform 双实现目录。
改为：**一个具体类 `Renderer`，头文件只含引擎自有类型，实现在 .cpp 里整体替换**：

- 脚手架期：`Renderer.h` + `RendererSFML.cpp`（临时，内部持有 `sf::RenderWindow`）；
- 终态：`Renderer.h`（不动）+ `RendererSDL3.cpp`（内部持有 `SDL_Window* + SDL_Renderer*`）；
- 切换 = CMake 换一个源文件名 + `GameEngine.cpp` 换窗口/事件泵代码，游戏层零改动。

这样避免了"为单一实现设计接口"的过度设计，同时保留一层防火墙：
游戏层永远不见 `SDL_Texture` / `SDL_Event` 等 C 类型（SDL 头是纯 C，污染面大），
将来若再换后端或上 Emscripten，只动 `RendererSDL3.cpp` 一个文件。

### 2. Renderer 职责：窗口 + 事件泵 + 渲染 一体

SDL3 中 `SDL_Window` 与 `SDL_Renderer` 本就成对出现；SFML 中 `sf::RenderWindow` 同样身兼窗口与渲染。
因此 `Renderer` 收拢三件事（这也让 `GameEngine` 彻底不接触第三方类型）：

1. **窗口**：`createWindow(size, title)` / `destroyWindow()` / `getSize()`；
2. **事件泵**：`pollEvent(EngineEvent& out) -> bool`（内部完成 sf::Event / SDL_Event → EngineEvent 转换）；
3. **渲染**：clear / present / 各绘制命令 / 相机。

### 3. 数值类型：先别名、后自研

```cpp
// src/Core/Types.h —— 阶段一：别名，零风险
namespace eng {
    using Vec2f = sf::Vector2f;
    using Vec2i = sf::Vector2i;
    using Vec2u = sf::Vector2u;
    using Vec3f = sf::Vector3f;   // 3D 模块（ModelManager/GameObject3D）使用
    using Time  = sf::Time;
    using Color = sf::Color;
    using IntRect  = sf::IntRect;
    using FloatRect = sf::FloatRect;
}
```

全局把 `sf::Vector2f` 机械替换为 `eng::Vec2f`（**排除** `src/Network/`）。
SDL3 落地时只改这一个头文件，换成自研 `struct Vec2f { float x, y; }`（保持运算符 API 一致）——
因为那时只有 `RendererSDL3.cpp` / `AssetManager.cpp` 等实现文件接触第三方 API。

### 4. 事件抽象：EngineEvent + 自有键码枚举

- `enum class Key { A, B, ..., Space, Enter, Escape, Up, Down, Left, Right, LShift, LCtrl, F1... }`；
- `struct EngineEvent`：`type` + 各事件子结构（key / mouseButton / mouseMove / wheel / resize / text / close）；
- SFML→EngineEvent 转换表先放在独立小文件（阶段一用），SDL3 切换时换 `SDL_Scancode → Key` 表；
- 转换只发生在 **Renderer::pollEvent 内部一个位置**，其余代码只见 EngineEvent。

### 5. 相机进 Renderer

SDL3 没有 `sf::View` 等价物。相机进 Renderer 接口后：
- 脚手架实现：`setCamera` 映射到 `sf::View`；
- SDL3 实现：保存相机参数，内部给所有绘制命令应用偏移/缩放；
- `screenToWorld`（现用 `mapPixelToCoords`，鼠标放球功能依赖）同样是 Renderer 方法。

### 6. 资源句柄化

`TextureHandle` / `FontHandle` 只是 `uint32_t` id，`sf::Texture` / `SDL_Texture` 等藏进 AssetManager 内部。
原因：**SDL_Texture 与 SDL_Renderer 绑定**，渲染器重建时纹理要跟着重建——句柄化后这个差异被
AssetManager 实现吸收，游戏层无感知。SDL3 切换 = 重写 `AssetManager.cpp` 内部（外部 API 不变）。

### 7. 音频：不抽象，切换时直改 SDL_mixer

音频调用点少（SuperMario BGM/音效），不值得单设接口层：
脚手架阶段**完全不动音频**（继续 `sf::Music` / `sf::Sound`）；SDL3 切换的同一提交里，
播放调用点直接改成 SDL_mixer（`Mix_Chunk` ≈ `sf::Sound`，`Mix_Music` ≈ `sf::Music`，心智模型一致）。

### 8. 依赖清单（SDL3 生态）

| 库 | 用途 | 对应被替换的 SFML 能力 |
|----|------|------------------------|
| SDL3（≥3.2） | 窗口/事件/渲染/音频设备 | sfml-window + sfml-graphics + sfml-system |
| SDL_image 3.x | PNG 等图片解码 | sf::Texture::loadFromFile |
| SDL_ttf 3.x | 文字渲染（配纹理缓存） | sf::Text / sf::Font |
| SDL_mixer 3.x | 音效/音乐 | sfml-audio |

`sfml-network` 继续链接（范围外）。

### 9. 目录规划

```
src/
  Core/            # Types.h, Event.h, KeyCodes.h —— 纯类型，零第三方依赖
  Render/          # Renderer.h, Handles.h —— 头文件零第三方依赖
                    #   实现文件：RendererSFML.cpp（脚手架，临时）→ RendererSDL3.cpp（终态）
  （AssetManager.cpp / Camera / GameEngine.cpp 同样"头文件引擎类型、实现内部藏第三方"）
```

---

## 四、接口草案

```cpp
// src/Render/Renderer.h —— 具体类，头文件零第三方 include
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    // ── 窗口 ──
    bool createWindow(eng::Vec2u size, const std::string& title);
    void destroyWindow();
    [[nodiscard]] eng::Vec2u getSize() const;
    [[nodiscard]] bool isWindowOpen() const;

    // ── 事件泵（内部完成 sf::Event / SDL_Event → EngineEvent 转换）──
    bool pollEvent(EngineEvent& out);

    // ── 帧控制 ──
    void clear(eng::Color c);
    void present();

    // ── 绘制命令 ──
    void drawTexture(TextureHandle h, const eng::FloatRect& src, const eng::FloatRect& dst,
                     float rotationDeg = 0.f, eng::Vec2f origin = {},
                     eng::Color tint = eng::Color::White);
    void drawRect(const eng::FloatRect& r, eng::Color c,
                  bool filled = true, float outlineThickness = 0.f);
    void drawLine(eng::Vec2f a, eng::Vec2f b, eng::Color c);
    void drawLines(const std::vector<eng::Vec2f>& points, eng::Color c);   // 折线/多边形
    void drawCircle(eng::Vec2f center, float radius, eng::Color c, bool filled = true);
    void drawText(FontHandle h, const std::string& text, eng::Vec2f pos,
                  unsigned size, eng::Color c);

    // ── 相机 ──
    void setCamera(eng::Vec2f center, eng::Vec2f size, float zoom = 1.f);
    void resetCamera();
    [[nodiscard]] eng::Vec2f screenToWorld(eng::Vec2i screenPos) const;
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

> **每一步 = 一次 git commit（由用户执行）= 一个原子操作**，必须同时满足：

1. **可编译**：该步完成后 `cmake --build` 通过。
2. **可运行**：程序能启动，所有现有功能行为不变。
3. **可回滚**：`git revert <commit>` 后回到上一步状态，无残留依赖。
4. **单一职责**：一步只做一件事。
5. **不破坏契约**：EventBus `"onCollision"+tag` 等既有契约不变。

完成后将 `[ ]` 改为 `[x]`。

---

### 阶段一：类型与事件抽象（机械替换为主，与后端无关）

#### Step 1 — Core/Types.h 数值类型别名
- [x] 新建 `src/Core/Types.h`（内容见技术决策 3）
- [x] 全局机械替换：`sf::Vector2f → eng::Vec2f` 等 8 个别名
      （**排除**：`src/Network/`；`Physics/` 内 b2Vec2 互转处随替换）
- **验证**：编译通过；所有场景运行表现不变（用户已确认）
- **新增文件**：`src/Core/Types.h`
- **改动文件**：90 个（纯类型名替换 + 添加 include；已验证：8 种旧类型零残留，
      所有 eng:: 使用者均含 `#include "Core/Types.h"`，include 均在无条件编译区）
- **原子性保证**：别名即原类型，语义零变化，编译器逐处校验

#### Step 2 — EngineEvent + Key 枚举 + 输入轮询 + SFML 转换器（纯新增）
- [x] 新建 `src/Core/Event.h`（`EventType` / `MouseButton` / `EngineEvent`，
      覆盖现有全部事件：Closed/Resized/Key*/MouseButton*/MouseMoved/MouseWheelScrolled/TextEntered + 焦点备用）
- [x] 新建 `src/Core/KeyCodes.h`（`enum class Key`：现有按键 A/D/W/J/R/Space/Escape/Enter/方向键
      + 常用键前瞻：字母/数字/F1-F12/修饰键/编辑键/标点，物理键位语义）
- [x] 新建 `src/Core/Input.h`（**计划外补充，实施时发现**：现有代码存在轮询式输入——
      `sf::Keyboard::isKeyPressed`（PhysicsTestScene）与 `sf::Mouse::getPosition`（6 处，均在
      handleEvent 链内）——抽象为 `eng::Input::isKeyPressed / getMousePosition`）
- [x] 新建 `src/Core/EventConvertSFML.h/.cpp`（**临时文件，Step 11 删除**）：
      `toEngineEvent(const sf::Event&) -> std::optional<EngineEvent>`（SFML 特有事件返回 nullopt）；
      `sf::Keyboard::Key ↔ Key` 双向映射（int 参数 + 前向声明，头文件零 SFML include）；
      `eng::Input` 的 SFML 实现（`setInputWindow` 注册窗口后提供窗口相对鼠标坐标）
- **验证**：编译通过（尚无人引用，纯新增）
- **新增文件**：`KeyCodes.h`、`Event.h`、`Input.h`、`EventConvertSFML.h/.cpp`
- **原子性保证**：纯新增，零引用；枚举名已对照 lib/SFML-2.6.1 头文件核实（Hyphen/Grave/Middle 等）
- **实施发现（记入 Step 3）**：
  1. `NetworkManager::handleEvent(sf::Event&)` 被 Scene 挂在事件链上（Closed 断连 + Client 按 R 重生），
     Step 3 必须同切 EngineEvent（仅 2 处判断；`sf::Packet/Socket` 属 sfml-network 范围外，不动）；
  2. PhysicsTestScene 的 `isKeyPressed` 轮询与 6 处 `sf::Mouse::getPosition` 在 Step 3 同切 `eng::Input`；
  3. `GameEngine` 主循环启动时需调 `eng::detail::setInputWindow(window)` 注册轮询窗口。

#### Step 3 — handleEvent 链路切换
- [x] `Component / GameObject / Scene / SceneManager / Camera` 的 `handleEvent` 签名：
      `sf::Event& → const eng::EngineEvent&`（含 `GameObject::handleComponents`）
- [x] `GameEngine` 主循环：`pollEvent` 后经 `eng::toEngineEvent` 转换分发（nullopt 跳过）；
      窗口创建后调 `eng::detail::setInputWindow(window)` 注册轮询窗口
- [x] 所有 `handleEvent` 实现改用 EngineEvent 字段（MarioController、Controller、Camera、
      Button/TextInput/Toggle、Mario 状态机 ×3、StateMachine、Mario、Cube3DWithController、
      NetworkManager、GameScene/GameScene3D/SuperMarioScene/PhysicsTestScene，共 40 个文件
      = 21 头 + 19 cpp）
- [x] 轮询输入同切：PhysicsTestScene `isKeyPressed` → `eng::Input::isKeyPressed`；
      6 处 `sf::Mouse::getPosition` → `eng::Input::getMousePosition`
      （Camera、Scene::getMousePosition、Cube3DWithController ×2、PhysicsTestScene、主循环注册）
- **验证**：全场景交互测试（A/D 移动、Space 跳、鼠标放球、相机拖动、窗口缩放、文本输入）
- **已验证（静态）**：`sf::Event/Keyboard/Mouse` 全仓仅剩 3 个合法位置——
      `GameEngine.cpp`（主循环唯一转换点）与 `Core/EventConvertSFML.h/.cpp`（脚手架本体）；
      `BaseState` 继承 `Component`，全部 `override` 闭合
- **原子性保证**：类型系统兜底——漏改处必然编译错误，不会静默行为异常

---

### 阶段二：渲染脚手架（Renderer 类 + SFML 临时实现）

#### Step 4 — 资源句柄类型（纯新增）
- [x] 新建 `src/Render/Handles.h`（`TextureHandle` / `FontHandle`，`uint32_t` id + `isValid()`
      + 相等比较 + `std::hash` 特化；音频 SoundBuffer 不做句柄，Step 10 直改 SDL_mixer）
- **验证**：编译通过（无人引用）
- **原子性保证**：纯新增

#### Step 5 — Renderer.h + RendererSFML.cpp 脚手架 + GameEngine 接管
- [x] 新建 `src/Render/Renderer.h`：窗口/事件泵/帧控制/绘制命令/相机全套接口，
      头文件零第三方 include（SFML 仅前向声明，供临时 `getSfmlWindow()` 过渡 API，Step 6e 删）。
      **实施时接口补充**（相对草案）：`closeWindow()`（对应原 window->close）、
      `setFramerateLimit()`（原主循环调用）、`drawPolygon()`（实心凸多边形，PhysicsDebugDraw
      实心多边形/圆角矩形拼合需要）、`drawRect` 增加 `outlineColor` 参数
      （物理调试框"填充色+白描边"两色需求）
- [x] 新建 `src/Render/RendererSFML.cpp`（**脚手架，临时，Step 10 整体替换**）：
      持有 `sf::RenderWindow`；`pollEvent` 内部循环跳过 SFML 特有事件（nullopt 继续 poll，
      不会误报"无事件"）；`drawTexture` 临时 `sf::Sprite` 路径；`drawText` 用
      `sf::String::fromUtf8`（UTF-8 直传中文）；`setCamera` 映射 `sf::View`；
      `screenToWorld` 用 `mapPixelToCoords`；createWindow 内部完成 `setInputWindow` 注册。
      全文件 `#ifndef SERVER_BUILD` 包裹
- [x] **AssetManager 句柄 API 提前到本步**（原计划在 Step 7）：`drawTexture(TextureHandle)`
      需要句柄解析，故以**纯增量**方式加入——`getTextureHandle(name)`（首次访问分配 id）、
      `getTexture(TextureHandle)`（Renderer 内部用）、`getFontHandle()`（单字体固定 id=1）、
      `getFont(FontHandle)`；**旧按名 API 原样保留**，现有调用点零改动（Step 7 再迁移并移除旧 API）
- [x] `GameEngine` 接管：`eng::Renderer renderer` 成员替代 `sf::RenderWindow* window`
      （析构不再手写 delete）；`start()` 去 const（Renderer 方法非 const，main.cpp 调用不受影响）；
      主循环 `renderer.pollEvent/clear/present`；场景构造传 `renderer.getSfmlWindow()`（过渡）
- [x] 顺手修正：`Core/EventConvertSFML.cpp` 加 `#ifndef SERVER_BUILD` 守卫
      （引用 sfml-window 符号，服务端构建不链接该库；Step 3 遗漏）
- **验证**：编译通过；窗口正常打开，各场景表现不变（渲染仍走旧 window->draw 路径，
      仅窗口创建/事件泵/clear/present 四件事换由 Renderer 执行）
- **原子性保证**：Renderer 已接管窗口生命周期与事件泵；绘制路径未动，
      行为差异理论为零（事件经同一转换器，语义不变）

#### Step 6 — render 链路渐进切换（5 个子提交，每个可独立运行）
- [ ] **6a 基类与基础设施**：`Scene/GameObject/Component/SceneManager` 的 render 签名
      `sf::RenderWindow* → Renderer&`；`Camera` 内部改用 `Renderer::setCamera`
      （去掉 `sf::View` 成员）；`Scene::getMousePosition` 改用 `screenToWorld`
- [ ] **6b 物理场景**：PhysicsTestScene 5 处 + PhysicsDebugDraw 13 处 → 绘制命令
- [ ] **6c Mario 系**：SuperMarioScene 3 处、4 个 State 共 7 处、Animation、Brick、FireBall、Box
- [ ] **6d UI 场景**：Button 6 处、Toggle 4 处、TextInput 4 处、MenuScene 2 处、SettingsScene 2 处
- [ ] **6e 3D 与杂项**：GameObject3D 线框（顶点圆 + 线，`drawCircle`/`drawLines` 覆盖）、
      Cube3D/Human3D/Penguin3D/NewModel3D、Player/Circle、碰撞调试框 2 处、
      MoveComponent 速度箭头、HealthBar；完成后删除 `Scene::getWindow()` 与
      `RendererSFML.cpp` 暴露的过渡取窗接口
- **验证**：每切一个子项跑对应场景；全部完成后逐场景核对与迁移前一致
- **原子性保证**：每个子提交自洽可编译；同一场景的改动不跨提交

#### Step 7 — AssetManager 句柄化 + Animation 纯数据化
- [ ] `getTexture/getFont` 返回 `TextureHandle/FontHandle`；sf 对象藏入内部 map
      （**外部 API 从此与第三方无关**；音频照旧不动）
- [ ] `Animation` 去掉 `sf::Sprite` 成员：只存帧矩形序列 + 帧时长（纯数据），
      渲染由持有它的对象调 `drawTexture(handle, frameRect, dstRect, ...)`
- [ ] 各 GameObject 移除 `sf::Sprite` 成员（Mario 状态机里的 `left_sprite/right_sprite` 改为
      每帧计算 src/dst 矩形）
- **验证**：Mario 跑/跳/死动画帧序列与迁移前一致
- **原子性保证**：7a AssetManager（编译器强制改完所有调用点）、7b Animation+使用方

**阶段一~二完成 = 脚手架里程碑**：游戏层已零 SFML 引用（Network/ 除外），
SFML 退缩为 `RendererSFML.cpp` + `AssetManager.cpp` + 少量音频调用点三个文件的内部实现。

---

### 阶段三：SDL3 替换（核心大步）

#### Step 8 — SDL3 依赖接入（不影响现有构建）
- [ ] CMake `FetchContent` 拉取 SDL3（≥3.2）+ SDL_image 3.x + SDL_ttf 3.x + SDL_mixer 3.x
- [ ] 仅声明依赖，不切换任何源文件
- **验证**：`cmake --build` 通过；SFML 路径行为不变
- **原子性保证**：新依赖只编译不链接进主目标的行为

#### Step 9 — SDL3 实现文件（纯新增，可编译未启用）
- [ ] `src/Render/RendererSDL3.cpp`：实现 `Renderer.h` 全部接口
      （`SDL_RenderTexture/RenderLine/RenderGeometry`；相机 = 内部统一变换；
      文字 = SDL_ttf + 纹理缓存（按 font+text+size 做 key，LRU 淘汰）；
      事件泵 = `SDL_PollEvent` + `SDL_Scancode → Key` 映射表）
- [ ] `AssetManager.cpp` 的 SDL3 内部实现（SDL_image → `SDL_Texture`，接口不变）——
      与脚手架版本并存的方式：临时用 `#ifdef ENGINE_SDL3` 或两份 `.cpp` 按 CMake 选择
- [ ] SDL_mixer 音频：`Mix_Chunk`/`Mix_Music` 加载与播放（对应现有 `sf::Sound`/`sf::Music` 调用点的目标写法）
- **验证**：编译通过；`ENGINE_SDL3` 未定义时行为完全不变
- **原子性保证**：新实现全部在编译开关后，默认路径零影响

#### Step 10 — 切换：SDL3 生效（单一目的提交）
- [ ] CMake：源文件从 `RendererSFML.cpp` 换成 `RendererSDL3.cpp`，定义 `ENGINE_SDL3`，
      链接 SDL3 全家 + 移除 sfml-graphics/window/audio/system 链接
- [ ] 音频播放调用点改 SDL_mixer（调用点少，直改）
- [ ] `Core/Types.h` 别名换成自研 struct（`Vec2f` 等，运算符 API 保持一致）
- **验证**：**逐场景跑全部功能**——Menu / GameScene / GameScene3D / Settings / SuperMario /
  PhysicsTest（A/D 移动、Space 跳、R 重置、鼠标放球、相机拖动、文字输入、BGM/音效、
  物理调试绘制）
- **原子性保证**：所有调用方早已走 Renderer/Handle 抽象，本提交只动
  实现文件选择 + 少量音频调用 + Types.h；若出问题，回滚本提交即回到 SFML

#### Step 11 — 删除脚手架与 SFML 残留
- [ ] 删除 `RendererSFML.cpp`、`EventConvertSFML.h/.cpp`、`AssetManager` 的 SFML 内部实现、
      一切过渡期 `#ifdef`
- [ ] 全仓清理：除 `src/Network/` 外 `#include <SFML/...>` 为零
      （`sfml-network` 头在 Network/ 内继续使用）
- [ ] CMake 移除 SFML graphics/window/audio/system 的 FetchContent/链接
- **验证**：编译通过 + 全场景回归；`grep -r "sf::" src/` 仅 Network/ 目录有命中
- **原子性保证**：纯删除死代码，Step 10 后这些文件已无人引用

---

### 阶段四（可选后续，另行立项）

- [ ] **Emscripten/Web 版**：SDL3 官方支持，`emcmake` + wasm 构建，验证浏览器运行
- [ ] **网络模块迁移**：若要彻底告别 SFML，`sfml-network` → SDL_Net 或 asio（功能对齐评估后另立计划）

---

## 六、SDL3 已知坑（Step 9~10 逐条核对）

| # | 坑 | 对策 |
|---|-----|------|
| 1 | `SDL_Texture` 与 `SDL_Renderer` 绑定，渲染器重建纹理全部失效 | 资源句柄化（Step 7），AssetManager 内部集中重建 |
| 2 | SDL_ttf 逐帧 `TTF_Render*` 生成纹理很慢 | 文字纹理缓存（font+text+size 做 key，LRU 淘汰） |
| 3 | 无 `sf::View` 等价物 | 相机进 `Renderer`（Step 5），SDL3 实现内部统一变换 |
| 4 | 键盘 scancode / keycode 双体系 | 统一用 scancode 映射 `Key` 枚举（物理键位，游戏习惯） |
| 5 | 鼠标世界坐标（现 `mapPixelToCoords`） | `screenToWorld` 已是 Renderer 方法，SDL3 实现自算 |
| 6 | 大量 `RenderLine` 逐条调用慢 | 3D 线框/调试绘制聚合到 `drawLines`，SDL3 用 `SDL_RenderGeometry` 批量 |
| 7 | SDL3 音频是 stream 设备模型，直接操作很底层 | 用 SDL_mixer（Chunk/Music 模型），不裸用 SDL_audio |
| 8 | 中文/UTF-8 文字渲染 | SDL_ttf UTF-8 接口，字体文件沿用（Minecraft_AE.ttf 等） |
| 9 | Y-down 坐标系 | SFML/SDL3 一致，Box2D（PPM=64、重力 Y+）完全不受影响 |
| 10 | SDL3 事件无 TextEntered 直接等价（是 text input 需先 `SDL_StartTextInput`） | Renderer 窗口创建后开启 text input；TextInput 场景重点测试 |
| 11 | 圆角矩形 Toggle/Button 的九宫格拼合 | 绘制命令用矩形/圆拼合复现（现有做法本身就是拼合，平移即可） |

---

## 七、验收标准

**阶段一~二（脚手架落地）**：
- [ ] `src/Core/`、`src/Render/` 头文件无任何第三方 include
- [ ] 除 `src/Network/`、`RendererSFML.cpp`、`AssetManager.cpp`（内部）、音频调用点外，
      全仓 `#include <SFML/...>` 为零
- [ ] 所有场景行为与抽象前一致
- [ ] `SERVER_BUILD` 构建不受影响

**阶段三（SDL3 终态）**：
- [ ] 除 `src/Network/` 外全仓零 SFML 引用；CMake 无 sfml-graphics/window/audio/system
- [ ] 全场景功能对齐脚手架版本（容忍渲染细节的像素级差异）
- [ ] 帧率不低于 SFML 版的 90%
- [ ] 无 `ENGINE_BACKEND` 类开关——SDL3 是唯一实现
