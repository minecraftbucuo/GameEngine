cmake_minimum_required(VERSION 3.26)

# ══════════ 构建开关 ═════════════════════════════════════════════
# 「每个构建目录该用什么开关」统一由 CMakePresets.json 决定：
#     cmake --preset client | server | web | tests
# 预设通过 -D 下发，而 -D 在**每次配置**都覆盖缓存 —— 预设值即权威值。
# 因此不会再出现「改了文件不生效」，也不会再出现 build-server 被静默打回
# 客户端构建（旧 resolve_switch 的 _LAST_ 启发式无法判断值是谁写的，
# 只能猜，猜错就悄悄回落到文件默认值）。
#
# 下面只声明默认值，作为「不带预设直接 cmake -S . -B xxx」时的兜底。
#   客户端(OFF)：窗口/渲染/输入/音频/网络全量
#   服务端(ON) ：纯网络+物理，无渲染无音频（SERVER_BUILD 宏生效）
option(BUILD_FOR_SERVER "构建无头服务端（无渲染/无音频，SERVER_BUILD 宏生效）" OFF)
# ON ：全部依赖静态链入 exe（单文件发布，编译慢体积大）
# OFF：动态链接（exe 小、编译快，运行需 SDL3.dll 等陪同）
option(BUILD_STATIC "全部依赖静态链入 exe（单文件发布）" ON)

# ── 单元测试开关（默认 OFF ⇒ 不带预设的既有构建行为完全不变）──────
# BUILD_TESTS      ：是否构建单元测试目标
# BUILD_TESTS_ONLY ：只配置测试，跳过全部游戏依赖与主目标。
#   单元测试只依赖纯逻辑模块（eng::Packet / ConfigManager），不需要 SDL /
#   Box2D。跳过它们可让配置从「数分钟（且需联网拉依赖）」降到「数秒且完全
#   离线」—— CI 的测试 job 就该这么配。
option(BUILD_TESTS "构建单元测试（Packet / ConfigManager 等纯逻辑模块）" OFF)
option(BUILD_TESTS_ONLY "只配置单元测试：跳过游戏依赖与主目标（CI / 离线）" OFF)

# 默认 Release：未显式指定构建类型时启用（多配置生成器如 VS 不适用，由 IDE 选择）
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

# 配置摘要：一眼确认关键设定是否如预期（沿用原 message() 风格）
# —— 之前 build-server 被静默打回客户端构建，就是因为没有这行显式确认。
if (CMAKE_CONFIGURATION_TYPES)
    message("Build type: <多配置生成器，由 IDE/--config 选择>")
else ()
    message("Build type: ${CMAKE_BUILD_TYPE}")
endif ()
if (BUILD_FOR_SERVER)
    message("BUILD_FOR_SERVER = ${BUILD_FOR_SERVER}  (server)")
else ()
    message("BUILD_FOR_SERVER = ${BUILD_FOR_SERVER}  (client)")
endif ()
if (BUILD_STATIC)
    message("BUILD_STATIC = ${BUILD_STATIC}  (static)")
else ()
    message("BUILD_STATIC = ${BUILD_STATIC}  (dynamic)")
endif ()
if (BUILD_TESTS_ONLY)
    message("BUILD_TESTS_ONLY = ${BUILD_TESTS_ONLY}  (只配测试，跳过游戏依赖与主目标)")
elseif (BUILD_TESTS)
    message("BUILD_TESTS = ${BUILD_TESTS}  (主目标 + 单元测试)")
endif ()

if (BUILD_FOR_SERVER)
    project(GameEngineServer LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server)
else ()
    project(GameEngine LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

# ── 编译器警告（编译期质量网）────────────────────────────────────
# 只作用于本项目自有目标：第三方依赖由 FetchContent 以 SYSTEM 引入，
# 其头文件告警已被 -isystem 抑制，这里再对自有代码显式开警告。
# 用 target_compile_options 而非 add_compile_options —— 后者会波及
# 第三方目标，把别人的告警混进本项目构建输出。
# GAME_WERROR=ON 可把警告升级为错误，供 CI 阻止新代码带回警告。
option(GAME_WERROR "把编译警告当作错误（CI 用）" OFF)

if (MSVC)
    set(GAME_WARNING_FLAGS /W4)
else ()
    # -Woverloaded-virtual 显式写死，不依赖 -Wall 带出来。
    # 本机 MinGW GCC 13.1 实测：该警告分两档，-Wall 只启用较弱的 =1，裸写等价于 =2；
    # BoxCollision 的名字隐藏在 =1 下漏报、=2 才报出，而 CI 的 GCC 直接报了 —— 于是
    # 出现「本地全绿、CI 报错」。显式写死让两边看到同一套警告。
    # 该警告指向真问题：派生类的同名重载会把基类虚函数完全遮蔽。
    set(GAME_WARNING_FLAGS -Wall -Wextra -Woverloaded-virtual)
endif ()

if (GAME_WERROR)
    if (MSVC)
        list(APPEND GAME_WARNING_FLAGS /WX)
    else ()
        list(APPEND GAME_WARNING_FLAGS -Werror)
    endif ()
endif ()

# ── WEB 构建（Emscripten / WASM）：仅 web 预设（EMSCRIPTEN 工具链）时生效 ──
# 桌面/服务端构建零影响（EMSCRIPTEN 由 Emscripten.cmake 工具链设置）
# 用法见 docs/wasm-web-port-plan.md 与 scripts/build_web.ps1
#   · 产物在 web/ 下（html/js/wasm/data 四件套），Asset 经 --preload-file 打进虚拟盘
#     /Asset，与 getExeDir() 的 "/" 约定配套（迁移计划决策 3）
#   · -fexceptions 必须开：json/filesystem/Packet 大量 try/catch，
#     Emscripten 默认禁异常捕获，抛出即 abort（决策 5）；声明先于 FetchContent 使依赖同吃此选项
#   · MAX_WEBGL_VERSION=2：SDL3 的 GLES2 渲染后端需要 WebGL2 画布上下文
#   · 不开 pthread：避免部署端强制 COOP/COEP 头，GitHub Pages 开箱即用（决策 6）
#   · SDL_net 本步暂保留参与编译（emscripten 头文件齐全能编过；运行期无裸 socket
#     自然不可用），源码级裁剪与移除统一放到 Step 5（决策 7 分阶段）
if (EMSCRIPTEN)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/web)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")          # 链接器直接产出可打开的 html 入口页
    add_compile_options(-fexceptions)
    add_link_options(
            -fexceptions
            "-sALLOW_MEMORY_GROWTH=1"             # 资源全量预解码进内存，按需扩容
            "-sFORCE_FILESYSTEM=1"                # preload 虚拟文件系统
            "-sMAX_WEBGL_VERSION=2"
            "-lwebsocket.js"                      # 联机 N2：TcpClient WEB 后端
                                                  # （emscripten/websocket.h 的 JS 运行时胶水）
            "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
            "SHELL:--preload-file ${CMAKE_CURRENT_SOURCE_DIR}/Asset@/Asset"
    )
endif ()

# 静态/动态统一驱动：控制所有依赖库的构建方式与 exe 链接方式
# （CACHE FORCE 保证在同一 build 目录里切换时子项目旧缓存不残留）
if (BUILD_STATIC)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # Box2D / SDL_net 走此标准开关
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    if (WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "-static")   # MinGW 全静态 exe
    endif ()
else ()
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
endif ()

# ══════════════════════════════════════════════════════════════════
# 第三方依赖 + 游戏主目标 + 资源拷贝
# BUILD_TESTS_ONLY=ON 时整体跳过（单元测试不依赖它们，见上方开关说明）
# ══════════════════════════════════════════════════════════════════
if (NOT BUILD_TESTS_ONLY)

# ── 依赖一：Box2D v2.4（物理，引擎核心依赖，两端共用）─────────────
include(FetchContent)
# 注意：v2.4 无 BOX2D_BUILD_EXAMPLES（2.3 时代开关名），testbed 默认 ON 且
# 无条件拉 extern/glfw 等；无图形环境（Linux 服务器）会因缺 RandR 头配置失败
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
# GIT_SHALLOW：浅克隆（depth=1，丢弃 git 历史）
# GIT_SUBMODULES：只拉用到的子模块（见下方各卫星库声明）。
# 两者缺一不可：不裁子模块时，SDL_image 会把 libjxl/libavif 等十几个
# 被禁用后端的依赖仓库也全量拖进 .git/modules（实测 _deps 超 2GB）。
# EXCLUDE_FROM_ALL：依赖目标不进默认 all，被主目标链接时才编（省 SDL3_test 等）
# SYSTEM：依赖头文件走 -isystem，第三方告警不扰编译输出
FetchContent_Declare(
    Box2D
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG v2.4.1
    GIT_SHALLOW 1
    EXCLUDE_FROM_ALL
    SYSTEM
)
FetchContent_MakeAvailable(Box2D)

# ── 依赖二：SDL3 全家（窗口/渲染/输入/音频/网络）─────────────────
# 版本对齐（已核各卫星库源码 SDL_REQUIRED_VERSION）：mixer 3.2.0 要求
# SDL≥3.4.0 → 核心 3.4.14 满足全部。SDL_net 3 无 release tag，pin main commit。
# 以下 set 均为依赖内部配置（非用户开关）：解码器按 Asset 实际格式裁剪，
# 用不到的关掉以省编译时间与体积。
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)   # vendored freetype 兼容 CMake 4

# 无头服务器：SDL 只是 SDL_net 的底座（线程/时钟/原子属核心设施，不受子系统开关影响）。
# 必须关图形/音频子系统，否则 SDL 在 Linux 配置期检查 X11/Wayland/ALSA 等开发包，
# 无图形环境直接失败（如 Couldn't find dependency package for XCURSOR）。
# 开关名已核 sdl-src/CMakeLists.txt 的 define_sdl_subsystem 宏（L238-263）
if (BUILD_FOR_SERVER)
    # SDL 配置收尾检查（macros.cmake L405）：UNIX 上没找到 X11/Wayland 即 FATAL_ERROR，
    # 与子系统开关无关；此变量是官方逃生门 = "我确实不要桌面窗口"（服务器场景）
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
    set(SDL_VIDEO OFF CACHE BOOL "" FORCE)     # 连带 X11/Wayland/KMSDRM/GPU/Render/Camera
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)     # 连带 ALSA/PulseAudio/PipeWire/JACK
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)    # 连带 libusb 检查
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
endif ()

# SDL_image：资源全为 PNG，裁掉其余格式；PNG 走 vendored libpng+zlib
set(SDLIMAGE_JPG OFF)
set(SDLIMAGE_TIF OFF)
set(SDLIMAGE_WEBP OFF)
set(SDLIMAGE_JXL OFF)
set(SDLIMAGE_AVIF OFF)

# SDL_ttf：vendored freetype；关 harfbuzz/plutosvg（中文 UTF-8 渲染不需要）
set(SDLTTF_VENDORED ON)
set(SDLTTF_HARFBUZZ OFF)
set(SDLTTF_PLUTOSVG OFF)

# SDL_mixer：资源仅 ogg/wav（WAVE 内置、OGG 走 stb_vorbis 默认开），裁其余
set(SDLMIXER_MP3 OFF)
set(SDLMIXER_FLAC OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_AIFF OFF)
set(SDLMIXER_VOC OFF)
set(SDLMIXER_AU OFF)

FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    GIT_SHALLOW 1
    EXCLUDE_FROM_ALL
    SYSTEM
)
# SDL_net 3 移除了 1.x 默认开启的 TCP_NODELAY，且未提供任何公开 API/property
# 可设置（源码零引用、流接口不暴露句柄）——广域网上小于 MSS 的状态帧会被
# Nagle+延迟ACK 扣到上一包的 ACK 到达才发，客户端表现为按 RTT 成簇的卡顿。
# 补丁经幂等脚本应用：换工具链/清缓存会让 populate 重跑，直接 git apply 会在
# 已打过补丁的树上因上下文不符报 "patch does not apply"，脚本先查标记再决定。
find_package(Git REQUIRED)   # 仅作用于 _deps 第三方克隆（FetchContent 克隆本身也依赖 git）
FetchContent_Declare(
    SDL_net
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
    GIT_TAG 4dd9d8418e9781d10481fd6177e3a1064ca201fe   # main 2026-06-17
    # 不加 GIT_SHALLOW：tag 为 commit hash 时浅克隆不保证成功（CMake 文档警示），
    # 且本库仅 17MB 无利可图
    PATCH_COMMAND "${CMAKE_COMMAND}"
                  -DSRC_DIR=${CMAKE_BINARY_DIR}/_deps/sdl_net-src
                  -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl_net_tcp_nodelay.patch
                  -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                  -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl_net_apply_patch.cmake
    EXCLUDE_FROM_ALL
    SYSTEM
)
if (NOT BUILD_FOR_SERVER)
    # 客户端专属卫星库
    FetchContent_Declare(
        SDL_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW 1
        GIT_SUBMODULES external/libpng external/zlib   # 仅 PNG 后端所需
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_Declare(
        SDL_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW 1
        GIT_SUBMODULES external/freetype               # harfbuzz/plutosvg 已禁用不拉
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    FetchContent_Declare(
        SDL_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-3.2.0
        GIT_SHALLOW 1
        GIT_SUBMODULES external/ogg external/vorbis    # 其余格式已禁用不拉
        EXCLUDE_FROM_ALL
        SYSTEM
    )
endif ()
# SDL 在前：SDL_net 经 SDL3::SDL3 目标找到它
FetchContent_MakeAvailable(SDL SDL_net)
if (NOT BUILD_FOR_SERVER)
    FetchContent_MakeAvailable(SDL_image SDL_ttf SDL_mixer)
endif ()

# ── 引擎源码与目标 ────────────────────────────────────────────────
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Asset)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${SRC})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
target_compile_options(${PROJECT_NAME} PRIVATE ${GAME_WARNING_FLAGS})
if (BUILD_FOR_SERVER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SERVER_BUILD)
endif ()
target_include_directories(
        ${PROJECT_NAME}
        PRIVATE
        ${SRC_DIR}
        ${SRC_DIR}/Components
        ${SRC_DIR}/Components/Collisions
        ${SRC_DIR}/Components/CollisionHandles
        ${SRC_DIR}/GameObjects
        ${SRC_DIR}/Scene
        ${SRC_DIR}/State
        ${SRC_DIR}/Manager
        ${SRC_DIR}/Network
        ${SRC_DIR}/Physics

        ${LIB_DIR}
)

# 链接：客户端 = SDL 全家 + Box2D；服务端 = SDL_net + SDL 核心 + Box2D
# （SDL_net 静态构建不声明对 SDL3 的链接依赖——已核 sdl_net-src L164 仅共享
#   分支才链 SDL3::SDL3-shared，静态消费方必须自己显式链 SDL3）
if (NOT BUILD_FOR_SERVER)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            SDL3::SDL3
            SDL3_image::SDL3_image
            SDL3_ttf::SDL3_ttf
            SDL3_mixer::SDL3_mixer
            SDL3_net::SDL3_net
    )
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
endif ()
target_link_libraries(${PROJECT_NAME} PRIVATE box2d)

# 资源随构建拷到 exe 旁（getExeDir 约定：运行期 ./Asset/... 相对路径以 exe 目录为基准）
#   客户端：整目录拷贝（纹理/音频/字体/动画/地图/配置全量）
#   服务端：无渲染无音频（SERVER_BUILD 门控），运行期只读 config.json 与关卡地图 JSON，
#           只拷这两样；新增关卡 JSON 放 Asset/maps/ 即随构建自动同步
#   WEB    ：资源已由 --preload-file 打进 .data 包，跳过
if (NOT BUILD_FOR_SERVER AND NOT EMSCRIPTEN)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${ASSET_DIR}/
            $<TARGET_FILE_DIR:GameEngine>/Asset
    )
elseif (BUILD_FOR_SERVER)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset
            COMMAND ${CMAKE_COMMAND} -E copy
                    ${ASSET_DIR}/config.json
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset/
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${ASSET_DIR}/maps
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/Asset/maps
    )

    # 调试面板前端：随构建同步到 exe 旁的 web/debug（部署用）。
    # 开发时面板优先读源码树 ../../web/debug，改前端刷新即生效、不必重新构建。
    # 用独立 target 而非 PRE_BUILD：MinGW Makefiles 下 PRE_BUILD 等价 PRE_LINK，
    # 目标已最新时不会执行，前端改动不会被同步。
    add_custom_target(copy_panel_web ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_SOURCE_DIR}/web/debug
                    $<TARGET_FILE_DIR:${PROJECT_NAME}>/web/debug
            COMMENT "Syncing debug panel web assets"
    )
    add_dependencies(${PROJECT_NAME} copy_panel_web)
endif ()

endif ()   # NOT BUILD_TESTS_ONLY

# ── 单元测试（可选，默认 OFF，由 tests 预设置 ON）───────────────────
# 用法：
#   cmake --preset tests
#   cmake --build --preset tests
#   ctest --preset tests          # 或 ctest --test-dir build-tests --output-on-failure
#
# 不用预设的等价写法：
#   cmake -S . -B build-tests -DBUILD_TESTS=ON -DBUILD_TESTS_ONLY=ON
if (BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()
