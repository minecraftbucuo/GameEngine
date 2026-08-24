# 幂等地给 vendored SDL_net 应用 TCP_NODELAY 补丁。
# 背景：换工具链/清缓存会让 FetchContent 重跑 populate 流水线，若对已打过
# 补丁的源码树重复执行 git apply，会因上下文不符而报 "patch does not apply"。
# 因此先检测目标文件是否已含补丁标记：有则跳过；没有才真正 apply；
# apply 失败（如上游源码变动）则大声报错终止 configure。
#
# 用法（由 CMakeLists 的 PATCH_COMMAND 调用）：
#   cmake -DSRC_DIR=<sdl_net-src> -DPATCH_FILE=<sdl_net_tcp_nodelay.patch>
#         -DGIT_EXECUTABLE=<git> -P sdl_net_apply_patch.cmake

if(NOT EXISTS "${SRC_DIR}/src/SDL_net.c")
    message(FATAL_ERROR "sdl_net_apply_patch: 找不到 ${SRC_DIR}/src/SDL_net.c")
endif()
if(NOT EXISTS "${PATCH_FILE}")
    message(FATAL_ERROR "sdl_net_apply_patch: 找不到补丁文件 ${PATCH_FILE}")
endif()

file(READ "${SRC_DIR}/src/SDL_net.c" sdl_net_content)
string(FIND "${sdl_net_content}" "SetStreamNoDelay" marker_pos)
if(NOT marker_pos EQUAL -1)
    message(STATUS "SDL_net 源码已含 NODELAY 补丁，跳过 patch 步骤")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace --whitespace=nowarn "${PATCH_FILE}"
    WORKING_DIRECTORY "${SRC_DIR}"
    RESULT_VARIABLE git_apply_rc
    ERROR_VARIABLE git_apply_err
)
if(NOT git_apply_rc EQUAL 0)
    message(FATAL_ERROR
        "SDL_net NODELAY 补丁应用失败（GIT_TAG 对应的上游源码可能已变动，需重新适配补丁）:\n"
        "${git_apply_err}")
endif()
message(STATUS "SDL_net NODELAY 补丁应用成功")
