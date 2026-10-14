//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include <typeinfo>
#include "Logger.h"

class EventBus {
    class HolderBase {
    public:
        virtual ~HolderBase() = default;

        // 实际存储的回调元素类型（已 decay）。subscribe / publish 靠它做类型校验，
        // 避免对不匹配的 EventHolder<T> 做 static_cast（那是 UB）。
        [[nodiscard]] virtual const std::type_info& elementType() const = 0;
    };

    template<typename T>
    class EventHolder : public HolderBase {
    public:
        ~EventHolder() override = default;
        [[nodiscard]] const std::type_info& elementType() const override { return typeid(T); }
        std::vector<std::function<void(const T&)>> events;
    };
public:
    // 当前生效实例：默认是进程级单例，可用 setInstance() 换成独立实例。
    // 既有 27 个调用点（散在 14 个文件）全部形如 EventBus::getInstance().xxx()，
    // 故本步零调用点改动。命名与 ConfigManager 一致：两个类的 getInstance()
    // 都是「当前生效者」，不设"默认单例"的独立入口以免被误用绕过替换。
    static EventBus& getInstance() {
        static EventBus fallback;
        return active_instance ? *active_instance : fallback;
    }

    // 替换当前生效实例；传 nullptr 恢复默认。
    // 供测试在 setup / teardown 成对调用——订阅表是全局状态，不换实例
    // 就会跨用例泄漏（上个用例订阅的回调会打进下个用例）。
    // 非线程安全：仅限启动期/测试使用。
    static void setInstance(EventBus* bus) {
        active_instance = bus;
    }

    // 允许测试/工具构造独立实例（生产代码请一律用 getInstance()）
    EventBus() = default;

    template <typename T>
    void subscribe(const std::string& event, std::function<void(const T&)>&& callback) {
        // 统一 decay：subscribe<const bool> 与 publish(.., true) 归一为同一 EventHolder<bool>，
        // 否则两者会落到不同的 Holder 类型上（const bool vs bool），publish 侧的
        // static_cast 便成为 UB。归一后语义更符合直觉，也消除了这处不匹配。
        using EventT = std::decay_t<T>;
        auto& holder_ptr = event_holder[event];
        if (!holder_ptr) {
            holder_ptr = std::make_unique<EventHolder<EventT>>();
        } else if (holder_ptr->elementType() != typeid(EventT)) {
            // 同名事件被以不同类型订阅：拒绝并报错，绝不 static_cast 到错误的 Holder。
            LOG_ERROR_FMT("subscribe type mismatch for event '{}': existing={}, requested={}",
                          event, holder_ptr->elementType().name(), typeid(EventT).name());
            return;
        }
        auto* holder = static_cast<EventHolder<EventT>*>(holder_ptr.get());
        holder->events.push_back(std::move(callback));
        LOG_TRACE_FMT("{} subscribed", event);
    }

    template <typename T>
    void publish(const std::string& eventName, const T& data) {
        using EventT = std::decay_t<T>;
        const auto it = event_holder.find(eventName);
        if (it == event_holder.end() || !it->second) {
            // LOG_WARN_FMT("{} not found", eventName);
            return;
        }
        if (it->second->elementType() != typeid(EventT)) {
            // 发布类型与订阅类型不符：丢弃并报错，不再做未校验的 static_cast（原 UB）。
            LOG_ERROR_FMT("publish type mismatch for event '{}': stored={}, published={}",
                          eventName, it->second->elementType().name(), typeid(EventT).name());
            return;
        }
        auto* holder = static_cast<EventHolder<EventT>*>(it->second.get());
        for (const auto& event : holder->events) {
            event(data);
        }
    }

    void removeSubscribe(const std::string& event_name) {
        if (!event_holder.contains(event_name)) {
            LOG_WARN_FMT("{} not found", event_name);
            return;
        }
        LOG_TRACE_FMT("{} removed", event_name);
        event_holder.erase(event_name);
    }

private:
    // 被 setInstance() 替换的实例；nullptr = 使用默认单例。
    // 裸指针无锁读：publish/subscribe 在游戏热路径高频调用，不做原子操作。
    inline static EventBus* active_instance = nullptr;

    std::unordered_map<std::string, std::unique_ptr<HolderBase>> event_holder;
};

