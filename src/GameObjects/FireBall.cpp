//
// Created by MINEC on 2026/5/8.
//

#include "FireBall.h"

#include "FrameManager.h"
#include "GravityComponent.h"
#include "Collision.h"
#include "EventBus.h"
#include "BoxCollision.h"
#include "Logger.h"
#include "MoveComponent.h"
#include "NetworkManager.h"
#include "Scene.h"
#include "Core/Types.h"

// 服务端爆炸到销毁的延迟：给客户端留出播放爆炸动画的窗口
constexpr int EXPLODE_DESTROY_DELAY_MS = 300;

FireBall::FireBall(const unsigned int owner_id, const float x, const float y, const float speed_x) {
    this->owner_id = owner_id;
    this->position = eng::Vec2f(x, y);

#ifndef SERVER_BUILD
    animation.setFrames(FrameManager::getInstance().getFrame("fireball_frame"));
    explosionAnimation.setFrames(FrameManager::getInstance().getFrame("explosion_frame"));
    this->setSize(animation.getFrameWidth(), animation.getFrameHeight());
#else
    this->setSize(CONFIG.game.defaultBlockSize / 2, CONFIG.game.defaultBlockSize / 2);
#endif

    this->addComponent<Collision, BoxCollision>();
    this->addComponent<GravityComponent>();

    this->addComponent<MoveComponent>()->setSpeed(eng::Vec2f(speed_x, CONFIG.game.fireballSpeedY));

    // 设置为 10s 自动爆炸
    ttl_timer.start(CONFIG.game.fireBallTTL);
    ttl_timer.setCallback([this]() -> void { this->setExploded(); });

    this->tag = "fireball:" + std::to_string(this->id);
    className = "FireBall";
}

FireBall::~FireBall() {
    LOG_TRACE_FMT("The object tagged {} is destroyed", this->getTag());
    EventBus::getInstance().removeSubscribe("onCollision" + this->tag);
}

void FireBall::start() {
    GameObject::start();
    EventBus::getInstance().subscribe<CollisionEvent>(
        "onCollision" + this->tag,
        [this](const CollisionEvent& collisionEvent) {
            handleCollision(collisionEvent);
        }
    );
}

#ifndef SERVER_BUILD
void FireBall::render(eng::Renderer& renderer) {
    GameObject::render(renderer);
    if (is_exploded) explosionAnimation.render(renderer, this->position);
    else animation.render(renderer, this->position);
}
#endif

#ifndef SERVER_BUILD
void FireBall::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_exploded) {
        explosionAnimation.update(deltaTime);
        if (explosionAnimation.isOver()) {
            destroy();
        }
    }
    else {
        animation.update(deltaTime);
    }
    ttl_timer.update(deltaTime);
}
#else
void FireBall::update(eng::Time deltaTime) {
    GameObject::update(deltaTime);
    if (is_exploded) {
        // 爆炸后延迟销毁（由 explode_timer 回调触发 destroy 广播 RemoveObject），
        // 客户端在这段窗口内播放爆炸动画
        explode_timer.update(deltaTime);
        return;
    }
    ttl_timer.update(deltaTime);
}
#endif

void FireBall::setExploded() {
    is_exploded = true;
    this->getComponent<GravityComponent>()->setActive(false);
    this->getComponent<MoveComponent>()->setActive(false);
    const float offset = CONFIG.game.defaultBlockSize / 4;
    this->getComponent<MoveComponent>()->addPosition(eng::Vec2f(-offset, -offset), false);
#ifdef SERVER_BUILD
    // 服务端爆炸后延迟销毁：客户端收到 exploded 快照本地播爆炸动画，
    // 服务端立即 RemoveObject 会截断动画甚至抢在快照前直接删除
    explode_timer.setCallback([this] { destroy(); });
    explode_timer.start(EXPLODE_DESTROY_DELAY_MS);
#endif
}

void FireBall::handleCollision(const CollisionEvent& event) {
    auto& this_ = event.a;
    auto& other = event.b;

    if (owner_id == other->getId()) return;
    // 已爆炸：碰撞组件仍活跃到销毁为止，期间不再响应（防止 setExploded 重复偏移）
    if (is_exploded) return;

    // 击中敌人：炮弹直接爆炸（敌人死亡由敌人侧的 handleCollision 处理）
    if (other->getClassName() == "Goomba" || other->getClassName() == "Bowser") {
        setExploded();
        return;
    }

    // 乌龟免疫火系（Koopa 侧对炮弹穿行，双向一致）：炮弹穿过不爆炸。
    // 蘑菇是道具、BOSS 火焰弹/飞斧是敌方投射物：均与炮弹互不交互（穿行）——
    // 否则炮弹会对着完全不受影响的对象单方面爆炸/弹跳
    const std::string& other_name = other->getClassName();
    if (other_name == "Koopa" || other_name == "Mushroom" ||
        other_name == "BowserFire" || other_name == "BowserAxe") {
        return;
    }

    // std::cout << this_->getTag() << ' ' << other->getTag() << std::endl;

    if (!this_->getMoveAble()) return;
    const std::shared_ptr<MoveComponent>& moveComponent = this_->getComponent<MoveComponent>();
    if (!moveComponent) return;

    // 计算 x 方向和 y 方向的重合度
    const float dx = std::min(event.a_position.x + this_->getSize().x,
                              event.b_position.x + other->getSize().x) - std::max(
        event.a_position.x, event.b_position.x);
    const float dy = std::min(event.a_position.y + this_->getSize().y,
                              event.b_position.y + other->getSize().y) - std::max(
        event.a_position.y, event.b_position.y);

    // 水平碰撞
    if (dx <= dy) {
        float right_x = std::abs(
            event.a_position.x + this_->getSize().x - (event.b_position.x + other->getSize().x * 0.5f));
        float left_x = std::abs(event.a_position.x - (event.b_position.x + other->getSize().x * 0.5f));
        if (right_x < left_x) {
            moveComponent->moveCollisionXTo(event.b_position.x - this_->getSize().x);
        }
        else {
            moveComponent->moveCollisionXTo(event.b_position.x + other->getSize().x);
        }

        // 设置为爆炸
        setExploded();
    }
    else {
        // 处理垂直方向碰撞：当火球向上运动且重叠区域差异较小时忽略碰撞
        if (this_->getSpeed().y < 0 && dx - dy < 10.f) return;

        const float relativeSpeedY = event.b_speed.y - event.a_speed.y;
        moveComponent->setSpeedY(relativeSpeedY * 0.5f);
        if (relativeSpeedY < 0) {
            moveComponent->setSpeedY(std::min(relativeSpeedY * 0.5f, CONFIG.game.fireballSpeedY));
        }

        // 计算火球顶部和底部与碰撞物体的距离，判断碰撞面并调整位置
        float top_y = std::abs(event.a_position.y - (event.b_position.y + other->getSize().y * 0.5f));
        float bottom_y = std::abs(
            event.a_position.y + this_->getSize().y - (event.b_position.y + other->getSize().y * 0.5f));
        if (top_y > bottom_y) {
            moveComponent->moveCollisionYTo(event.b_position.y - this_->getSize().y);
        }
        else {
            moveComponent->moveCollisionYTo(event.b_position.y + other->getSize().y);
        }
    }
}

void FireBall::serialize(eng::Packet& packet, const NetworkMsg type) {
    // NetworkManager 检测到火球的产生后自动调用通知所有的客户端
    if (type == NetworkMsg::SpawnFireBall || type == NetworkMsg::SpawnObject) {
        // 交给 Scene 处理
        packet << NetworkMsg::SpawnFireBall;
        packet << this->id << ObjectType::FireBall << this->owner_id << this->position.x <<
            this->position.y << this->getSpeed().x << this->getSpeed().y;
    } else if (type == NetworkMsg::UpdateObject) {
        // 火球是纯抛射物，客户端无需预测，直接硬同步服务端权威位置/速度
        packet << type << this->id << type << this->position.x << this->position.y <<
            this->getSpeed().x << this->getSpeed().y << is_exploded;
    }
}

void FireBall::deserialize(eng::Packet& packet) {
    NetworkMsg msg_type;
    packet >> msg_type;
    if (msg_type != NetworkMsg::UpdateObject) return;
    float x, y, s_x, s_y;
    bool exploded;
    packet >> x >> y >> s_x >> s_y >> exploded;
    // 服务端判爆兜底：本地尚未预测到（碰撞判定分歧）时强制本地爆炸（播放爆炸动画，
    // 动画播完由 update 里的 destroy 静默移除，与服务端随后的 RemoveObject 幂等）
    if (exploded && !is_exploded) {
        setExploded();
        return;
    }
    if (is_exploded) return;
    if (const auto move = getComponent<MoveComponent>()) {
        move->setPosition(eng::Vec2f(x, y));
        move->setSpeed(eng::Vec2f(s_x, s_y));
    }
}

void FireBall::destroy() {
    auto* nm = getScene() ? getScene()->getNetworkManager() : nullptr;
    if (nm && nm->isServer()) nm->broadcastRemoveObject(this->getId());
    NetworkGameObject::destroy();
}

unsigned int FireBall::getOwnerId() const {
    return this->owner_id;
}
