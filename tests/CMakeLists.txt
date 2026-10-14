# ── 单元测试目标 ────────────────────────────────────────────────────
# 刻意不链 SDL / Box2D：单元测试要能在无图形、无音频、无物理的环境
# （CI、Linux 服务器）里跑，保持「秒级且完全离线」。
# 被测的 .cpp 直接编进来，不依赖主目标。
# 当前覆盖：eng::Packet、ConfigManager、碰撞几何（BoxCollision / CircleCollision）。

add_executable(GameEngineTests
        test_main.cpp
        test_packet.cpp
        test_configmanager.cpp
        test_collision_geometry.cpp

        # 被测源码（纯逻辑）
        ${CMAKE_SOURCE_DIR}/src/Network/Packet.cpp
        ${CMAKE_SOURCE_DIR}/src/Manager/ConfigManager.cpp

        # 碰撞几何
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/Collision.cpp
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/BoxCollision.cpp
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/CircleCollision.cpp
)

# 碰撞源文件以 SERVER_BUILD 编译：该宏下 BoxCollision/CircleCollision 的 render()
# 被 #ifndef 排除，于是不必链接 Renderer 的实现（RendererSDL3.cpp 依赖 SDL），
# 纯几何逻辑得以留在「秒级、完全离线」的测试目标里。这与真实的服务端构建
# 用的是同一套宏组合，不是为测试硬造的开关。
# 用 TARGET_DIRECTORY 把宏限定在本目标，避免波及主目标（客户端构建需要 render()）。
set_property(SOURCE
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/Collision.cpp
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/BoxCollision.cpp
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions/CircleCollision.cpp
        TARGET_DIRECTORY GameEngineTests
        PROPERTY COMPILE_DEFINITIONS SERVER_BUILD)

target_compile_features(GameEngineTests PRIVATE cxx_std_20)
# 警告开关与主目标同源（GAME_WARNING_FLAGS 由根 CMakeLists 定义，向下可见）
target_compile_options(GameEngineTests PRIVATE ${GAME_WARNING_FLAGS})

target_include_directories(GameEngineTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Network
        ${CMAKE_SOURCE_DIR}/src/Manager
        ${CMAKE_SOURCE_DIR}/src/Components             # Component.h（Collision 的基类）
        ${CMAKE_SOURCE_DIR}/src/Components/Collisions  # Collision.h
        ${CMAKE_SOURCE_DIR}/src/GameObjects            # GameObject.h（BoxCollision::start 用）
        ${CMAKE_SOURCE_DIR}/lib          # nlohmann/json 与 doctest
)

# 产物单独放，避免与游戏 exe 混在同一目录（build/bin、build/server）
set_target_properties(GameEngineTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# 注册到 CTest：ctest --test-dir <build> 即可运行
add_test(NAME GameEngineTests COMMAND GameEngineTests)
