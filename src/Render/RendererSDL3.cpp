//
// Created by MINEC on 2026/8/23.
//
#ifndef SERVER_BUILD

// Renderer 的 SDL3 实现（迁移 Step 11 起唯一后端）：
// 窗口 + 事件泵 + 绘制命令 + 相机 + 文字缓存。
// 历史语义对齐注释（"与脚手架一致"等）指向迁移期已删除的 RendererSFML.cpp。

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

#include "Render/Renderer.h"
#include "Core/Input.h"
#include "Manager/AssetManager.h"
#include "Manager/Logger.h"

namespace eng {

namespace {

constexpr int   kCircleSegments = 32;   // SFML CircleShape 默认 30 段，取近似的平滑值
constexpr int   kCornerSegments = 8;    // 圆角矩形每角弧段数（与脚手架一致）
constexpr float kPi = 3.14159265358979f;

struct TextEntry {
    SDL_Texture* texture = nullptr;
    float w = 0.f;
    float h = 0.f;
    std::list<std::string>::iterator lru{};
};

// 引擎单实例渲染状态（GameEngine 持有唯一 Renderer，等价脚手架的私有 window 成员）
struct Sdl3State {
    SDL_Window*   window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool          closeRequested = false;

    // 相机（cameraActive=false 等价 SFML 默认视图：屏幕像素坐标 1:1）
    bool  cameraActive = false;
    Vec2f camCenter{};
    Vec2f camSize{};
    float camZoom = 1.f;

    // 帧率限制（present 后睡眠剩余节拍）
    unsigned fpsLimit = 0;
    bool     presentTimerInit = false;
    Uint64   lastPresent = 0;   // SDL_GetPerformanceCounter 计数

    // 待补发事件队列（退格合成 TextEntered(8)，见 pollEvent）
    std::vector<EngineEvent> pending;

    // 字体缓存：光栅化字号 → TTF_Font（TTF 字体对象与字号绑定，按需打开）
    std::unordered_map<int, TTF_Font*> fonts;
    bool ttfInited = false;

    // 文字纹理缓存：key = "<baseSize>\x1F<text>" → 白字纹理（绘制时 ColorMod 着色），LRU 淘汰
    static constexpr size_t kTextCacheCap = 128;
    std::unordered_map<std::string, TextEntry> textCache;
    std::list<std::string> textLru;
};
Sdl3State g;

// ── 基础工具 ──

float windowWidth() {
    int w = 0;
    SDL_GetWindowSize(g.window, &w, nullptr);
    return static_cast<float>(w);
}

float windowHeight() {
    int h = 0;
    SDL_GetWindowSize(g.window, nullptr, &h);
    return static_cast<float>(h);
}

// 相机有效可视区尺寸：zoom 与 sf::View::zoom 一致（size * zoom，zoom>1 视野更大、物体更小）
Vec2f camEffectiveSize() {
    return { g.camSize.x * g.camZoom, g.camSize.y * g.camZoom };
}

// 相机世界→屏幕缩放系数
Vec2f camScale() {
    if (!g.cameraActive) return {1.f, 1.f};
    const Vec2f s = camEffectiveSize();
    return { windowWidth() / s.x, windowHeight() / s.y };
}

// 世界 → 屏幕（相机未激活时恒等，等价 SFML 默认视图）
Vec2f worldToScreen(const Vec2f p) {
    if (!g.cameraActive) return p;
    const Vec2f s = camEffectiveSize();
    const Vec2f leftTop(g.camCenter.x - s.x * 0.5f, g.camCenter.y - s.y * 0.5f);
    return { (p.x - leftTop.x) * (windowWidth() / s.x),
             (p.y - leftTop.y) * (windowHeight() / s.y) };
}

SDL_Color toSDLColor(const Color c) { return { c.r, c.g, c.b, c.a }; }
SDL_FColor toFColor(const Color c) { return { c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f }; }

// 三角形批提交（SDL_RenderGeometry 只认三角形）
void fillTriangles(const std::vector<SDL_Vertex>& verts, const std::vector<int>& indices) {
    if (verts.empty() || indices.empty()) return;
    SDL_RenderGeometry(g.renderer, nullptr,
                       verts.data(), static_cast<int>(verts.size()),
                       indices.data(), static_cast<int>(indices.size()));
}

// 迕 pivot 旋转（角度制、顺时针；与 sf::Transform::rotate 同约定，y 轴向下）
Vec2f rotateAround(const Vec2f p, const Vec2f pivot, const float deg) {
    if (deg == 0.f) return p;
    const float a = deg * kPi / 180.f;
    const float c = std::cos(a), s = std::sin(a);
    const Vec2f d(p.x - pivot.x, p.y - pivot.y);
    return { pivot.x + d.x * c - d.y * s, pivot.y + d.x * s + d.y * c };
}

// 取 UTF-8 串首码点（SDL 文本事件 → EngineEvent.codepoint；非法序列返回 U+FFFD）
char32_t utf8FirstCodepoint(const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    if (!p || !*p) return 0;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
        return static_cast<char32_t>(((p[0] & 0x1F) << 6) | (p[1] & 0x3F));
    if ((p[0] & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80)
        return static_cast<char32_t>(((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F));
    if ((p[0] & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80)
        return static_cast<char32_t>(((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F));
    return 0xFFFD;
}

// ── Key ↔ SDL_Scancode（物理键位，与 EventConvertSFML 的表逐项对齐）──

SDL_Scancode toScancode(const Key key) {
    switch (key) {
        case Key::A: return SDL_SCANCODE_A;
        case Key::B: return SDL_SCANCODE_B;
        case Key::C: return SDL_SCANCODE_C;
        case Key::D: return SDL_SCANCODE_D;
        case Key::E: return SDL_SCANCODE_E;
        case Key::F: return SDL_SCANCODE_F;
        case Key::G: return SDL_SCANCODE_G;
        case Key::H: return SDL_SCANCODE_H;
        case Key::I: return SDL_SCANCODE_I;
        case Key::J: return SDL_SCANCODE_J;
        case Key::K: return SDL_SCANCODE_K;
        case Key::L: return SDL_SCANCODE_L;
        case Key::M: return SDL_SCANCODE_M;
        case Key::N: return SDL_SCANCODE_N;
        case Key::O: return SDL_SCANCODE_O;
        case Key::P: return SDL_SCANCODE_P;
        case Key::Q: return SDL_SCANCODE_Q;
        case Key::R: return SDL_SCANCODE_R;
        case Key::S: return SDL_SCANCODE_S;
        case Key::T: return SDL_SCANCODE_T;
        case Key::U: return SDL_SCANCODE_U;
        case Key::V: return SDL_SCANCODE_V;
        case Key::W: return SDL_SCANCODE_W;
        case Key::X: return SDL_SCANCODE_X;
        case Key::Y: return SDL_SCANCODE_Y;
        case Key::Z: return SDL_SCANCODE_Z;
        case Key::Num0: return SDL_SCANCODE_0;
        case Key::Num1: return SDL_SCANCODE_1;
        case Key::Num2: return SDL_SCANCODE_2;
        case Key::Num3: return SDL_SCANCODE_3;
        case Key::Num4: return SDL_SCANCODE_4;
        case Key::Num5: return SDL_SCANCODE_5;
        case Key::Num6: return SDL_SCANCODE_6;
        case Key::Num7: return SDL_SCANCODE_7;
        case Key::Num8: return SDL_SCANCODE_8;
        case Key::Num9: return SDL_SCANCODE_9;
        case Key::F1: return SDL_SCANCODE_F1;
        case Key::F2: return SDL_SCANCODE_F2;
        case Key::F3: return SDL_SCANCODE_F3;
        case Key::F4: return SDL_SCANCODE_F4;
        case Key::F5: return SDL_SCANCODE_F5;
        case Key::F6: return SDL_SCANCODE_F6;
        case Key::F7: return SDL_SCANCODE_F7;
        case Key::F8: return SDL_SCANCODE_F8;
        case Key::F9: return SDL_SCANCODE_F9;
        case Key::F10: return SDL_SCANCODE_F10;
        case Key::F11: return SDL_SCANCODE_F11;
        case Key::F12: return SDL_SCANCODE_F12;
        case Key::Escape: return SDL_SCANCODE_ESCAPE;
        case Key::Enter: return SDL_SCANCODE_RETURN;
        case Key::Space: return SDL_SCANCODE_SPACE;
        case Key::Tab: return SDL_SCANCODE_TAB;
        case Key::Backspace: return SDL_SCANCODE_BACKSPACE;
        case Key::Insert: return SDL_SCANCODE_INSERT;
        case Key::Delete: return SDL_SCANCODE_DELETE;
        case Key::Home: return SDL_SCANCODE_HOME;
        case Key::End: return SDL_SCANCODE_END;
        case Key::PageUp: return SDL_SCANCODE_PAGEUP;
        case Key::PageDown: return SDL_SCANCODE_PAGEDOWN;
        case Key::LShift: return SDL_SCANCODE_LSHIFT;
        case Key::RShift: return SDL_SCANCODE_RSHIFT;
        case Key::LCtrl: return SDL_SCANCODE_LCTRL;
        case Key::RCtrl: return SDL_SCANCODE_RCTRL;
        case Key::LAlt: return SDL_SCANCODE_LALT;
        case Key::RAlt: return SDL_SCANCODE_RALT;
        case Key::Up: return SDL_SCANCODE_UP;
        case Key::Down: return SDL_SCANCODE_DOWN;
        case Key::Left: return SDL_SCANCODE_LEFT;
        case Key::Right: return SDL_SCANCODE_RIGHT;
        case Key::Comma: return SDL_SCANCODE_COMMA;
        case Key::Period: return SDL_SCANCODE_PERIOD;
        case Key::Slash: return SDL_SCANCODE_SLASH;
        case Key::Semicolon: return SDL_SCANCODE_SEMICOLON;
        case Key::Apostrophe: return SDL_SCANCODE_APOSTROPHE;
        case Key::LBracket: return SDL_SCANCODE_LEFTBRACKET;
        case Key::RBracket: return SDL_SCANCODE_RIGHTBRACKET;
        case Key::Minus: return SDL_SCANCODE_MINUS;
        case Key::Equal: return SDL_SCANCODE_EQUALS;
        case Key::Backquote: return SDL_SCANCODE_GRAVE;
        default: return SDL_SCANCODE_UNKNOWN;
    }
}

Key fromScancode(const SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_A: return Key::A;
        case SDL_SCANCODE_B: return Key::B;
        case SDL_SCANCODE_C: return Key::C;
        case SDL_SCANCODE_D: return Key::D;
        case SDL_SCANCODE_E: return Key::E;
        case SDL_SCANCODE_F: return Key::F;
        case SDL_SCANCODE_G: return Key::G;
        case SDL_SCANCODE_H: return Key::H;
        case SDL_SCANCODE_I: return Key::I;
        case SDL_SCANCODE_J: return Key::J;
        case SDL_SCANCODE_K: return Key::K;
        case SDL_SCANCODE_L: return Key::L;
        case SDL_SCANCODE_M: return Key::M;
        case SDL_SCANCODE_N: return Key::N;
        case SDL_SCANCODE_O: return Key::O;
        case SDL_SCANCODE_P: return Key::P;
        case SDL_SCANCODE_Q: return Key::Q;
        case SDL_SCANCODE_R: return Key::R;
        case SDL_SCANCODE_S: return Key::S;
        case SDL_SCANCODE_T: return Key::T;
        case SDL_SCANCODE_U: return Key::U;
        case SDL_SCANCODE_V: return Key::V;
        case SDL_SCANCODE_W: return Key::W;
        case SDL_SCANCODE_X: return Key::X;
        case SDL_SCANCODE_Y: return Key::Y;
        case SDL_SCANCODE_Z: return Key::Z;
        case SDL_SCANCODE_0: return Key::Num0;
        case SDL_SCANCODE_1: return Key::Num1;
        case SDL_SCANCODE_2: return Key::Num2;
        case SDL_SCANCODE_3: return Key::Num3;
        case SDL_SCANCODE_4: return Key::Num4;
        case SDL_SCANCODE_5: return Key::Num5;
        case SDL_SCANCODE_6: return Key::Num6;
        case SDL_SCANCODE_7: return Key::Num7;
        case SDL_SCANCODE_8: return Key::Num8;
        case SDL_SCANCODE_9: return Key::Num9;
        case SDL_SCANCODE_F1: return Key::F1;
        case SDL_SCANCODE_F2: return Key::F2;
        case SDL_SCANCODE_F3: return Key::F3;
        case SDL_SCANCODE_F4: return Key::F4;
        case SDL_SCANCODE_F5: return Key::F5;
        case SDL_SCANCODE_F6: return Key::F6;
        case SDL_SCANCODE_F7: return Key::F7;
        case SDL_SCANCODE_F8: return Key::F8;
        case SDL_SCANCODE_F9: return Key::F9;
        case SDL_SCANCODE_F10: return Key::F10;
        case SDL_SCANCODE_F11: return Key::F11;
        case SDL_SCANCODE_F12: return Key::F12;
        case SDL_SCANCODE_ESCAPE: return Key::Escape;
        case SDL_SCANCODE_RETURN: return Key::Enter;
        case SDL_SCANCODE_SPACE: return Key::Space;
        case SDL_SCANCODE_TAB: return Key::Tab;
        case SDL_SCANCODE_BACKSPACE: return Key::Backspace;
        case SDL_SCANCODE_INSERT: return Key::Insert;
        case SDL_SCANCODE_DELETE: return Key::Delete;
        case SDL_SCANCODE_HOME: return Key::Home;
        case SDL_SCANCODE_END: return Key::End;
        case SDL_SCANCODE_PAGEUP: return Key::PageUp;
        case SDL_SCANCODE_PAGEDOWN: return Key::PageDown;
        case SDL_SCANCODE_LSHIFT: return Key::LShift;
        case SDL_SCANCODE_RSHIFT: return Key::RShift;
        case SDL_SCANCODE_LCTRL: return Key::LCtrl;
        case SDL_SCANCODE_RCTRL: return Key::RCtrl;
        case SDL_SCANCODE_LALT: return Key::LAlt;
        case SDL_SCANCODE_RALT: return Key::RAlt;
        case SDL_SCANCODE_UP: return Key::Up;
        case SDL_SCANCODE_DOWN: return Key::Down;
        case SDL_SCANCODE_LEFT: return Key::Left;
        case SDL_SCANCODE_RIGHT: return Key::Right;
        case SDL_SCANCODE_COMMA: return Key::Comma;
        case SDL_SCANCODE_PERIOD: return Key::Period;
        case SDL_SCANCODE_SLASH: return Key::Slash;
        case SDL_SCANCODE_SEMICOLON: return Key::Semicolon;
        case SDL_SCANCODE_APOSTROPHE: return Key::Apostrophe;
        case SDL_SCANCODE_LEFTBRACKET: return Key::LBracket;
        case SDL_SCANCODE_RIGHTBRACKET: return Key::RBracket;
        case SDL_SCANCODE_MINUS: return Key::Minus;
        case SDL_SCANCODE_EQUALS: return Key::Equal;
        case SDL_SCANCODE_GRAVE: return Key::Backquote;
        default: return Key::Unknown;
    }
}

MouseButton fromSDLMouseButton(const Uint8 button) {
    switch (button) {
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_RIGHT: return MouseButton::Right;
        case SDL_BUTTON_LEFT:
        default: return MouseButton::Left;   // 与脚手架一致：未识别按钮归为左键
    }
}

// 圆角矩形填充（脚手架 fillRoundedRect 的 SDL 移植：3 直边矩形 + 四角扇形）
void fillRoundedRectSDL(const FloatRect& r, const float radius, const Color color) {
    const float rad = std::min(radius, std::min(r.width, r.height) * 0.5f);
    const float x = r.left, y = r.top, w = r.width, h = r.height;
    const Vec2f sc = camScale();
    const Vec2f tl = worldToScreen({x, y});
    const SDL_Color col = toSDLColor(color);

    // 中间水平矩形 + 左右矩形（覆盖四角之间的直边区域）
    SDL_SetRenderDrawColor(g.renderer, col.r, col.g, col.b, col.a);
    const SDL_FRect rects[3] = {
        { tl.x + rad * sc.x, tl.y, (w - 2 * rad) * sc.x, h * sc.y },
        { tl.x,              tl.y + rad * sc.y, rad * sc.x, (h - 2 * rad) * sc.y },
        { tl.x + (w - rad) * sc.x, tl.y + rad * sc.y, rad * sc.x, (h - 2 * rad) * sc.y },
    };
    for (const auto& rect : rects) SDL_RenderFillRect(g.renderer, &rect);

    // 四角扇形（角度：0=右，PI/2=上；y 向下故 sin 取反，与脚手架一致）
    const struct { Vec2f center; float a0, a1; } corners[] = {
        {{x + w - rad, y + rad},      0.f,          kPi / 2.f},     // 右上
        {{x + w - rad, y + h - rad},  3 * kPi / 2.f, 2 * kPi},      // 右下
        {{x + rad,      y + h - rad}, kPi,          3 * kPi / 2.f}, // 左下
        {{x + rad,      y + rad},     kPi / 2.f,    kPi},           // 左上
    };
    const SDL_FColor fc = toFColor(color);
    for (const auto& corner : corners) {
        const Vec2f c = worldToScreen(corner.center);
        const float rx = rad * sc.x, ry = rad * sc.y;
        std::vector<SDL_Vertex> verts;
        std::vector<int> idx;
        verts.push_back({{c.x, c.y}, fc, {0.f, 0.f}});
        for (int i = 0; i <= kCornerSegments; ++i) {
            const float angle = corner.a0 + (corner.a1 - corner.a0) * i / kCornerSegments;
            verts.push_back({{ c.x + std::cos(angle) * rx,
                               c.y - std::sin(angle) * ry }, fc, {0.f, 0.f}});
        }
        for (int i = 1; i <= kCornerSegments; ++i) idx.insert(idx.end(), {0, i, i + 1});
        fillTriangles(verts, idx);
    }
}

// 字号 → TTF_Font（懒打开 + 缓存；打开失败缓存 nullptr 防止反复重试）
TTF_Font* fontForSize(const float size) {
    const int base = std::max(1, static_cast<int>(std::lround(size)));
    if (const auto it = g.fonts.find(base); it != g.fonts.end()) return it->second;
    if (!g.ttfInited) {
        if (!TTF_Init()) {
            LOG_ERROR_FMT("TTF_Init failed: {}", SDL_GetError());
            g.fonts.emplace(base, nullptr);
            return nullptr;
        }
        g.ttfInited = true;
    }
    const std::string& path = AssetManager::getInstance().getFontPath(
        AssetManager::getInstance().getFontHandle());
    TTF_Font* font = TTF_OpenFont(path.c_str(), static_cast<float>(base));
    if (!font) LOG_ERROR_FMT("TTF_OpenFont({}) failed: {}", path, SDL_GetError());
    g.fonts.emplace(base, font);
    return font;
}

// 文字纹理缓存查找（未命中则光栅化白字纹理；LRU 淘汰）
TextEntry* textTextureFor(const int baseSize, const std::string& text) {
    const std::string key = std::to_string(baseSize) + '\x1f' + text;
    if (const auto it = g.textCache.find(key); it != g.textCache.end()) {
        g.textLru.splice(g.textLru.begin(), g.textLru, it->second.lru);   // touch LRU
        return &it->second;
    }
    TTF_Font* font = fontForSize(static_cast<float>(baseSize));
    if (!font) return nullptr;
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(),
                                                  SDL_Color{255, 255, 255, 255});
    if (!surface) {
        LOG_ERROR_FMT("TTF_RenderText_Blended failed: {}", SDL_GetError());
        return nullptr;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(g.renderer, surface);
    float w = 0.f, h = 0.f;
    if (texture) SDL_GetTextureSize(texture, &w, &h);
    SDL_DestroySurface(surface);
    if (!texture) {
        LOG_ERROR_FMT("SDL_CreateTextureFromSurface(text) failed: {}", SDL_GetError());
        return nullptr;
    }
    g.textLru.push_front(key);
    TextEntry entry{texture, w, h, g.textLru.begin()};
    const auto [it, ok] = g.textCache.emplace(key, entry);
    // LRU 淘汰
    while (g.textLru.size() > Sdl3State::kTextCacheCap) {
        const std::string& victim = g.textLru.back();
        if (const auto vit = g.textCache.find(victim); vit != g.textCache.end()) {
            SDL_DestroyTexture(vit->second.texture);
            g.textCache.erase(vit);
        }
        g.textLru.pop_back();
    }
    return ok ? &it->second : nullptr;
}

} // namespace

// ── 窗口 ──

Renderer::~Renderer() {
    destroyWindow();
}

bool Renderer::createWindow(const Vec2u size, const std::string& title) {
    if (g.window) return true;   // 已创建
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR_FMT("SDL_Init(VIDEO) failed: {}", SDL_GetError());
        return false;
    }
    g.window = SDL_CreateWindow(title.c_str(), static_cast<int>(size.x),
                                static_cast<int>(size.y), SDL_WINDOW_RESIZABLE);
    if (!g.window) {
        LOG_ERROR_FMT("SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }
    g.renderer = SDL_CreateRenderer(g.window, nullptr);
    if (!g.renderer) {
        LOG_ERROR_FMT("SDL_CreateRenderer failed: {}", SDL_GetError());
        SDL_DestroyWindow(g.window);
        g.window = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }
    // 半透明绘制全局开启（SFML 默认 blend 语义）
    SDL_SetRenderDrawBlendMode(g.renderer, SDL_BLENDMODE_BLEND);
    // SDL 需显式开启文本输入，TEXT_INPUT 事件与 IME 才会产生（SFML 恒开启）
    SDL_StartTextInput(g.window);
    g.closeRequested = false;
    g.cameraActive = false;
    g.presentTimerInit = false;
    return true;
}

void Renderer::destroyWindow() {
    if (!g.window && !g.renderer) return;
    for (auto& [key, entry] : g.textCache) SDL_DestroyTexture(entry.texture);
    g.textCache.clear();
    g.textLru.clear();
    for (auto& [size, font] : g.fonts) TTF_CloseFont(font);
    g.fonts.clear();
    if (g.ttfInited) { TTF_Quit(); g.ttfInited = false; }
    if (g.renderer) { SDL_DestroyRenderer(g.renderer); g.renderer = nullptr; }
    if (g.window) { SDL_DestroyWindow(g.window); g.window = nullptr; }
    g.closeRequested = false;
    g.cameraActive = false;
    g.pending.clear();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Renderer::closeWindow() {
    g.closeRequested = true;   // SFML window->close() 语义：随后 isWindowOpen 为 false
}

bool Renderer::isWindowOpen() const {
    return g.window && !g.closeRequested;
}

Vec2u Renderer::getSize() const {
    if (!g.window) return Vec2u(0, 0);
    int w = 0, h = 0;
    SDL_GetWindowSize(g.window, &w, &h);
    return Vec2u(static_cast<unsigned>(w), static_cast<unsigned>(h));
}

void Renderer::setSize(const Vec2u size) {
    if (g.window) SDL_SetWindowSize(g.window, static_cast<int>(size.x), static_cast<int>(size.y));
}

void Renderer::setFramerateLimit(const unsigned fps) {
    g.fpsLimit = fps;
    g.presentTimerInit = false;
}

// ── 事件泵 ──

bool Renderer::pollEvent(EngineEvent& out) {
    if (!g.window) return false;
    // 先冲刷补发队列（如退格合成的 TextEntered(8)）
    if (!g.pending.empty()) {
        out = g.pending.front();
        g.pending.erase(g.pending.begin());
        return true;
    }
    SDL_Event e{};
    while (SDL_PollEvent(&e)) {
        EngineEvent ev{};
        bool ok = true;
        switch (e.type) {
            case SDL_EVENT_QUIT:                     // 与 CLOSE_REQUESTED 同归 WindowClose
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                ev.type = EventType::WindowClose;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                ev.type = EventType::WindowResize;
                ev.newSize = Vec2u(static_cast<unsigned>(e.window.data1),
                                   static_cast<unsigned>(e.window.data2));
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                ev.type = EventType::GainFocus;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                ev.type = EventType::LostFocus;
                break;
            case SDL_EVENT_KEY_DOWN:
                ev.type = EventType::KeyPress;
                ev.key = fromScancode(e.key.scancode);
                // SFML 会把退格同时作为 TextEntered(8) 投递；SDL 文本事件只含可打印字符，
                // 在此补发（跳过按住 repeat，SFML 的 repeat 同样不产生新 TextEntered）
                if (e.key.scancode == SDL_SCANCODE_BACKSPACE && !e.key.repeat) {
                    EngineEvent bs{};
                    bs.type = EventType::TextEntered;
                    bs.codepoint = 8;
                    g.pending.push_back(bs);
                }
                break;
            case SDL_EVENT_KEY_UP:
                ev.type = EventType::KeyRelease;
                ev.key = fromScancode(e.key.scancode);
                break;
            case SDL_EVENT_TEXT_INPUT: {
                const char32_t cp = utf8FirstCodepoint(e.text.text);
                if (cp == 0) { ok = false; break; }   // 空串跳过，继续轮询
                ev.type = EventType::TextEntered;
                ev.codepoint = cp;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                ev.type = EventType::MouseButtonPress;
                ev.mouseButton = fromSDLMouseButton(e.button.button);
                ev.mousePos = Vec2i(static_cast<int>(std::lround(e.button.x)),
                                    static_cast<int>(std::lround(e.button.y)));
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                ev.type = EventType::MouseButtonRelease;
                ev.mouseButton = fromSDLMouseButton(e.button.button);
                ev.mousePos = Vec2i(static_cast<int>(std::lround(e.button.x)),
                                    static_cast<int>(std::lround(e.button.y)));
                break;
            case SDL_EVENT_MOUSE_MOTION:
                ev.type = EventType::MouseMove;
                ev.mousePos = Vec2i(static_cast<int>(std::lround(e.motion.x)),
                                    static_cast<int>(std::lround(e.motion.y)));
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                ev.type = EventType::MouseWheel;
                ev.wheelDelta = e.wheel.y;   // 正=向上滚，与 SFML 同号
                ev.mousePos = Vec2i(static_cast<int>(std::lround(e.wheel.mouse_x)),
                                    static_cast<int>(std::lround(e.wheel.mouse_y)));
                break;
            default:
                // SDL 特有且引擎不关心的事件（设备热插拔/剪贴板等），跳过后继续轮询
                ok = false;
                break;
        }
        if (ok) {
            out = ev;
            return true;
        }
    }
    return false;
}

// ── 帧控制 ──

void Renderer::clear(const Color c) {
    if (!g.renderer) return;
    SDL_SetRenderDrawColor(g.renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(g.renderer);
}

void Renderer::present() {
    if (!g.renderer) return;
    SDL_RenderPresent(g.renderer);
    if (g.fpsLimit == 0) return;
    const Uint64 freq = SDL_GetPerformanceFrequency();
    const Uint64 target = freq / g.fpsLimit;   // 每帧节拍（计数器单位）
    const Uint64 now = SDL_GetPerformanceCounter();
    if (!g.presentTimerInit) {
        g.presentTimerInit = true;
        g.lastPresent = now;
        return;
    }
    if (const Uint64 elapsed = now - g.lastPresent; elapsed < target) {
        // tick → 纳秒换算（QPC 1 tick ≠ 1ns，漏乘会短睡百倍导致限帧失效）
        SDL_DelayNS((target - elapsed) * 1000000000ULL / freq);
    }
    g.lastPresent = SDL_GetPerformanceCounter();
}

// ── 绘制命令 ──

void Renderer::drawTexture(const TextureHandle h, const FloatRect& src, const FloatRect& dst,
                           const float rotationDeg, const Vec2f origin, const Color tint,
                           const bool flipX) {
    if (!g.renderer || !h.isValid()) return;
    if (src.width <= 0.f || src.height <= 0.f) return;
    SDL_Texture* texture = AssetManager::getInstance().getTexture(h);
    if (!texture) return;
    // src 原样采样（镜像只经 flip 参数，绝不动 src——图集语义，脚手架同规则）
    const SDL_FRect sr{ src.left, src.top, src.width, src.height };
    SDL_FRect dr{ dst.left, dst.top, dst.width, dst.height };
    // center 为 dst 内旋转支点（自 dst 左上角起算）——与脚手架 origin 语义一致
    SDL_FPoint ctr{ origin.x, origin.y };
    if (g.cameraActive) {
        const Vec2f sc = camScale();
        const Vec2f tl = worldToScreen({dst.left, dst.top});
        dr = { tl.x, tl.y, dst.width * sc.x, dst.height * sc.y };
        ctr = { origin.x * sc.x, origin.y * sc.y };
    }
    SDL_SetTextureColorMod(texture, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(texture, tint.a);
    SDL_RenderTextureRotated(g.renderer, texture, &sr, &dr, rotationDeg, &ctr,
                             flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void Renderer::drawRect(const FloatRect& r, const Color fillColor, const bool filled,
                        const float outlineThickness, const Color outlineColor,
                        const float rotationDeg, const Vec2f origin) {
    if (!g.renderer) return;
    const float l = r.left, t = r.top, w = r.width, h = r.height;
    const Vec2f pivot(l + origin.x, t + origin.y);   // 旋转支点（世界坐标）
    // 先旋转（世界系）再映射屏幕（等价 SFML position+origin+rotation 的视图变换）
    const auto R = [&](const Vec2f p) { return worldToScreen(rotateAround(p, pivot, rotationDeg)); };

    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    const auto quad = [&](const Vec2f a, const Vec2f b, const Vec2f c, const Vec2f d,
                          const Color col) {
        const SDL_FColor fc = toFColor(col);
        const int base = static_cast<int>(verts.size());
        for (const Vec2f p : {a, b, c, d}) verts.push_back({{p.x, p.y}, fc, {0.f, 0.f}});
        idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };

    // 基础矩形四角
    const Vec2f i0(R({l, t})), i1(R({l + w, t})), i2(R({l + w, t + h})), i3(R({l, t + h}));
    // SFML 绘制顺序：先填充后描边（负厚度描边覆盖填充边缘，视觉一致）
    if (filled) quad(i0, i1, i2, i3, fillColor);
    if (outlineThickness != 0.f) {
        // 描边带 = 基础矩形与外扩/内缩矩形之间的 4 个梯形（e>0 外扩，e<0 内缩）
        const float e = outlineThickness;
        const Vec2f o0(R({l - e, t - e})), o1(R({l + w + e, t - e})),
                    o2(R({l + w + e, t + h + e})), o3(R({l - e, t + h + e}));
        quad(o0, o1, i1, i0, outlineColor);
        quad(o1, o2, i2, i1, outlineColor);
        quad(o2, o3, i3, i2, outlineColor);
        quad(o3, o0, i0, i3, outlineColor);
    }
    fillTriangles(verts, idx);
}

void Renderer::drawLine(const Vec2f a, const Vec2f b, const Color c) {
    if (!g.renderer) return;
    const Vec2f pa = worldToScreen(a), pb = worldToScreen(b);
    SDL_SetRenderDrawColor(g.renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(g.renderer, pa.x, pa.y, pb.x, pb.y);
}

void Renderer::drawLines(const std::vector<Vec2f>& points, const Color c) {
    if (!g.renderer || points.size() < 2) return;
    std::vector<SDL_FPoint> pts;
    pts.reserve(points.size());
    for (const auto& p : points) {
        const Vec2f s = worldToScreen(p);
        pts.push_back({s.x, s.y});
    }
    SDL_SetRenderDrawColor(g.renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLines(g.renderer, pts.data(), static_cast<int>(pts.size()));
}

void Renderer::drawPolygon(const std::vector<Vec2f>& points, const Color c) {
    if (!g.renderer || points.size() < 3) return;
    // 凸多边形扇形三角化（与 sf::ConvexShape 凸面渲染等价）
    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    const SDL_FColor fc = toFColor(c);
    verts.reserve(points.size());
    for (const auto& p : points) {
        const Vec2f s = worldToScreen(p);
        verts.push_back({{s.x, s.y}, fc, {0.f, 0.f}});
    }
    for (size_t i = 1; i + 1 < points.size(); ++i) {
        idx.insert(idx.end(), {0, static_cast<int>(i), static_cast<int>(i + 1)});
    }
    fillTriangles(verts, idx);
}

void Renderer::drawCircle(const Vec2f center, const float radius, const Color c, const bool filled,
                          const float outlineThickness, const Color outlineColor) {
    if (!g.renderer || radius <= 0.f) return;
    // 脚手架同规则：非填充且未显式给厚度时保证 1px 轮廓可见，颜色取填充色
    const float thickness = outlineThickness != 0.f ? outlineThickness : (filled ? 0.f : 1.f);
    const Color bandColor = outlineThickness != 0.f ? outlineColor : c;

    const Vec2f sc = camScale();
    const Vec2f ctr = worldToScreen(center);
    // 描边带内外半径（SDL/SFML 语义：正厚度向外扩，负厚度向内缩）
    const float ir = thickness >= 0.f ? radius : radius + thickness;
    const float or_ = thickness >= 0.f ? radius + thickness : radius;

    std::vector<SDL_Vertex> verts;
    std::vector<int> idx;
    const auto ringAt = [&](const int i, const float r) -> SDL_FPoint {
        const float a = 2.f * kPi * i / kCircleSegments;
        return { ctr.x + std::cos(a) * r * sc.x, ctr.y + std::sin(a) * r * sc.y };
    };
    const auto push = [&](const SDL_FPoint p, const Color col) {
        verts.push_back({{p.x, p.y}, toFColor(col), {0.f, 0.f}});
    };

    if (filled) {
        const SDL_FPoint c0{ctr.x, ctr.y};
        for (int i = 0; i < kCircleSegments; ++i) {
            const int base = static_cast<int>(verts.size());
            push(c0, c);
            push(ringAt(i, radius), c);
            push(ringAt(i + 1, radius), c);
            idx.insert(idx.end(), {base, base + 1, base + 2});
        }
    }
    if (thickness != 0.f) {
        for (int i = 0; i < kCircleSegments; ++i) {
            const int base = static_cast<int>(verts.size());
            push(ringAt(i, ir), bandColor);
            push(ringAt(i + 1, ir), bandColor);
            push(ringAt(i + 1, or_), bandColor);
            push(ringAt(i, or_), bandColor);
            idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }
    fillTriangles(verts, idx);
}

void Renderer::drawRoundedRect(const FloatRect& r, const float radius, const Color fillColor,
                               const float outlineThickness, const Color outlineColor) {
    if (!g.renderer) return;
    // 脚手架同策略：先画外扩描边层再画填充层覆盖
    if (outlineThickness > 0.f) {
        fillRoundedRectSDL(FloatRect(r.left - outlineThickness, r.top - outlineThickness,
                                     r.width + outlineThickness * 2, r.height + outlineThickness * 2),
                           radius + outlineThickness, outlineColor);
    }
    fillRoundedRectSDL(r, radius, fillColor);
}

void Renderer::drawText(const FontHandle h, const std::string& text, const Vec2f pos,
                        const float size, const Color c, const float scale) {
    if (!g.renderer || !h.isValid()) return;
    // 光栅化字号固定（size 四舍五入到整数，残差并入变换 scale）——与脚手架同策略：
    // 动画期间调用方保持 size 不变、只动 scale ⇒ 命中纹理缓存，GPU 平滑拉伸
    const int baseSize = std::max(1, static_cast<int>(std::lround(size)));
    TextEntry* entry = textTextureFor(baseSize, text);
    if (!entry) return;
    const float totalScale = size / static_cast<float>(baseSize) * scale;
    SDL_FRect dr{ pos.x, pos.y, entry->w * totalScale, entry->h * totalScale };
    if (g.cameraActive) {
        const Vec2f sc = camScale();
        const Vec2f tl = worldToScreen(pos);
        dr = { tl.x, tl.y, entry->w * totalScale * sc.x, entry->h * totalScale * sc.y };
    }
    // 缓存为白字纹理，按调用色着色（ColorMod 乘法调制）
    SDL_SetTextureColorMod(entry->texture, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(entry->texture, c.a);
    SDL_RenderTexture(g.renderer, entry->texture, nullptr, &dr);
}

Vec2f Renderer::measureText(const FontHandle h, const std::string& text, const float size,
                            const float scale) {
    if (!h.isValid()) return {};
    const int baseSize = std::max(1, static_cast<int>(std::lround(size)));
    TTF_Font* font = fontForSize(static_cast<float>(baseSize));
    if (!font) return {};
    int w = 0, hgt = 0;
    if (!TTF_GetStringSize(font, text.c_str(), text.size(), &w, &hgt)) return {};
    const float totalScale = size / static_cast<float>(baseSize) * scale;
    return { static_cast<float>(w) * totalScale, static_cast<float>(hgt) * totalScale };
}

// ── 相机 ──

void Renderer::setCamera(const Vec2f center, const Vec2f size, const float zoom) {
    if (!g.renderer) return;
    g.cameraActive = true;
    g.camCenter = center;
    g.camSize = size;
    g.camZoom = (zoom != 0.f) ? zoom : 1.f;
}

Renderer::CameraState Renderer::getCamera() const {
    if (!g.cameraActive) {
        // 默认视图：窗口大小、左上原点的中心（等价 SFML getDefaultView 语义）
        const float w = windowWidth(), h = windowHeight();
        return { {w * 0.5f, h * 0.5f}, {w, h}, 1.f };
    }
    return { g.camCenter, g.camSize, g.camZoom };
}

void Renderer::resetCamera() {
    g.cameraActive = false;
}

Vec2f Renderer::screenToWorld(const Vec2i screenPos) const {
    if (!g.cameraActive) {
        return Vec2f(static_cast<float>(screenPos.x), static_cast<float>(screenPos.y));
    }
    const Vec2f s = camEffectiveSize();
    const Vec2f leftTop(g.camCenter.x - s.x * 0.5f, g.camCenter.y - s.y * 0.5f);
    return { leftTop.x + static_cast<float>(screenPos.x) / windowWidth() * s.x,
             leftTop.y + static_cast<float>(screenPos.y) / windowHeight() * s.y };
}

} // namespace eng

// ── SDL3 实现层内部共享（供 AssetManager.cpp 懒建纹理）──
namespace eng::detail {

SDL_Renderer* sdl3Renderer() {
    return g.renderer;
}

} // namespace eng::detail

// ── 输入轮询（SDL3 实现，替代 EventConvertSFML.cpp 中的同名接口）──
namespace eng::Input {

bool isKeyPressed(const Key key) {
    const SDL_Scancode sc = toScancode(key);
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    int numkeys = 0;
    const bool* states = SDL_GetKeyboardState(&numkeys);
    return sc < numkeys && states[sc];
}

Vec2i getMousePosition() {
    float x = 0.f, y = 0.f;
    SDL_GetMouseState(&x, &y);
    return Vec2i(static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)));
}

} // namespace eng::Input

#endif // SERVER_BUILD
