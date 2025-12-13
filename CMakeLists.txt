cmake_minimum_required(VERSION 3.30)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# 设置 SFML 路径（根据你的实际路径修改）
set(SFML_ROOT "E:/Document/SFML/SFML-2.6.1-gcc")

# 包含头文件
include_directories(${SFML_ROOT}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Components)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Components/Collisions)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Components/CollisionHandles)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/GameObjects)

# 定义源文件列表
set(SOURCES
        main.cpp
        CollisionSystem.h
        Components/Component.h
        EventBus.h
        Events.h
        GameEngine.h
        GameObjects/GameObject.h
        Components/GravityComponent.h
        GameObjects/Ground.h
        GameObjects/Player.h
        World.h
        WorldContext.h
        Components/Collisions/Collision.h
        Components/Collisions/CircleCollision.cpp
        Components/Collisions/CircleCollision.h
        Components/Collisions/BoxCollision.cpp
        Components/Collisions/BoxCollision.h
        Components/MoveComponent.h
        Components/Controller.h
        Components/CollisionHandles/CollisionHandle.h
        Components/CollisionHandles/BoxCollisionHandle.h
        Components/CollisionHandles/CircleCollisionHandle.h
        GameObjects/BoxGameObject.h)

if(WIN32)
    # Windows 下需区分 Debug/Release 后缀（如 sfml-graphics-d.lib）
    link_directories(${SFML_ROOT}/lib)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(
            ${PROJECT_NAME}
            debug sfml-graphics-d
            debug sfml-window-d
            debug sfml-system-d
            optimized sfml-graphics
            optimized sfml-window
            optimized sfml-system
    )
else()
    # Linux/macOS 通常无后缀
    link_directories(${SFML_ROOT}/lib)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} sfml-graphics sfml-window sfml-system)
endif()
