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
    set(BUILD_SHARED_LIBS OFF)
    set(SFML_BUILD_AUDIO OFF)
    set(SFML_BUILD_NETWORK ON)
    set(SFML_BUILD_GRAPHICS OFF)
    set(SFML_BUILD_WINDOW OFF)
    if (NOT BUILD_FOR_SERVER)
        set(SFML_BUILD_AUDIO ON)
        set(SFML_BUILD_GRAPHICS ON)
        set(SFML_BUILD_WINDOW ON)
    endif ()
    FetchContent_MakeAvailable(SFML)
    set(SFML_ROOT "${sfml_SOURCE_DIR}")
else ()
    # 设置 SFML 路径
    # 优先尝试系统提供的 SFML
    find_package(SFML 2.6 COMPONENTS system network graphics window audio QUIET)
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

if (BUILD_SFML_FROM_SOURCE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        sfml-system
        sfml-network
    )
    if (NOT BUILD_FOR_SERVER)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            sfml-graphics
            sfml-window
            sfml-audio
        )
    endif ()
else ()
    target_link_directories(
            ${PROJECT_NAME}
            PRIVATE
            ${SFML_ROOT}/lib
    )

    if (WIN32)
        if (WIN_STATIC_LINK)
            message("Static link")
            # Windows 下静态链接
            list(APPEND LINK_LIBRARIES
                    debug sfml-audio-s-d
                    debug sfml-graphics-s-d
                    debug sfml-window-s-d
                    debug sfml-network-s-d
                    debug sfml-system-s-d

                    optimized sfml-audio-s
                    optimized sfml-graphics-s
                    optimized sfml-window-s
                    optimized sfml-network-s
                    optimized sfml-system-s

                    # OpenAL 音频库
                    openal32
                    # 音频编解码器库
                    flac
                    vorbisenc
                    vorbisfile
                    vorbis
                    ogg
                    # 其他依赖库
                    freetype
                    opengl32
                    winmm
                    ws2_32
            )
        else ()
            message("Dynamic link")
            list(APPEND LINK_LIBRARIES
                    debug sfml-system-d
                    debug sfml-network-d
                    optimized sfml-system
                    optimized sfml-network
            )
            if (NOT BUILD_FOR_SERVER)
                list(APPEND LINK_LIBRARIES
                        debug sfml-graphics-d
                        debug sfml-window-d
                        debug sfml-audio-d
                        optimized sfml-graphics
                        optimized sfml-window
                        optimized sfml-audio
                )
            endif ()
        endif ()
    else ()
        list(APPEND LINK_LIBRARIES
                sfml-system
                sfml-network
        )
        if (NOT BUILD_FOR_SERVER)
            list(APPEND LINK_LIBRARIES
                    sfml-graphics
                    sfml-window
                    sfml-audio
            )
        endif ()
    endif ()

    target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBRARIES})
endif ()

# 链接 Box2D（无论 SFML 用何种方式都需附加）
if (BUILD_WITH_BOX2D)
    target_link_libraries(${PROJECT_NAME} PRIVATE box2d)
endif ()

if (NOT BUILD_FOR_SERVER)
    add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${SRC_DIR}/Asset/
            $<TARGET_FILE_DIR:GameEngine>/Asset
    )
endif ()