cmake_minimum_required(VERSION 3.30)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

# 设置 SFML 路径
set(SFML_ROOT "./lib/SFML-2.6.1-gcc")

# 包含头文件
include_directories(${SFML_ROOT}/include)
include_directories(./lib)
include_directories(src)
include_directories(src/Components)
include_directories(src/Components/Collisions)
include_directories(src/Components/CollisionHandles)
include_directories(src/GameObjects)
include_directories(src/Scene)
include_directories(src/State)
include_directories(src/Manager)

# 定义源文件列表
file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

if(WIN32)
    # Windows 下需区分 Debug/Release 后缀（如 sfml-graphics-d.lib）
    link_directories(${SFML_ROOT}/lib)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(
            ${PROJECT_NAME}
            debug sfml-graphics-d
            debug sfml-window-d
            debug sfml-system-d
            debug sfml-network-d
            optimized sfml-graphics
            optimized sfml-window
            optimized sfml-system
            optimized sfml-network
    )
else()
    # Linux/macOS 通常无后缀
    link_directories(${SFML_ROOT}/lib)
    add_executable(${PROJECT_NAME} ${SOURCES})
    target_link_libraries(${PROJECT_NAME} sfml-graphics sfml-window sfml-system sfml-network)
endif()
