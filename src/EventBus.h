//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <memory>
#include "Logger.h"

class EventBus {
    class HolderBase {
    public:
        virtual ~HolderBase() = default;
    };

    template<typename T>
    class EventHolder : public HolderBase {
    public:
        ~EventHolder() override = default;
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
        auto& holder_ptr = event_holder[event];
        if (!holder_ptr) holder_ptr = std::make_unique<EventHolder<T>>();
        auto* holder = static_cast<EventHolder<T>*>(holder_ptr.get());
        holder->events.push_back(std::move(callback));
        LOG_TRACE_FMT("{} subscribed", event);
    }

    template <typename T>
    void publish(const std::string& eventName, const T& data) {
        if (const auto it = event_holder.find(eventName); it != event_holder.end() && it->second) {
            for (auto* holder = static_cast<EventHolder<std::decay_t<T>>*>(it->second.get());
                const auto& event : holder->events) {
                event(data);
            }
        } else {
            // LOG_WARN_FMT("{} not found", eventName);
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

