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
    static EventBus& getInstance() {
        static EventBus instance;
        return instance;
    }

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
            LOG_WARN_FMT("{} not found", eventName);
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
    std::unordered_map<std::string, std::unique_ptr<HolderBase>> event_holder;
    EventBus() = default;
};

