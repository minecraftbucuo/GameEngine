//
// Created by MINEC on 2026/5/7.
//

#pragma once
#include "Animation.h"
#include "Events.h"
#include "NetworkGameObject.h"
#include "Timer.h"
#include "Core/Types.h"


class FireBall : public NetworkGameObject {
public:
    FireBall(unsigned int owner_id, float x, float y, float speed_x = 0.f);

    ~FireBall() override;

    void start() override;
#ifndef SERVER_BUILD
    void render(eng::Renderer& renderer) override;
#endif
    void update(eng::Time deltaTime) override;

    void setExploded();

    void handleCollision(const CollisionEvent& event);

    void serialize(eng::Packet& packet, NetworkMsg type) override;

    void deserialize(eng::Packet& packet) override;

    unsigned int getOwnerId() const;

private:
    bool is_exploded = false;
#ifndef SERVER_BUILD
    Animation animation;
    Animation explosionAnimation;
#endif
    unsigned int owner_id;
    Timer ttl_timer;
};
