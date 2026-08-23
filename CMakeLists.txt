cmake_minimum_required(VERSION 3.20)

set(WIN_STATIC_LINK ON)
set(BUILD_FOR_SERVER OFF)
option(BUILD_SFML_FROM_SOURCE "Build SFML from source using FetchContent" OFF)

if (BUILD_FOR_SERVER)
    message("Build for Server")
    project(GameEngineServer LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server)
else ()
    message("Build for C/S")
    project(GameEngine LANGUAGES CXX)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()

if (BUILD_SFML_FROM_SOURCE)
    message(STATUS "Building SFML from source via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 2.6.1
    )
    # SDL3 迁移 Step 11：渲染栈已换 SDL3，SFML 只剩 network+system 给 Network/ 用
    set(BUILD_SHARED_LIBS OFF)
    set(SFML_BUILD_AUDIO OFF)
    set(SFML_BUILD_NETWORK ON)
    set(SFML_BUILD_GRAPHICS OFF)
    set(SFML_BUILD_WINDOW OFF)
    FetchContent_MakeAvailable(SFML)
    set(SFML_ROOT "${sfml_SOURCE_DIR}")
else ()
    # 设置 SFML 路径（仅 network + system，Network/ 模块迁移范围外保留）
    find_package(SFML 2.6 COMPONENTS system network QUIET)
    if (SFML_FOUND)
        message(STATUS "Using system SFML: ${SFML_DIR}")
    else ()
        # 如果系统没有提供 SFML，使用项目提供的版本
        message(STATUS "System SFML not found, using bundled version")
        if (WIN32)
            set(SFML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/SFML-2.6.1-gcc")
        else ()
            set(SFML_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib/SFML-2.6.1-linux")
        endif ()
    endif ()
endif ()

if (WIN32 AND WIN_STATIC_LINK)
    add_definitions(-DSFML_STATIC)
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

# ── SDL3 渲染栈（迁移 Step 11 起唯一后端，客户端构建无条件拉取）──────
# 版本对齐（已核实各卫星库源码 CMakeLists 的 SDL_REQUIRED_VERSION）：
#   mixer 3.2.0 要求 SDL≥3.4.0，ttf 3.2.0 要求 ≥3.2.6，image 3.2.0 要求 ≥3.2.0
#   → SDL 核心 3.4.14（2026-08，3.4 系列 ABI 稳定）满足全部。
if (NOT BUILD_FOR_SERVER)
    message(STATUS "Building with SDL3 ecosystem (renderer backend)")
    include(FetchContent)

    # vendored freetype 声明 cmake_minimum_required(3.0)，CMake 4.x 已移除 <3.5 兼容；
    # 官方缓解：放宽策略版本下限（仅影响旧策略告警，不影响构建产物）
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

    # SDL 本体：静态库（与 SFML 静态链接策略一致）
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
    set(SDL_TEST OFF)
    set(SDL_INSTALL OFF)

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
        SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.14
    )
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
    # SDL 必须在最前，卫星库通过 SDL3::SDL3 目标自动找到它
    FetchContent_MakeAvailable(SDL SDL_image SDL_ttf SDL_mixer)
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
        ${SFML_ROOT}/include
)

# ── 链接（SDL3 迁移 Step 11 单后端终态）─────────────────────────────
# 客户端：渲染/窗口/输入/音频 = SDL3 全家；
# 客户端与服务端共同：SFML network+system（Network/ 模块迁移范围外保留）
if (NOT BUILD_FOR_SERVER)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            SDL3::SDL3
            SDL3_image::SDL3_image
            SDL3_ttf::SDL3_ttf
            SDL3_mixer::SDL3_mixer
    )
endif ()

if (BUILD_SFML_FROM_SOURCE)
    target_link_libraries(${PROJECT_NAME} PRIVATE sfml-network sfml-system)
else ()
    target_link_directories(${PROJECT_NAME} PRIVATE ${SFML_ROOT}/lib)
    if (WIN32)
        if (WIN_STATIC_LINK)
            target_link_libraries(${PROJECT_NAME} PRIVATE
                    debug sfml-network-s-d optimized sfml-network-s
                    debug sfml-system-s-d optimized sfml-system-s
                    ws2_32 winmm)
        else ()
            target_link_libraries(${PROJECT_NAME} PRIVATE
                    debug sfml-network-d optimized sfml-network
                    debug sfml-system-d optimized sfml-system)
        endif ()
    else ()
        target_link_libraries(${PROJECT_NAME} PRIVATE sfml-network sfml-system)
    endif ()
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