//
// Created by MINEC on 2026/8/23.
//

// 【临时脚手架文件 — SDL3 迁移 Step 11 删除，届时由 RendererSDL3.cpp 接管实现】
#include "Core/EventConvertSFML.h"

#ifndef SERVER_BUILD   // 客户端专用：引用 sfml-window 符号，服务端构建不链接
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

namespace eng {

namespace {
    // Input::getMousePosition 轮询用窗口指针（setInputWindow 注册）
    sf::RenderWindow* s_inputWindow = nullptr;
}

Key toEngineKey(const int sfKey) {
    switch (sfKey) {
        case sf::Keyboard::A: return Key::A;
        case sf::Keyboard::B: return Key::B;
        case sf::Keyboard::C: return Key::C;
        case sf::Keyboard::D: return Key::D;
        case sf::Keyboard::E: return Key::E;
        case sf::Keyboard::F: return Key::F;
        case sf::Keyboard::G: return Key::G;
        case sf::Keyboard::H: return Key::H;
        case sf::Keyboard::I: return Key::I;
        case sf::Keyboard::J: return Key::J;
        case sf::Keyboard::K: return Key::K;
        case sf::Keyboard::L: return Key::L;
        case sf::Keyboard::M: return Key::M;
        case sf::Keyboard::N: return Key::N;
        case sf::Keyboard::O: return Key::O;
        case sf::Keyboard::P: return Key::P;
        case sf::Keyboard::Q: return Key::Q;
        case sf::Keyboard::R: return Key::R;
        case sf::Keyboard::S: return Key::S;
        case sf::Keyboard::T: return Key::T;
        case sf::Keyboard::U: return Key::U;
        case sf::Keyboard::V: return Key::V;
        case sf::Keyboard::W: return Key::W;
        case sf::Keyboard::X: return Key::X;
        case sf::Keyboard::Y: return Key::Y;
        case sf::Keyboard::Z: return Key::Z;
        case sf::Keyboard::Num0: return Key::Num0;
        case sf::Keyboard::Num1: return Key::Num1;
        case sf::Keyboard::Num2: return Key::Num2;
        case sf::Keyboard::Num3: return Key::Num3;
        case sf::Keyboard::Num4: return Key::Num4;
        case sf::Keyboard::Num5: return Key::Num5;
        case sf::Keyboard::Num6: return Key::Num6;
        case sf::Keyboard::Num7: return Key::Num7;
        case sf::Keyboard::Num8: return Key::Num8;
        case sf::Keyboard::Num9: return Key::Num9;
        case sf::Keyboard::F1: return Key::F1;
        case sf::Keyboard::F2: return Key::F2;
        case sf::Keyboard::F3: return Key::F3;
        case sf::Keyboard::F4: return Key::F4;
        case sf::Keyboard::F5: return Key::F5;
        case sf::Keyboard::F6: return Key::F6;
        case sf::Keyboard::F7: return Key::F7;
        case sf::Keyboard::F8: return Key::F8;
        case sf::Keyboard::F9: return Key::F9;
        case sf::Keyboard::F10: return Key::F10;
        case sf::Keyboard::F11: return Key::F11;
        case sf::Keyboard::F12: return Key::F12;
        case sf::Keyboard::Escape: return Key::Escape;
        case sf::Keyboard::Enter: return Key::Enter;
        case sf::Keyboard::Space: return Key::Space;
        case sf::Keyboard::Tab: return Key::Tab;
        case sf::Keyboard::Backspace: return Key::Backspace;
        case sf::Keyboard::Insert: return Key::Insert;
        case sf::Keyboard::Delete: return Key::Delete;
        case sf::Keyboard::Home: return Key::Home;
        case sf::Keyboard::End: return Key::End;
        case sf::Keyboard::PageUp: return Key::PageUp;
        case sf::Keyboard::PageDown: return Key::PageDown;
        case sf::Keyboard::LShift: return Key::LShift;
        case sf::Keyboard::RShift: return Key::RShift;
        case sf::Keyboard::LControl: return Key::LCtrl;
        case sf::Keyboard::RControl: return Key::RCtrl;
        case sf::Keyboard::LAlt: return Key::LAlt;
        case sf::Keyboard::RAlt: return Key::RAlt;
        case sf::Keyboard::Up: return Key::Up;
        case sf::Keyboard::Down: return Key::Down;
        case sf::Keyboard::Left: return Key::Left;
        case sf::Keyboard::Right: return Key::Right;
        case sf::Keyboard::Comma: return Key::Comma;
        case sf::Keyboard::Period: return Key::Period;
        case sf::Keyboard::Slash: return Key::Slash;
        case sf::Keyboard::Semicolon: return Key::Semicolon;
        case sf::Keyboard::Apostrophe: return Key::Apostrophe;
        case sf::Keyboard::LBracket: return Key::LBracket;
        case sf::Keyboard::RBracket: return Key::RBracket;
        case sf::Keyboard::Hyphen: return Key::Minus;
        case sf::Keyboard::Equal: return Key::Equal;
        case sf::Keyboard::Grave: return Key::Backquote;
        default: return Key::Unknown;
    }
}

int toSfKey(const Key key) {
    switch (key) {
        case Key::A: return sf::Keyboard::A;
        case Key::B: return sf::Keyboard::B;
        case Key::C: return sf::Keyboard::C;
        case Key::D: return sf::Keyboard::D;
        case Key::E: return sf::Keyboard::E;
        case Key::F: return sf::Keyboard::F;
        case Key::G: return sf::Keyboard::G;
        case Key::H: return sf::Keyboard::H;
        case Key::I: return sf::Keyboard::I;
        case Key::J: return sf::Keyboard::J;
        case Key::K: return sf::Keyboard::K;
        case Key::L: return sf::Keyboard::L;
        case Key::M: return sf::Keyboard::M;
        case Key::N: return sf::Keyboard::N;
        case Key::O: return sf::Keyboard::O;
        case Key::P: return sf::Keyboard::P;
        case Key::Q: return sf::Keyboard::Q;
        case Key::R: return sf::Keyboard::R;
        case Key::S: return sf::Keyboard::S;
        case Key::T: return sf::Keyboard::T;
        case Key::U: return sf::Keyboard::U;
        case Key::V: return sf::Keyboard::V;
        case Key::W: return sf::Keyboard::W;
        case Key::X: return sf::Keyboard::X;
        case Key::Y: return sf::Keyboard::Y;
        case Key::Z: return sf::Keyboard::Z;
        case Key::Num0: return sf::Keyboard::Num0;
        case Key::Num1: return sf::Keyboard::Num1;
        case Key::Num2: return sf::Keyboard::Num2;
        case Key::Num3: return sf::Keyboard::Num3;
        case Key::Num4: return sf::Keyboard::Num4;
        case Key::Num5: return sf::Keyboard::Num5;
        case Key::Num6: return sf::Keyboard::Num6;
        case Key::Num7: return sf::Keyboard::Num7;
        case Key::Num8: return sf::Keyboard::Num8;
        case Key::Num9: return sf::Keyboard::Num9;
        case Key::F1: return sf::Keyboard::F1;
        case Key::F2: return sf::Keyboard::F2;
        case Key::F3: return sf::Keyboard::F3;
        case Key::F4: return sf::Keyboard::F4;
        case Key::F5: return sf::Keyboard::F5;
        case Key::F6: return sf::Keyboard::F6;
        case Key::F7: return sf::Keyboard::F7;
        case Key::F8: return sf::Keyboard::F8;
        case Key::F9: return sf::Keyboard::F9;
        case Key::F10: return sf::Keyboard::F10;
        case Key::F11: return sf::Keyboard::F11;
        case Key::F12: return sf::Keyboard::F12;
        case Key::Escape: return sf::Keyboard::Escape;
        case Key::Enter: return sf::Keyboard::Enter;
        case Key::Space: return sf::Keyboard::Space;
        case Key::Tab: return sf::Keyboard::Tab;
        case Key::Backspace: return sf::Keyboard::Backspace;
        case Key::Insert: return sf::Keyboard::Insert;
        case Key::Delete: return sf::Keyboard::Delete;
        case Key::Home: return sf::Keyboard::Home;
        case Key::End: return sf::Keyboard::End;
        case Key::PageUp: return sf::Keyboard::PageUp;
        case Key::PageDown: return sf::Keyboard::PageDown;
        case Key::LShift: return sf::Keyboard::LShift;
        case Key::RShift: return sf::Keyboard::RShift;
        case Key::LCtrl: return sf::Keyboard::LControl;
        case Key::RCtrl: return sf::Keyboard::RControl;
        case Key::LAlt: return sf::Keyboard::LAlt;
        case Key::RAlt: return sf::Keyboard::RAlt;
        case Key::Up: return sf::Keyboard::Up;
        case Key::Down: return sf::Keyboard::Down;
        case Key::Left: return sf::Keyboard::Left;
        case Key::Right: return sf::Keyboard::Right;
        case Key::Comma: return sf::Keyboard::Comma;
        case Key::Period: return sf::Keyboard::Period;
        case Key::Slash: return sf::Keyboard::Slash;
        case Key::Semicolon: return sf::Keyboard::Semicolon;
        case Key::Apostrophe: return sf::Keyboard::Apostrophe;
        case Key::LBracket: return sf::Keyboard::LBracket;
        case Key::RBracket: return sf::Keyboard::RBracket;
        case Key::Minus: return sf::Keyboard::Hyphen;
        case Key::Equal: return sf::Keyboard::Equal;
        case Key::Backquote: return sf::Keyboard::Grave;
        default: return sf::Keyboard::Unknown;
    }
}

MouseButton toEngineMouseButton(const int sfButton) {
    switch (sfButton) {
        case sf::Mouse::Left: return MouseButton::Left;
        case sf::Mouse::Right: return MouseButton::Right;
        case sf::Mouse::Middle: return MouseButton::Middle;
        default: return MouseButton::Left;
    }
}

std::optional<EngineEvent> toEngineEvent(const sf::Event& event) {
    EngineEvent out{};
    switch (event.type) {
        case sf::Event::Closed:
            out.type = EventType::WindowClose;
            break;
        case sf::Event::Resized:
            out.type = EventType::WindowResize;
            out.newSize = Vec2u(event.size.width, event.size.height);
            break;
        case sf::Event::LostFocus:
            out.type = EventType::LostFocus;
            break;
        case sf::Event::GainedFocus:
            out.type = EventType::GainFocus;
            break;
        case sf::Event::TextEntered:
            out.type = EventType::TextEntered;
            out.codepoint = event.text.unicode;
            break;
        case sf::Event::KeyPressed:
            out.type = EventType::KeyPress;
            out.key = toEngineKey(event.key.code);
            break;
        case sf::Event::KeyReleased:
            out.type = EventType::KeyRelease;
            out.key = toEngineKey(event.key.code);
            break;
        case sf::Event::MouseWheelScrolled:
            out.type = EventType::MouseWheel;
            out.wheelDelta = event.mouseWheelScroll.delta;
            out.mousePos = Vec2i(event.mouseWheelScroll.x, event.mouseWheelScroll.y);
            break;
        case sf::Event::MouseButtonPressed:
            out.type = EventType::MouseButtonPress;
            out.mouseButton = toEngineMouseButton(event.mouseButton.button);
            out.mousePos = Vec2i(event.mouseButton.x, event.mouseButton.y);
            break;
        case sf::Event::MouseButtonReleased:
            out.type = EventType::MouseButtonRelease;
            out.mouseButton = toEngineMouseButton(event.mouseButton.button);
            out.mousePos = Vec2i(event.mouseButton.x, event.mouseButton.y);
            break;
        case sf::Event::MouseMoved:
            out.type = EventType::MouseMove;
            out.mousePos = Vec2i(event.mouseMove.x, event.mouseMove.y);
            break;
        default:
            // SFML 特有事件（摇杆/触屏/传感器等）引擎不关心
            return std::nullopt;
    }
    return out;
}

} // namespace eng

namespace eng::detail {

void setInputWindow(sf::RenderWindow* window) {
    s_inputWindow = window;
}

} // namespace eng::detail

namespace eng::Input {

bool isKeyPressed(const Key key) {
    return sf::Keyboard::isKeyPressed(static_cast<sf::Keyboard::Key>(toSfKey(key)));
}

Vec2i getMousePosition() {
    if (s_inputWindow) {
        return sf::Mouse::getPosition(*s_inputWindow);
    }
    // 未注册窗口时退化为屏幕坐标（正常流程 GameEngine 启动即注册，不应走到这里）
    return sf::Mouse::getPosition();
}

} // namespace eng::Input

#endif // SERVER_BUILD
