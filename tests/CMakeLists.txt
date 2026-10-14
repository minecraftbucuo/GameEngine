# ── 单元测试目标 ────────────────────────────────────────────────────
# 刻意只链「纯逻辑」模块（eng::Packet / ConfigManager），不链 SDL / Box2D：
# 单元测试要能在无图形、无音频、无物理的环境（CI、Linux 服务器）里跑。
# 被测的 .cpp 直接编进来，不依赖主目标 —— 主目标带 SERVER_BUILD 等宏，
# 测试要的是干净的默认配置。

add_executable(GameEngineTests
        test_main.cpp
        test_packet.cpp
        test_configmanager.cpp

        # 被测源码
        ${CMAKE_SOURCE_DIR}/src/Network/Packet.cpp
        ${CMAKE_SOURCE_DIR}/src/Manager/ConfigManager.cpp
)

target_compile_features(GameEngineTests PRIVATE cxx_std_20)
# 警告开关与主目标同源（GAME_WARNING_FLAGS 由根 CMakeLists 定义，向下可见）
target_compile_options(GameEngineTests PRIVATE ${GAME_WARNING_FLAGS})

target_include_directories(GameEngineTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/Network
        ${CMAKE_SOURCE_DIR}/src/Manager
        ${CMAKE_SOURCE_DIR}/lib          # nlohmann/json 与 doctest
)

# 产物单独放，避免与游戏 exe 混在同一目录（build/bin、build/server）
set_target_properties(GameEngineTests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# 注册到 CTest：ctest --test-dir <build> 即可运行
add_test(NAME GameEngineTests COMMAND GameEngineTests)
