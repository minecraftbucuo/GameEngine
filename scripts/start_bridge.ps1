# S1 一键联机桥（N5）：websockify 单端口同时服务网页与 WS 桥
# 用法（开服者机器上，仓库根目录）：
#   1. 先启动游戏服务端：桌面构建 GameEngine.exe → 菜单「超级玛丽 Server」
#   2. .\scripts\start_bridge.ps1          # 默认 8081 → 127.0.0.1:6666，页面来自 build-web\web
#   3. 加入者浏览器打开 http://<开服者IP>:8081/GameEngine.html → 点「超级玛丽 Client」
# 注意：
#   - 首次对外开服需放行防火墙（管理员 PowerShell 执行一次）：
#     netsh advfirewall firewall add rule name="Mario Bridge" dir=in action=allow protocol=TCP localport=8081
#   - WEB 包的 config.json 需 serverIp="auto"（页面与桥同源，自动寻址零配置）
#     —— 本机三件套开发流（页面:8000 / 桥:8081 分离）请保持默认 127.0.0.1
#   - 桌面版加入者无需此桥，直接「超级玛丽 Client」连开服者 IP（TCP 6666）
param(
    [string]$WebDir = "build-web\web",
    [int]$ListenPort = 8081,
    [string]$Target = "127.0.0.1:6666"
)

$ErrorActionPreference = "Stop"

# 自动定位仓库根（与 .sh 的 cd "$(dirname "$0")/.." 等价）：任意目录均可调用本脚本
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Get-Command websockify -ErrorAction SilentlyContinue)) {
    throw "未找到 websockify，请先执行：pip install websockify"
}
if (-not (Test-Path (Join-Path $WebDir "GameEngine.html"))) {
    throw "未找到 $WebDir\GameEngine.html，请先执行 .\scripts\build_web.ps1"
}

Write-Host "桥：ws://0.0.0.0:$ListenPort -> $Target"
Write-Host "页面：http://<本机IP>:$ListenPort/GameEngine.html （config serverIp 须为 auto）"
websockify $ListenPort $Target --web $WebDir
