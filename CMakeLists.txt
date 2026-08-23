cmake_minimum_required(VERSION 3.20)

set(WIN_STATIC_LINK ON)
set(BUILD_FOR_SERVER OFF)

if (BUILD_FOR_SERVER)
    message("Build for Server")
    project(GameEngineServer LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server)
else ()
    message("Build for C/S")
    project(GameEngine LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

if (WIN32 AND WIN_STATIC_LINK)
    set(CMAKE_EXE_LINKER_FLAGS "-static")
endif ()

# ── Box2D 物理引擎接入 ──────────────────────────────────────────────
# 原子性：纯新增依赖，不改任何现有源码逻辑；关 BUILD_WITH_BOX2D 即恢复原状态
option(BUILD_WITH_BOX2D "Build with Box2D physics engine" ON)
if (BUILD_WITH_BOX2D)
    message(STATUS "Building with Box2D physics engine")
    include(FetchContent)
    FetchContent_Declare(
        Box2D
        GIT_REPOSITORY https://github.com/erincatto/box2d.git
        GIT_TAG v2.4.1
    )
    # Box2D 默认会编译示例和测试，关闭以加速构建
    set(BUILD_SHARED_LIBS OFF)
    set(BOX2D_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Box2D)
endif ()
# ───────────────────────────────────────────────────────────────────

# ── SDL3 本体 + SDL_net（SDL_net 迁移 N2 起两端共用）──────────────
# 版本对齐（已核实各卫星库源码 CMakeLists 的 SDL_REQUIRED_VERSION）：
#   mixer 3.2.0 要求 SDL≥3.4.0，ttf 3.2.0 要求 ≥3.2.6，image 3.2.0 要求 ≥3.2.0
#   → SDL 核心 3.4.14（2026-08，3.4 系列 ABI 稳定）满足全部。
# 客户端专属卫星库（image/ttf/mixer）见下方 NOT BUILD_FOR_SERVER 块。
# SDL_net 依赖 SDL3 核心，而服务端（SERVER_BUILD）也要用网络 →
# SDL 本体从"仅客户端"提升为无条件拉取；image/ttf/mixer 仍留客户端块。
# SDL_net 3 无 release tag（最新 release 仍是 2.2.0），pin main 分支 commit。
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

# SDL 本体：静态库（与 SFML 静态链接策略一致）
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
set(SDL_TEST OFF)
set(SDL_INSTALL OFF)

# SDL_net：静态构建（已核对其 main CMakeLists：无 SDLNET_STATIC 专有开关，
# 走标准 BUILD_SHARED_LIBS；子项目模式下检测到 SDL3::SDL3 目标即跳过 find_package）
# 缓存形式防子项目 cmake_dependent_option 以默认值覆盖
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLNET_INSTALL OFF)

include(FetchContent)
FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
)
FetchContent_Declare(
    SDL_net
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
    GIT_TAG 4dd9d8418e9781d10481fd6177e3a1064ca201fe   # main 2026-06-17 "PS Vita support"
)
# SDL 必须在前，SDL_net 通过 SDL3::SDL3 目标找到它
FetchContent_MakeAvailable(SDL SDL_net)

if (NOT BUILD_FOR_SERVER)
    message(STATUS "Building with SDL3 ecosystem (renderer backend)")

    # SDL_image：资源全为 PNG；PNG 走内置 stb 解码（零外部依赖），裁掉用不到的格式
    set(SDLIMAGE_JPG OFF)
    set(SDLIMAGE_TIF OFF)
    set(SDLIMAGE_WEBP OFF)
    set(SDLIMAGE_JXL OFF)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_INSTALL OFF)

    # SDL_ttf：vendored freetype（子模块已随 FetchContent 拉取）；
    # 关 harfbuzz/plutosvg（重依赖，中文 UTF-8 渲染不需要文本整形）
    set(SDLTTF_VENDORED ON)
    set(SDLTTF_HARFBUZZ OFF)
    set(SDLTTF_PLUTOSVG OFF)
    set(SDLTTF_INSTALL OFF)

    # SDL_mixer：WAV 核心内置；OGG 默认 stb_vorbis（源码内置零依赖）；
    # 关掉用不到的格式（前缀两种写法都设，源码尚未下载无法确认，多余项无害）
    set(SDLMIXER_MP3 OFF)
    set(SDL3MIXER_MP3 OFF)
    set(SDLMIXER_FLAC OFF)
    set(SDL3MIXER_FLAC OFF)
    set(SDLMIXER_MOD OFF)
    set(SDL3MIXER_MOD OFF)
    set(SDLMIXER_OPUS OFF)
    set(SDL3MIXER_OPUS OFF)
    set(SDLMIXER_INSTALL OFF)
    set(SDL3MIXER_INSTALL OFF)

    FetchContent_Declare(
        SDL_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG release-3.2.0
    )
    FetchContent_Declare(
        SDL_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-3.2.0
    )
    FetchContent_Declare(
        SDL_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-3.2.0
    )
    # SDL 本体已在上方两端共用块拉取；这里只拉客户端专属卫星库
    FetchContent_MakeAvailable(SDL_image SDL_ttf SDL_mixer)
endif ()
# ───────────────────────────────────────────────────────────────────

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

# ── 链接（SDL_net 迁移 N5：SFML 全仓移除，SDL 全家 + Box2D）────────
# 客户端：渲染/窗口/输入/音频/网络 = SDL3 全家；
# 服务端：SDL_net（传递链接 SDL3 核心）
if (NOT BUILD_FOR_SERVER)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            SDL3::SDL3
            SDL3_image::SDL3_image
            SDL3_ttf::SDL3_ttf
            SDL3_mixer::SDL3_mixer
            SDL3_net::SDL3_net
    )
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL3_net::SDL3_net)
endif ()

# 链接 Box2D（物理引擎，两端共用）
if (BUILD_WITH_BOX2D)
    target_link_libraries(${PROJECT_NAME} PRIVATE box2d)
endif ()

# ───────────────────────────────────────────────────────────────────
# （SDL3 迁移 Step 9 的 EngineSDL3 编译验证目标已在 Step 10 移除：
#   主目标已直接编译 RendererSDL3.cpp / AssetManager.cpp）

if (NOT BUILD_FOR_SERVER)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SRC_DIR}/Asset/
            $<TARGET_FILE_DIR:GameEngine>/Asset
    )
endif ()