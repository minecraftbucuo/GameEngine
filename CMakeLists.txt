cmake_minimum_required(VERSION 3.20)

# ══════════ 用户开关（仅此两个，命令行可用 -D 覆盖）══════════════
# 客户端(OFF)：窗口/渲染/输入/音频/网络全量
# 服务端(ON) ：纯网络+物理，无渲染无音频（SERVER_BUILD 宏生效）
#   例：cmake -S . -B build-server -DBUILD_FOR_SERVER=ON
option(BUILD_FOR_SERVER "Build headless server (no renderer/audio)" OFF)
# ON ：全部依赖静态链入 exe（单文件发布，编译慢体积大）
# OFF：动态链接（exe 小、编译快，运行需 SDL3.dll 等陪同）
option(BUILD_STATIC "Statically link all dependencies into the exe" ON)
# ═══════════════════════════════════════════════════════════════

if (BUILD_FOR_SERVER)
    message("Build for Server")
    project(GameEngineServer LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server)
else ()
    message("Build for Client")
    project(GameEngine LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

# 静态/动态统一驱动：控制所有依赖库的构建方式与 exe 链接方式
# （CACHE FORCE 保证在同一 build 目录里切换时子项目旧缓存不残留）
if (BUILD_STATIC)
    message("Build for Static")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # Box2D / SDL_net 走此标准开关
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    if (WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "-static")   # MinGW 全静态 exe
    endif ()
else ()
    message("Build for Dynamic")
    set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
endif ()

# ── 依赖一：Box2D v2.4（物理，引擎核心依赖，两端共用）─────────────
include(FetchContent)
set(BOX2D_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
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
FetchContent_Declare(
    SDL_net
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
    GIT_TAG 4dd9d8418e9781d10481fd6177e3a1064ca201fe   # main 2026-06-17
    # 不加 GIT_SHALLOW：tag 为 commit hash 时浅克隆不保证成功（CMake 文档警示），
    # 且本库仅 17MB 无利可图
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
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS ${SRC_DIR}/*.cpp)

add_executable(${PROJECT_NAME} ${SRC})
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
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

# 客户端：资源目录随构建拷到 exe 旁
if (NOT BUILD_FOR_SERVER)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SRC_DIR}/Asset/
            $<TARGET_FILE_DIR:GameEngine>/Asset
    )
endif ()
