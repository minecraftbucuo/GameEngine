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
namespace sf {
    class RenderWindow;   // 仅供临时过渡 API 前向声明，Step 6e 删除
}

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
    // text 按 UTF-8 解释（中文可直接传入）
    void drawText(FontHandle h, const std::string& text, Vec2f pos,
                  unsigned size, Color c);
    // 文本尺寸测量（用于居中排版；与 drawText 同一字体管线）
    [[nodiscard]] Vec2f measureText(FontHandle h, const std::string& text, unsigned size);

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

    // 【临时过渡 API — Step 6e 删除】
    // 供尚未迁移到 Renderer 绘制命令的旧渲染路径（Scene::getWindow 等）取底层窗口
    // 类内 inline 定义：服务端构建（无 RendererSFML.cpp）被 vtable 引用时也不产生外部符号依赖
    [[nodiscard]] sf::RenderWindow* getSfmlWindow() const {
        return window;
    }

private:
    sf::RenderWindow* window{};   // 脚手架期内部持有；SDL3 期换 SDL_Window*/SDL_Renderer*
};

} // namespace eng
