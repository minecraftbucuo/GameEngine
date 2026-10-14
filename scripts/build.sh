#!/usr/bin/env bash
# 一键构建（Linux）
#   ./scripts/build.sh            交互模式：选构建目标（默认服务器），再问是否构建 Web 版
#   ./scripts/build.sh server     免交互：只构建服务器版（无头，纯网络+物理）
#   ./scripts/build.sh desktop    免交互：只构建桌面版（需图形开发包，服务器上一般没有）
#   ./scripts/build.sh server web 免交互：服务器版 + Web 版（Web 需已安装 emsdk）
# 产物：
#   服务器 build-server/server/GameEngineServer（config.json + maps 已随构建拷入同级 Asset/）
#   桌面   build/bin/GameEngine（Asset 由 CMake 自动拷贝）
#   Web    build-web/web/GameEngine.html 四件套
set -euo pipefail
cd "$(dirname "$0")/.."

TARGET=""
BUILD_WEB=0

if [[ $# -eq 0 ]]; then
    read -rp "构建目标 [1]服务器(默认) [2]桌面: " choice
    TARGET=$([[ "$choice" == "2" ]] && echo desktop || echo server)
    read -rp "是否同时构建 Web 版本? [y/N]: " web
    [[ "$web" =~ ^[Yy] ]] && BUILD_WEB=1
else
    for arg in "$@"; do
        case "$arg" in
            server|desktop|web) ;;
            *) echo "未知参数：$arg（可用：server desktop web）"; exit 1 ;;
        esac
    done
    [[ " $* " == *" server "* ]]  && TARGET=server
    [[ " $* " == *" desktop "* ]] && TARGET=desktop
    [[ " $* " == *" web "* ]]     && BUILD_WEB=1
fi

build_server() {
    echo "==> 构建服务器版 -> build-server/"
    cmake --preset server
    cmake --build --preset server --parallel "$(nproc)"
    echo "==> 服务器版完成：build-server/server/GameEngineServer"
    echo "    （config.json 与关卡地图 JSON 已由 CMake 随构建拷入同级 Asset/）"
}

build_desktop() {
    echo "==> 构建桌面版 -> build/"
    cmake --preset client
    cmake --build --preset client --parallel "$(nproc)"
    echo "==> 桌面版完成：build/bin/GameEngine"
}

build_web() {
    echo "==> 构建 Web 版 -> build-web/"
    # 找 emsdk 环境脚本（可用环境变量 EMSDK_ENV 指定路径）
    local env_script=""
    if [[ -n "${EMSDK_ENV:-}" && -f "$EMSDK_ENV" ]]; then
        env_script="$EMSDK_ENV"
    elif [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
        env_script="$HOME/emsdk/emsdk_env.sh"
    elif [[ -f /opt/emsdk/emsdk_env.sh ]]; then
        env_script="/opt/emsdk/emsdk_env.sh"
    fi
    if [[ -n "$env_script" ]]; then
        # shellcheck disable=SC1090
        source "$env_script"
    fi
    # web 预设的 toolchainFile 指向 $env{EMSDK}/upstream/emscripten/...，
    # 因此不再需要 emcmake 包装（那会和预设重复指定工具链），但必须已激活
    # emsdk（emsdk_env.sh 会设置 EMSDK 与 PATH 里的 emcc/node/python）。
    [[ -n "${EMSDK:-}" ]] || {
        echo "错误：EMSDK 环境变量未设置，请先 source emsdk_env.sh"
        echo "      （安装：git clone https://github.com/emscripten-core/emsdk.git ~/emsdk && ~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest）"
        exit 1
    }
    cmake --preset web
    cmake --build --preset web --parallel "$(nproc)"
    echo "==> Web 版完成：build-web/web/（GameEngine.html 四件套）"
}

[[ "$TARGET" == "server"  ]] && build_server
[[ "$TARGET" == "desktop" ]] && build_desktop
[[ "$BUILD_WEB" == "1"     ]] && build_web
echo "==== 构建结束 ===="
