//
// Created by MINEC on 2025/12/10.
//

#pragma once
#include "Camera.h"
#include <SFML/Graphics.hpp>
#include <memory>

class GameObject;
class SceneManager;

class SceneContext {
public:
    static SceneContext& getInstance();
#ifndef SERVER_BUILD
    void setCamera(Camera* _camera);

    void setWindow(sf::RenderWindow* _window);
#endif
    void setGameObjects(const std::vector<std::shared_ptr<GameObject>>* _game_objects);

    void setSceneManager(SceneManager* _scene_manager);
#ifndef SERVER_BUILD
    [[nodiscard]] Camera* getCamera() const;

    [[nodiscard]] sf::RenderWindow* getWindow() const;
#endif

    [[nodiscard]] unsigned int getWindowWidth() const;

    [[nodiscard]] unsigned int getWindowHeight() const;

    [[nodiscard]] const std::vector<std::shared_ptr<GameObject>>* getGameObjects() const;

    [[nodiscard]] SceneManager* getSceneManager() const;
#ifndef SERVER_BUILD
    [[nodiscard]] sf::Vector2i getMousePosition() const;
#endif

private:
#ifndef SERVER_BUILD
    sf::RenderWindow* window{};
    Camera* camera{};
#endif
    const std::vector<std::shared_ptr<GameObject>>* game_objects{};
    SceneManager* scene_manager{};
};

