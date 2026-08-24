# 一键 WEB 构建：激活 emsdk -> emcmake 配置 -> 编译
# 产物：<BuildDir>/web/GameEngine.html（含 js / wasm / data 四件套）
# 预览：在产物目录起任意静态服务器（如 python -m http.server 8080），
#       浏览器访问 http://localhost:8080/GameEngine.html
# 注意：不能直接双击 html 用 file:// 打开，浏览器会拦截 wasm/data 的 fetch
param(
    [string]$BuildDir = "build-web",
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

emcmake cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "emcmake 配置失败（exit=$LASTEXITCODE），请把上方完整日志发我" }

cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "编译失败（exit=$LASTEXITCODE），请把上方完整日志发我" }

Write-Host ""
Write-Host "==== 构建完成 ===="
Write-Host "产物目录：$(Resolve-Path (Join-Path $BuildDir 'web'))"
Write-Host "应包含四件套：GameEngine.html / GameEngine.js / GameEngine.wasm / GameEngine.data"
