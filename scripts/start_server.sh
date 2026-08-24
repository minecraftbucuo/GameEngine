#!/usr/bin/env bash
# 一键开服（Linux）：启动无头服务端 + websockify 桥（桥顺带发 WEB 页面）
# 前置：
#   1. ./scripts/build.sh           已构建服务器版（顺带构了 Web 版则网页也能玩）
#   2. pip install websockify
# 用法：./scripts/start_server.sh     （Ctrl+C 同时停掉服务端与桥）
# 可用环境变量覆盖：BRIDGE_PORT（默认 8081）GAME_PORT（默认 6666）
#   注意 GAME_PORT 必须与 config.json 的 network.port 一致（服务端读它监听）
set -euo pipefail
cd "$(dirname "$0")/.."

BRIDGE_PORT="${BRIDGE_PORT:-8081}"
GAME_PORT="${GAME_PORT:-6666}"
SERVER_BIN="build-server/server/GameEngineServer"
WEB_DIR="build-web/web"

[[ -x "$SERVER_BIN" ]] || { echo "错误：未找到 $SERVER_BIN，先执行 ./scripts/build.sh"; exit 1; }
command -v websockify >/dev/null 2>&1 || { echo "错误：未找到 websockify，先执行 pip install websockify"; exit 1; }

# WEB 页面存在则让桥顺带发（一个端口干两件事）；没有也能开服，桌面玩家照常连
WEB_ARGS=()
if [[ -f "$WEB_DIR/GameEngine.html" ]]; then
    WEB_ARGS=(--web "$WEB_DIR")
else
    echo "提示：未找到 $WEB_DIR/GameEngine.html，本次不发网页（仅桌面玩家可连）；"
    echo "      需要 WEB 版请执行 ./scripts/build.sh web 后重新运行本脚本"
fi

mkdir -p logs
echo "==> 启动服务端（监听 $GAME_PORT，端口读 config.json），日志 logs/server.log"
"$SERVER_BIN" >logs/server.log 2>&1 &
SERVER_PID=$!
cleanup() { kill "$SERVER_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "==> 启动桥：ws://0.0.0.0:$BRIDGE_PORT -> 127.0.0.1:$GAME_PORT"
echo "==> 网页玩家入口：http://<本机IP>:$BRIDGE_PORT/GameEngine.html"
echo "==> 桌面玩家：config.json 的 serverIp 填本机 IP 直连 $GAME_PORT"
websockify "$BRIDGE_PORT" "127.0.0.1:$GAME_PORT" ${WEB_ARGS[@]+"${WEB_ARGS[@]}"}
