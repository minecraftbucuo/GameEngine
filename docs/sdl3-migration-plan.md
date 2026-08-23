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
- [x] **6a 基类与基础设施**（双签名转发方案）：
      - `Component/GameObject/Scene` 新增 `render(eng::Renderer&)`，**默认实现转发旧签名**
        （未迁移的子类经转发走原 `render(sf::RenderWindow*)` 路径，行为零变化）；
        旧签名保留至 6e
      - `SceneManager::render` 切换到 `Renderer&`（唯一调用点 GameEngine 主循环同步）
      - `Scene` 构造改收 `eng::Renderer*`；新增 `getRenderer()`；`window` 成员保留为过渡
        （= `renderer->getSfmlWindow()`，6e 删除）；`getWindowSize/getMousePosition` 切 renderer
      - **Camera 彻底去 SFML**：`sf::View/sf::RenderWindow` 成员删除，内部改持 `eng::Renderer*`。
        floatRect.left/top 语义为**可视区左上角**（原 `sf::View(FloatRect)` 构造语义：
        矩形中心成为视图中心），`updateView()` 换算 `left+width/2, top+height/2` 后映射
        `Renderer::setCamera(center, size)`（曾误判为中心语义导致全场景视图偏移半屏，已修复）；
        `getCenter()` 同步返回 `left+width/2, top+height/2`（与原 `view.getCenter()` 一致）
      - 6 个场景构造签名 `sf::RenderWindow* → eng::Renderer*`（机械替换，渲染主体不动）
      - `GameEngine`：`addScene<X>(&renderer)`、`scene_manager->render(renderer)`
      - **服务端链接安全**：`Renderer::getSfmlWindow` 改类内 inline 定义——
        Component 基类新 render 内联体引用它，而 RendererSFML.cpp 被 SERVER_BUILD 排除，
        inline 定义避免服务端 vtable 缺符号
- **验证**：编译通过（客户端+服务端两版本）；各场景渲染与 Step 5 完全一致
  （全部经默认转发走旧路径）；PhysicsTest 相机拖动/缩放/方向键正常（Camera 已换实现）
- [x] **6b 物理场景**（首个全绘制命令场景）：
      - **Renderer 接口扩展**（实施时按需补充）：`drawRect` 增加 `rotationDeg + origin`
        （旋转物理体需要）；`drawCircle` 增加 `outlineThickness + outlineColor`
        （填充+白描边两色需求）。统一旋转语义：矩形为未旋转可视区域，origin 为矩形内支点
        （自左上角），绕支点旋转——与 SDL_RenderCopyRotF 的 center 一致；
        `drawTexture` 实现同步修正为此语义（当时无调用者，安全）
      - **GameObject::render(新签名) 基类默认实现改为转发旧虚 render**（6a 原为 renderComponents）：
        未迁移子类的旧 override 经新路径继续生效，已迁移子类直接覆盖新签名
      - PhysicsTestScene 内嵌 5 类（Player/Box/Ball/Ground/Platform）render 全部切绘制命令；
        场景新增 `render(eng::Renderer&) override`（对象循环 + renderDebug 搬入）；
        放球的 `mapPixelToCoords` 顺手切 `renderer->screenToWorld`
      - **PhysicsDebugDraw 彻底去 SFML**：`setWindow → setRenderer`，b2Draw 7 个回调 +
        drawVelocities 全部翻译成 drawLines/drawPolygon/drawCircle/drawLine（闭合折线=首尾相接；
        实心多边形=半透明填充+同色描边，与 SFML 行为一致）
      - PhysicsWorld::renderDebug 签名 `sf::RenderWindow* → eng::Renderer*`（前置声明同步）；
        Scene 旧 render 的 renderDebug 调用移除（职责移入 PhysicsTestScene）
      - `eng::Uint8` 别名补入 Types.h（颜色转换需要）
      - **插曲**：PhysicsDebugDraw.h/.cpp 两次 Write 均被编辑器缓冲区还原未落盘
        （探测文件证实 Write 工具正常、目标文件时间戳未变），经"写新文件名 + PowerShell
        Move-Item 替换"绕过解决，全部 12 项改动 PowerShell 逐项复核落盘
- **验证**：编译；PhysicsTest 场景：方块/球/斜面/地面渲染一致（含旋转体）；
  开 CONFIG.game.debug 后调试图形（形状描边/质心轴/速度箭头）与迁移前一致；
  鼠标放球位置正确（screenToWorld）；其余场景不受影响
- [x] **6c Mario 系**（含 Animation/FrameManager 数据化提前，原 Step 7 部分）：
      - **Renderer 接口扩展**：`drawTexture` 加 `flipX`（镜像精灵，SFML 用 src 矩形翻转实现，
        SDL3 用 flip 参数）；`CameraState/getCamera` + `setCamera(CameraState)`（死亡屏保存/恢复
        相机，替代 sf::View 操作）；`measureText`（文本居中排版，SDL3 对应 TTF_SizeText）；
        AssetManager 加 `getTextureSize(handle)`（背景等比缩放计算）
      - **Animation 纯数据化**：`Frame.texture` 由 `sf::Texture*` → `eng::TextureHandle`；
        删 `sf::Sprite` 成员与 `getSprite()`；`render(eng::Renderer&, pos)` 走 drawTexture
        （JSON 帧负 scale.x → flipX + abs 尺寸）；FrameManager 同步句柄化；
        Box 的 `getSprite().getGlobalBounds()` → `getFrameWidth/Height()`
      - **状态机链**：BaseState 去 SFML include；StateMachine/4 状态 render 切新签名；
        状态内 `sf::Sprite ×2`（左右）→ 句柄+矩形数据，方向用 `flipX = getIsLeft()` 表达
        （原 setScale(-4,4)+setOrigin 镜像语义 = dst 不变内容镜像，已验证等价）
      - **对象**：Brick/FireBall/Box render 切新签名（Brick 的 sf::Sprite → 句柄+矩形）
      - **组件调试绘制**：MoveComponent::drawArrow（drawLine+drawPolygon）、
        BoxCollision/CircleCollision debug 框切 drawRect/drawCircle
      - **SuperMarioScene**：render 新签名 override；bg 的 sf::Sprite → 句柄+dst 矩形
        （init 按窗口高度等比算）；死亡屏 sf::View 保存/恢复 → getCamera/resetCamera/setCamera，
        sf::Text → drawText + measureText 居中
      - **渲染链路闭合（关键修复）**：Scene 基类新签名默认改为**新对象循环**（原为转发旧，
        会使 Mario 等无对象级 override 的对象收不到新签名组件渲染）；GameObject 新签名默认 =
        转发旧虚 render + `renderComponents(renderer)`；旧虚 render 基类默认改空
        （Player/Circle 等旧 override 子类手动调 renderComponents(window) 不受影响）
- **验证**：编译双版本；Mario 场景全回归：跑动动画（左右镜像）、跳跃/死亡帧、
  火球+爆炸动画、箱子顶开动画（setBack 往返）、砖块贴图、背景铺满、死亡屏
  （半透明遮罩+居中文字+R 重生/Esc 退出）、开 debug 看碰撞红框/速度箭头/血条
- [x] **6d UI 场景**：Button 6 处、Toggle 4 处、TextInput 4 处、MenuScene 2 处、SettingsScene 2 处
  （实施补充：sf::String 全部换 UTF-8 std::string；drawRoundedRect 收进脚手架；
  drawText 分离"光栅化字号 size"与"变换缩放 scale"两个参数——连续文字动画必须
  固定字号走 GPU 缩放，否则逐帧重新光栅化产生字形抖动）
- [x] **6e 3D 与杂项**：GameObject3D 线框（顶点 4px 白圆 + 白色边线）、
      Cube3D/Human3D/Penguin3D/NewModel3D、Player/Circle（sf::CircleShape 数据化）、
      HealthBar（drawRect ×2）；**旧渲染签名全部删除**：
      `Component/GameObject/Scene::render(sf::RenderWindow*)`、
      `renderComponents(sf::RenderWindow*)`、`Scene::window` 成员、`Scene::getWindow()`、
      `Renderer::getSfmlWindow()`（窗口指针降为 Renderer 私有实现细节）；
      `Renderer::setSize()` 新增（GameScene3D 进出场改分辨率）；
      GameEngine 主循环计时 sf::Clock → std::chrono（服务端节拍同步替换）；
      多余 SFML include 清理（Timer/GameScene/Controller/ModelManager/Mario/
      Physics 四件头/NetworkManager Graphics）——
      游戏层 SFML include 现仅剩：Types.h（别名）、AssetManager（资源）、
      MarioController.h（sf::Sound）、脚手架两文件、Network/
- **验证**：每切一个子项跑对应场景；全部完成后逐场景核对与迁移前一致
- **原子性保证**：每个子提交自洽可编译；同一场景的改动不跨提交

#### Step 7 — AssetManager 句柄化 + Animation 纯数据化
- [x] `getTexture/getFont` 返回 `TextureHandle/FontHandle`；sf 对象藏入内部 map
      （**外部 API 从此与第三方无关**；音频照旧不动）
      （实施记录：7a 本体 = 删除旧按名 API `getTexture(name)` / `addTexture(name, sf::Texture)` /
      无参 `getFont()`（懒加载逻辑内联进 `getFont(FontHandle)`）；调用点已在 6c~6e 期间全部
      切至句柄 API（Brick/状态机 ×3/FrameManager/SuperMarioScene/Button/TextInput/
      MenuScene/SettingsScene），此步为纯删除，编译器验证零残留。
      `getTexture(TextureHandle)` / `getFont(FontHandle)` 返回 sf 类型——仅供 RendererSFML.cpp
      脚手架使用，属实现面 API，Step 9 与 SDL3 实现一起换签名）
- [x] `Animation` 去掉 `sf::Sprite` 成员（6c 提前完成：Frame 存句柄 + 帧矩形 + 帧时长纯数据，
      渲染由持有者调 `drawTexture(handle, frameRect, dstRect, ...)`）
- [x] 各 GameObject 移除 `sf::Sprite` 成员（6c/6e 提前完成：Mario 状态机 ×4 / Brick / Box /
      Player / Circle 均为句柄+矩形数据，左右方向用 `flipX` 表达）
- **验证**：Mario 跑/跳/死动画帧序列与迁移前一致（6c 已验证）；本步纯删除后全量编译 +
  抽查 Mario 场景（用户执行）
- **原子性保证**：7a AssetManager（编译器强制改完所有调用点）、7b Animation+使用方（已并入 6c）

**阶段一~二完成 = 脚手架里程碑（2026-08-23 达成）**：游戏层已零 SFML 引用，
SFML 退缩为以下实现面文件的内部细节（Step 9~11 逐个替换/删除）：
`RendererSFML.cpp` + `Core/EventConvertSFML.h/.cpp`（脚手架本体）、
`AssetManager.h/.cpp`（纹理/字体/音频资源）、`MarioController.h`（sf::Sound 音频调用点）、
`Core/Types.h`（数值别名，Step 10 换自研 struct）、`Network/`（范围外）。

---

### 阶段三：SDL3 替换（核心大步）

#### Step 8 — SDL3 依赖接入（不影响现有构建）
- [x] CMake `FetchContent` 拉取 SDL3（≥3.2）+ SDL_image 3.x + SDL_ttf 3.x + SDL_mixer 3.x
- [x] 仅声明依赖，不切换任何源文件
      （实施记录：锚定 SDL3 正式发布版组合 SDL release-3.2.0 / SDL_image release-3.2.0 /
      SDL_ttf release-3.2.0 / SDL_mixer release-3.0.0，后续可自行升级；
      全部静态构建（SDL_STATIC ON，与 SFML 静态策略一致），卫星库外部依赖全部 vendored
      源码构建（libpng/zlib/freetype/libogg/libvorbis），Windows 零系统依赖；
      按实际资源裁剪：SDL_image 仅 PNG（关 JPG/TIF/WEBP/JXL/AVIF），SDL_mixer 仅
      Vorbis+WAV（Asset 音频全为 ogg/wav，关 MP3/FLAC/MOD/OPUS）——构建时间与体积最小化；
      `BUILD_WITH_SDL3` 选项（默认 ON）控制，服务端构建不拉取；
      **主目标未链接任何 SDL 库**，运行时行为零变化；Step 10 才 target_link_libraries）
- **验证**：`cmake --build` 通过（首次配置需联网下载约百 MB 并编译 SDL3 全家，耗时数分钟，
      属预期）；SFML 路径行为不变；`-DBUILD_WITH_SDL3=OFF` 可关掉整块（回滚证明）
- **实施插曲（选项名前缀）**：卫星库 CMake 选项前缀**不带 3**——`SDLIMAGE_*` / `SDLTTF_*`；
      首版误写成 `SDL3IMAGE_*` / `SDL3TTF_*` 导致 SDLTTF_VENDORED 未生效，
      SDL_ttf 转找系统 Freetype 失败（MinGW 无系统库）硬报错。
      修正：`SDLTTF_VENDORED ON` 走子模块 freetype（FetchContent 默认拉取子模块，
      external/freetype 源码完整）；同时关 `SDLTTF_HARFBUZZ/PLUTOSVG`（中文 UTF-8
      渲染不需要文本整形，省重依赖）。SDL_image 的 PNG 走内置 stb 解码（零外部依赖，
      首次配置已验证通过）。SDL_mixer WAV 核心内置、OGG 默认 stb_vorbis（源码内置），
      前缀两种写法（SDLMIXER_/SDL3MIXER_）都设以防枚举名不确定。
- **实施插曲（CMake 4 策略版本）**：vendored freetype 声明 `cmake_minimum_required(3.0)`，
      CMake 4.x 已移除 <3.5 兼容直接硬报错；官方缓解 `set(CMAKE_POLICY_VERSION_MINIMUM 3.5
      CACHE STRING "" FORCE)`（放宽旧策略告警，不影响构建产物），已加在 FetchContent 之前。
- **实施插曲（SDL_mixer 无 3.0.0 tag）**：SDL_mixer 3.x 从未发布 3.0.0，
      首个正式版直接是 `release-3.2.0`（版本与 SDL 3.2.0 对齐；已查 GitHub tags 核实，
      3.x 系列为 3.1.2 预发布 → 3.2.0 → 3.2.2 → 3.2.4）。修正 tag 为 release-3.2.0。
- **实施插曲（核心与卫星库版本不对齐——最重要教训）**：卫星库 tag 号 ≠ SDL 核心 tag 号。
      mixer 3.2.0 实际要求 SDL ≥ **3.4.0**（其 CMakeLists `set(SDL_REQUIRED_VERSION 3.4.0)`），
      ttf 3.2.0 要求 ≥3.2.6，image 3.2.0 要求 ≥3.2.0；而 SDL 核心 release-3.2.0 是 2025-01
      的旧版 → mixer 编译报 `SDL_ALIGNED` / `SDL_PROP_AUDIOSTREAM_AUTO_CLEANUP_BOOLEAN`
      未定义。且 FetchContent 同场配置时 find_package 版本检查**不会**在 configure 期拦下，
      直接漏到编译期才炸。修正：SDL 核心 → `release-3.4.14`（2026-08 最新稳定，
      3.4 系列 ABI 稳定），满足全部卫星库。**规则：升级任一库前先读各卫星库源码的
      `SDL_REQUIRED_VERSION` 再选核心版本。**
- **实施插曲（SDL_mixer 3.2.x 为全新 API）**：3.2.0 起 mixer 重写为 track 模型
      （`MIX_Init`/`MIX_CreateMixer`/`MIX_LoadAudio`/`MIX_CreateTrack`/`MIX_PlayTrack`），
      **旧 `Mix_Chunk`/`Mix_Music`/`Mix_OpenAudio` API 已删除**——
      本计划"技术决策 7"中 Mix_Chunk≈sf::Sound 的心智模型作废，
      Step 9/10 音频按新 API 写（详见 Step 9 条目）。
- **原子性保证**：新依赖只编译不链接进主目标的行为

#### Step 9 — SDL3 实现文件（纯新增，可编译未启用）✅ 2026-08-23 完成
- [x] `src/Render/RendererSDL3.cpp`：实现 `Renderer.h` 全部接口
      （`SDL_RenderTextureRotated/RenderLines/RenderGeometry`；相机 = 内部统一变换；
      文字 = SDL_ttf + 纹理缓存（按 size+text 做 key，白字纹理 + ColorMod 着色，LRU 上限 128）；
      事件泵 = `SDL_PollEvent` + `SDL_Scancode ↔ Key` 双向映射表）
- [x] `src/Manager/AssetManagerSDL3.cpp` + `AssetManager.h` 双分支（`#ifdef ENGINE_SDL3`）：
      贴图走 SDL_image（load 时解码 surface，首次取用时经渲染器上传纹理后释放 surface）；
      句柄表逻辑与 SFML 版逐行一致
- [x] SDL_mixer 3.2 音频加载侧：`MIX_Init()`（无 flags）→ `MIX_CreateMixerDevice(默认播放设备)`
      → `MIX_LoadAudio(predecode=true)`（对齐 sf::SoundBuffer 全量解码）；
      播放侧（MIX_CreateTrack/PlayTrack 封装）留 Step 10 与 MarioController 一起切
- [x] CMake `EngineSDL3` 静态库目标：**只编译不链接**（编译验证），主目标 glob 排除两个 SDL3 文件
- **实施要点（踩坑记录）**：
  - SDL3_ttf 3.2 API 改名：`TTF_RenderUTF8_Blended` → `TTF_RenderText_Blended(font, text, length, color)`；
    测量用 `TTF_GetStringSize`；`TTF_OpenFont` 的 ptsize 为 float
  - SDL3_image 3.2 已删除 `IMG_Init/IMG_Quit`，解码器自动注册，直接 `IMG_Load`
  - SDL 文本事件只含可打印字符：退格需在 `SDL_EVENT_KEY_DOWN(BACKSPACE, 非repeat)` 时
    **合成 `TextEntered(8)` 补发**（内部 pending 队列），否则 TextInput 退格删除失效
  - SDL 需显式 `SDL_StartTextInput(window)` 才会产生 TEXT_INPUT 事件（SFML 恒开启）
  - `closeWindow()` 用 closeRequested 标志实现（SDL 无 window->close 等价物）
  - 字号绑定的 TTF_Font 由 RendererSDL3 按整数 baseSize 缓存（AssetManager 只给字体路径）；
    drawText 光栅化 baseSize + 残差并入 scale 的策略与脚手架一致（动画不重新光栅化）
  - 帧率限制：present 后按 PerformanceCounter 睡眠剩余节拍（SDL_DelayNS）
- **验证**：编译通过（`EngineSDL3` 目标）；主目标行为完全不变（SFML 路径未动）
- **原子性保证**：SDL3 代码全在 `#if defined(ENGINE_SDL3)` 之后且不进主目标，默认路径零影响

#### Step 10 — 切换：SDL3 生效（单一目的提交）✅ 2026-08-23 完成
- [x] `Core/Types.h` 别名换成自研实现（Vec2/Vec3 模板 + 类型间隐式转换构造、Rect、
      Color 常量、Time 微秒存储——运算符集与 SFML 语义逐条对齐，游戏层零改动）
- [x] CMake：`SDL3_ACTIVE`（=BUILD_WITH_SDL3 且客户端构建）时主目标排除
      `RendererSFML.cpp`/`EventConvertSFML.cpp`/`AssetManager.cpp`，编译 SDL3 两实现文件，
      定义 `ENGINE_SDL3`，链接 SDL3 全家 + sfml-network/system（Network/ 仍用）；
      **关 BUILD_WITH_SDL3 即完整回滚 SFML 路径**（脚手架已适配自研类型，保持可编译）
- [x] 音频播放侧：MarioController 双分支（`#ifdef ENGINE_SDL3`）——
      常驻 `MIX_Track` 绑预解码音频（=setBuffer），播放 = `MIX_StopTrack(0)+MIX_PlayTrack(0)`
      （=stop+play 重播语义），析构 `MIX_DestroyTrack`；SFML 分支原样保留至 Step 11
- [x] `Renderer.h` 删除 `sf::RenderWindow*` 私有成员（窗口彻底成实现细节）；
      脚手架改文件内静态指针 + eng↔sf 显式转换（toSf/toSfV）
- [x] `NetworkManager` 三个 update 签名 `sf::Time`→`eng::Time`（Types.h 自研后的类型裂缝，
      调用方传的正是 eng::Time；两后端下等价）
- [x] `GameEngine.cpp` 计时构造 `sf::seconds`→`eng::Time::seconds`（两模式均合法）
- [x] `AssetManagerSDL3` 混音器初始化补 `SDL_Init(SDL_INIT_AUDIO)`（资源加载在窗口创建前）
- [x] EngineSDL3 编译验证目标移除（主目标直接编译两实现文件）
- **验证**：**逐场景跑全部功能**——Menu / GameScene / GameScene3D / Settings / SuperMario /
  PhysicsTest（A/D 移动、Space 跳、R 重置、鼠标放球、相机拖动、文字输入、BGM/音效、
  物理调试绘制）—— SDL3 版首次全量回归
- **原子性保证**：所有调用方早已走 Renderer/Handle 抽象，本提交只动
  实现文件选择 + Types.h + 音频双分支；出问题关 BUILD_WITH_SDL3 即回 SFML

#### Step 11 — 删除脚手架与 SFML 残留 ✅ 2026-08-23 完成（迁移收官）
- [x] 删除 4 文件：`RendererSFML.cpp`、`EventConvertSFML.h/.cpp`、`AssetManager.cpp`（SFML 版）
- [x] `AssetManager.h` 去 `#ifdef ENGINE_SDL3` 双分支 → SDL3 单实现；
      `MarioController.h/.cpp` 去 SFML 音频分支；两 SDL3 实现文件去宏去守卫
- [x] `ENGINE_SDL3` 宏全仓清零（源码与 CMake 均不再定义/引用）
- [x] CMake 单后端终态：客户端无条件拉取 SDL3 全家并链接（`BUILD_WITH_SDL3` 开关删除）；
      SFML 只剩 network+system（`find_package` 组件收窄、graphics/window/audio 的
      FetchContent 开关与链接段全部删除，连带 OpenAL/flac/vorbis/freetype 传递依赖清零）
- [x] 全仓验证：`grep "#include <SFML" src/` 仅 Network/ 两文件（范围外）；
      `grep "ENGINE_SDL3" src/ CMakeLists.txt` 零命中
- **验证**：客户端+服务端双版本编译通过 + 全场景回归
- **收官状态**：窗口/渲染/输入/音频 = SDL3；网络 = sfml-network（范围外）；
  渲染栈 SFML 依赖（graphics/window/audio/system 链接与其传递依赖）全部移除

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
