//
// Created by MINEC on 2026/8/23.
//
#pragma once

#include <string>
#include <vector>

#include "Core/Types.h"
#include "Core/Event.h"
#include "Render/Handles.h"

// 引擎渲染器（SDL3 迁移 Step 5）
// 职责：窗口 + 事件泵 + 渲染命令 三合一。
// 头文件零第三方 include；实现文件整体替换（脚手架期 RendererSFML.cpp，
// SDL3 终态 RendererSDL3.cpp），游戏层代码不随实现变化。
namespace eng {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // ── 窗口 ──
    bool createWindow(Vec2u size, const std::string& title);
    void destroyWindow();
    void closeWindow();                      // 请求关闭（随后 isWindowOpen 变 false）
    [[nodiscard]] bool isWindowOpen() const;
    [[nodiscard]] Vec2u getSize() const;
    void setSize(Vec2u size);                 // 运行时改窗口尺寸（3D 场景进出场切换分辨率用）
    void setFramerateLimit(unsigned fps);

    // ── 事件泵（内部完成 sf::Event / SDL_Event → EngineEvent 转换，引擎不关心的事件被跳过）──
    bool pollEvent(EngineEvent& out);

    // ── 帧控制 ──
    void clear(Color c = Color::Black);
    void present();

    // ── 绘制命令 ──
    // 统一旋转语义：矩形为未旋转时的可视区域，origin 为矩形内支点（自左上角起算），
    // rotationDeg 绕支点旋转（SDL_RenderCopyRotF 的 center 与此一致）
    // src：源纹理上的矩形（像素）；dst：目标区域（世界/屏幕坐标，受相机影响）
    void drawTexture(TextureHandle h, const FloatRect& src, const FloatRect& dst,
                     float rotationDeg = 0.f, Vec2f origin = {},
                     Color tint = Color::White, bool flipX = false);
    void drawRect(const FloatRect& r, Color fillColor, bool filled = true,
                  float outlineThickness = 0.f, Color outlineColor = Color::White,
                  float rotationDeg = 0.f, Vec2f origin = {});
    void drawLine(Vec2f a, Vec2f b, Color c);
    void drawLines(const std::vector<Vec2f>& points, Color c);     // 折线
    void drawPolygon(const std::vector<Vec2f>& points, Color c);   // 实心凸多边形
    void drawCircle(Vec2f center, float radius, Color c, bool filled = true,
                    float outlineThickness = 0.f, Color outlineColor = Color::White);
    // 圆角矩形：填充色 + 可选描边（先画外扩描边层再画填充层）
    void drawRoundedRect(const FloatRect& r, float radius, Color fillColor,
                         float outlineThickness = 0.f, Color outlineColor = Color::White);
    // text 按 UTF-8 解释（中文可直接传入）。
    // size = 光栅化字号（字形按此尺寸渲染一次）；scale = 变换缩放（GPU 拉伸，不重新光栅化）。
    // 连续缩放动画（如按钮 hover）应固定 size、动画 scale——逐帧变 size 会不断重新光栅化，
    // 字形 hinting/字距跳动产生"变形"感（与 SDL_ttf 期同一策略）
    void drawText(FontHandle h, const std::string& text, Vec2f pos,
                  float size, Color c, float scale = 1.f);
    // 文本尺寸测量（用于居中排版；与 drawText 同一字体管线，scale 语义同上）
    [[nodiscard]] Vec2f measureText(FontHandle h, const std::string& text, float size, float scale = 1.f);

    // ── 相机 ──
    struct CameraState {
        Vec2f center;
        Vec2f size;
        float zoom = 1.f;
    };
    void setCamera(Vec2f center, Vec2f size, float zoom = 1.f);
    void setCamera(const CameraState& c) { setCamera(c.center, c.size, c.zoom); }
    [[nodiscard]] CameraState getCamera() const;   // 保存当前相机（配合 setCamera 恢复）
    void resetCamera();                            // 屏幕坐标系（窗口大小，左上原点）
    [[nodiscard]] Vec2f screenToWorld(Vec2i screenPos) const;
};

} // namespace eng
