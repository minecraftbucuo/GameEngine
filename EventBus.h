//
// Created by MINEC on 2025/12/9.
//


#pragma once
#include <unordered_map>
#include <string>
#include <functional>
#include <vector>
#include <any>
#include <SFML/Graphics.hpp>

class EventBus {
public:
    static EventBus& getInstance() {
        static EventBus instance;
        return instance;
    }

    template <typename T>
    void subscribe(const std::string& event, std::function<void(T)> callback) {
        listeners[event].push_back([callback](std::any data) {
            callback(std::any_cast<T>(data));
        });
    }

    template <typename T>
    void publish(const std::string& eventName, T data) {
        if (listeners.find(eventName) != listeners.end()) {
            for (auto& listener : listeners[eventName]) {
                listener(data);
            }
        }
    }

private:
    std::unordered_map<std::string, std::vector<std::function<void(std::any)>>> listeners;
    EventBus() = default;
};

