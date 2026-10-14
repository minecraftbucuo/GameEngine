# 一键 WEB 构建：激活 emsdk -> cmake --preset web 配置 -> 编译
# 产物：build-web/web/GameEngine.html（含 js / wasm / data 四件套）
# 预览：在产物目录起任意静态服务器（如 python -m http.server 8080），
#       浏览器访问 http://localhost:8080/GameEngine.html
# 注意：不能直接双击 html 用 file:// 打开，浏览器会拦截 wasm/data 的 fetch
#
# 说明：构建参数（生成器 / 工具链 / 开关）统一写在 CMakePresets.json 的
# web 预设里，本脚本只负责激活 emsdk 再调用预设。因此不再需要 emcmake 包装
# ——预设的 toolchainFile 已指向 $env{EMSDK}/upstream/emscripten/...，
# 再套一层 emcmake 会重复指定工具链。
param(
    [string]$EmsdkEnv = "E:\Project\GitHub\emsdk\emsdk_env.ps1"
)

$ErrorActionPreference = "Stop"

# 自动定位仓库根（与 .sh 的 cd "$(dirname "$0")/.." 等价）：任意目录均可调用本脚本
Set-Location (Split-Path $PSScriptRoot -Parent)

if (Test-Path $EmsdkEnv) {
    & $EmsdkEnv *> $null
}
else {
    Write-Warning "未找到 emsdk 环境脚本：$EmsdkEnv，假设环境已在当前终端手动激活"
}

if (-not $env:EMSDK) {
    throw "EMSDK 环境变量未设置（web 预设的 toolchainFile 依赖它）。请先激活 emsdk，或用 -EmsdkEnv 指定 emsdk_env.ps1 的路径。"
}

cmake --preset web -G "MinGW Makefiles"
if ($LASTEXITCODE -ne 0) { throw "配置失败（exit=$LASTEXITCODE），请把上方完整日志发我" }

cmake --build --preset web --parallel
if ($LASTEXITCODE -ne 0) { throw "编译失败（exit=$LASTEXITCODE），请把上方完整日志发我" }

Write-Host ""
Write-Host "==== 构建完成 ===="
Write-Host "产物目录：$(Resolve-Path 'build-web/web')"
Write-Host "应包含四件套：GameEngine.html / GameEngine.js / GameEngine.wasm / GameEngine.data"
