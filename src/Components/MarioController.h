//
// Created by MINEC on 2026/2/2.
//

#pragma once

#include "Component.h"
#include "Timer.h"
#include "Core/Types.h"
#ifndef SERVER_BUILD
#ifdef ENGINE_SDL3
struct MIX_Track;   // SDL_mixer track 前置声明（SDL3 迁移 Step 10；头文件不引 SDL 头）
#else
#include <SFML/Audio.hpp>
#endif
#endif

class MarioController : public Component {
public:
    ~MarioController() override;

    void start() override;

    void handleEvent(const eng::EngineEvent& event) override;

    void update(const eng::Time& deltaTime) override;

    void jump(bool play_sound = true);

    void runLeft() const;

    void runRight() const;

    void stopRun() const;

    void setIsPlayer(bool flag);

    void setWisPressed(bool flag);

    void shoot(bool play_sound = true);

private:
#ifndef SERVER_BUILD
#ifdef ENGINE_SDL3
    // SDL3 迁移 Step 10：一次性音效 = 常驻 track（绑预解码 MIX_Audio，play 时 restart）
    MIX_Track* jump_track = nullptr;
    MIX_Track* shoot_track = nullptr;
#else
    sf::Sound jump_sound;
    sf::Sound shoot_sound;
#endif
#endif
    bool w_is_pressed = false;
    Timer jump_timer;
    bool could_shoot = true;
    Timer shoot_timer;
    bool is_player = true;
};

