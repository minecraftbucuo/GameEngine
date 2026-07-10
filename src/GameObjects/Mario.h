//
// Created by MINEC on 2026/1/29.
//

#pragma once
#include <SFML/Graphics.hpp>
#include "NetworkGameObject.h"
#include "Events.h"
#include <SFML/Audio.hpp>

class Mario : public NetworkGameObject {
public:
    Mario(float x, float y, bool isPlayer = true);

    ~Mario() override;

    void start() override;

    void handleEvent(sf::Event& e) override;

    void update(sf::Time deltaTime) override;

    bool needGravity();

    void handleCollision(const CollisionEvent& event);

    bool getIsPlayer() const;

    void destroy() override;

    sf::Vector2f getCenter() override;

    void serialize(sf::Packet& packet, NetworkMsg type) override;

    void deserialize(sf::Packet& packet) override;

private:
    // 客户端远端玩家的显示平滑：把当前位置逐帧推向最近一次收到的服务端目标点。
    void applyRemoteNetworkSmoothing(sf::Time deltaTime);

    // 客户端本地玩家的服务端校正：小误差忽略，中等误差软修正，大误差直接同步。
    void reconcileLocalPlayer(const sf::Vector2f& serverPosition, const sf::Vector2f& serverSpeed, bool isJump);

    // 直接应用服务端权威状态：用于严重不同步时拉回，或未来需要无平滑同步的对象。
    void setAuthoritativeState(const sf::Vector2f& serverPosition, const sf::Vector2f& serverSpeed, bool isJump);

    bool isPlayer = true;
    bool hasNetworkTarget = false;
    sf::Vector2f networkTargetPosition{};
    sf::Vector2f networkTargetSpeed{};
};
