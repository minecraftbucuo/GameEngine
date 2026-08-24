# WEB 联机部署手册（N5）

> 配套计划：[websocket-net-plan.md](websocket-net-plan.md)（方向 A′：TcpClient WS 后端 + websockify 桥，服务端零改动）
> 前置阅读：WEB 客户端构建见 [wasm-web-port-plan.md](wasm-web-port-plan.md)，构建命令 `.\scripts\build_web.ps1`

## 各组件职责：这套部署里每个东西在干嘛

| 组件 | 是什么 | 职责 | 不负责 |
|---|---|---|---|
| **GameEngine 服务端** | 桌面构建的 `GameEngine.exe`（或 `BUILD_FOR_SERVER=ON` 无头构建） | 唯一的**游戏权威**：接连接、验证 token、跑物理 tick、把所有玩家状态广播给所有客户端 | 它**不知道**任何客户端是桌面还是网页——对它来说全是 TCP 字节流 |
| **websockify 桥** | Python 工具（`pip install websockify` 装的命令行程序） | **翻译官**：浏览器只被允许说 WebSocket（沙箱限制，WASM 拿不到裸 TCP socket），而服务端只认 TCP。桥把网页的每条 WS 二进制消息原样写进 TCP，把 TCP 收到的每个字节原样塞回 WS——**纯字节泵，不理解游戏协议**（这也是服务端零改动的原因） | 不做验证、不存状态、不转发到多个目标；桥进程挂了 = 所有网页客户端掉线（桌面版无感） |
| **页面服务** | websockify 的一个**命令行开关**（`--web 目录`，见下方命令解析）附带开启的静态文件下载功能 | 浏览器请求 `http://.../GameEngine.html` 时，把该目录里的构建产物（html/js/wasm/data 共约 29MB）发回去——**只在打开页面那一刻用，之后游戏数据完全不经过它** | 不参与游戏通信。开着它纯粹是省一个端口/一个进程（S2 里这两职分家：nginx 发页面、websockify 只当桥） |
| **桌面客户端** | 桌面构建的 exe | TCP 直连 6666，不经桥 | 不需要 websockify、不需要浏览器 |
| **`serverIp="auto"`** | config.json 的 `network.serverIp` 字段的一个取值 | 点「超级玛丽 Client」时网页问浏览器“我是从哪个地址打开的”（`location`），用它当连接目标——省去按开服者 IP 重新构建 WEB 包 | 只在「页面与桥同端口」拓扑成立（auto 拿到的是页面地址，页面地址=桥地址）；页面与桥分端口时（本机开发三件套）必须填 IP |

**一句话总结**：服务端是大脑（只认 TCP），桥是翻译官（WS↔TCP），页面服务器是发传单的（发完就走），桌面客户端走后门（TCP 直连）。

**为什么必须有桥**：WASM 出于安全被浏览器沙箱限制，无法建立裸 TCP 连接；WebSocket 是浏览器世界里唯一的原生双向通道。所以“网页联机”必然 = WebSocket 到某处 → 某处转成 TCP → 服务端。这个“某处”就是桥。

## 拓扑速览

```
桌面客户端 ──TCP 6666──────────────┐
                                   ▼
                          GameEngine 服务端（开服者 / VPS）
                                   ▲
网页客户端 ──ws(s)://…:8081──► websockify 桥（WS↔TCP 纯字节泵）
                                  │
              websockify 的 --web 开关顺带发页面四件套（仅页面加载时）
```

- **同一个服务端进程**同时接纳桌面客户端（TCP 直连）与网页客户端（经桥），互相可见
- 桥只服务网页客户端；桌面版完全不需要桥
- 游戏协议（uint32 长度前缀分帧）全程不变，桥不做任何解析

## S1 局域网 / 朋友开服（推荐起步）

开服者只需两步 + 一次防火墙配置；加入者零安装，浏览器直接玩。

### 开服者（Windows）

1. **启动服务端**：运行桌面构建的 `GameEngine.exe` → 菜单点「超级玛丽 Server」（或 `BUILD_FOR_SERVER=ON` 的无头服务端构建）。就是正常开服，和纯桌面联机完全一样——网页玩家能不能进来，跟这一步无关

2. **拉起桥**（仓库根目录）：
   ```powershell
   .\scripts\start_bridge.ps1
   # 可选参数：-WebDir build-web\web -ListenPort 8081 -Target 127.0.0.1:6666
   ```
   这个脚本做三件事：①检查 websockify 已安装、WEB 构建产物存在（缺了直接报错告诉你装/构建）；②打印访问地址；③执行真正的主角——下面这条命令：
   ```powershell
   websockify 8081 127.0.0.1:6666 --web build-web\web
   ```
   逐参数解释：
   | 参数 | 干了啥 |
   |---|---|
   | `8081` | 监听端口：桥在 8081 上等 WebSocket 连接（网页客户端连的就是它） |
   | `127.0.0.1:6666` | 转发目标：每来一条 WS 连接，桥就向这个 TCP 地址新开一条连接，之后双向搬运字节。`127.0.0.1` = 服务端和桥在同一台机器（TCP 连接是桥自己发起的，所以走本机回环就够） |
   | `--web build-web\web` | 顺手开个静态文件下载：浏览器请求 `http://…:8081/GameEngine.html` 时，把这个目录里的构建产物发回去。不开这个开关就只当桥、不发页面，页面得另找 http 服务器发 |

   跑起来后**一个端口干两件事**：浏览器要文件它给文件（http 请求），要游戏通道它给通道（WebSocket 连接）——所以脚本之后，加入者只需要一个地址 `http://<开服者IP>:8081/GameEngine.html`，连本机开发三件套里发页面用的 `python -m http.server` 都省了

3. **防火墙放行**（管理员 PowerShell，首次一次即可）：
   ```powershell
   netsh advfirewall firewall add rule name="Mario Bridge" dir=in action=allow protocol=TCP localport=8081
   ```
   这是 Windows 自带防火墙的命令行工具：`add rule` 新建规则；`dir=in` 方向=入站（别人连你）；`action=allow` 动作=放行；`protocol=TCP localport=8081` 只对 TCP 8081 端口生效。不放行的话，外网/局域网其他机器的连接会被 Windows 静默拦截，表现就是“自己能开自己能连，别人打不开页面”。`name` 随便取，将来删除规则用得上（`... delete rule name="Mario Bridge"`）

### 发布 WEB 包（构建者做一次）

`config.json` 的 `serverIp` 改为 `"auto"` 后执行 `.\scripts\build_web.ps1`——此后该构建包
在**任何**开服者机器上都零配置（客户端自动连页面自身来源）。注意：

- `auto` 仅适用于「页面与桥同端口」拓扑（`start_bridge.ps1` 即是）
- 本机开发三件套（页面 `:8000` / 桥 `:8081` 分离）请保持默认 `127.0.0.1`
- 桌面版不受 `auto` 影响（桌面分支不读该值的特殊语义）

### 加入者

- **网页**：浏览器开 `http://<开服者IP>:8081/GameEngine.html` → 「超级玛丽 Client」
- **桌面**：`config.json` 的 `serverIp` 填开服者 IP → 「超级玛丽 Client」（TCP 直连 6666，不走桥）

## S2 公网 VPS 拓扑

```
浏览器 ──https──► nginx（TLS 终结 + 静态页面）
              └──wss://域名/ws──► proxy_pass ──► websockify(:8081) ──TCP──► GameEngineServer(:6666)
```

要点：**https 页面只能连 wss**（混合内容限制），所以公网部署必须 TLS 终结；客户端
`config.json` 的 `serverIp` 直接填完整 URL `"wss://你的域名/ws"`（N3 已支持完整 ws(s):// 直连）。

### nginx 站点配置（样例）

nginx 在这台机器上干两件事：①对外发 https 页面（TLS 终结）；②把 `wss://` 流量转给本机的 websockify。逐块解释：

```nginx
server {
    listen 443 ssl;                # 监听 443 端口并开启 TLS——浏览器 https 的标准端口，
                                   # ssl 表示这条连接由 nginx 负责加解密（即"TLS 终结"：
                                   # 外面 https/wss，里面到各后端是明文）
    server_name mario.example.com; # 命中这个域名的请求才进本块
    ssl_certificate     /etc/letsencrypt/live/mario.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/mario.example.com/privkey.pem;
                                   # 证书与私钥路径，certbot 自动生成（见下）

    # ── 职责①：发页面 ──
    root /var/www/mario;           # 网站根目录：把 WEB 构建四件套拷到这里
    index GameEngine.html;         # 访问 https://域名/ 时默认返回的文件

    # ── 职责②：WebSocket 反向代理 ──
    # 浏览器连 wss://域名/ws 时，nginx 把这条连接转给本机 8081 的 websockify。
    # 注意这里 websockify 启动时不加 --web：页面由 nginx 发，桥只当桥（两职分家）
    location /ws {
        proxy_pass http://127.0.0.1:8081;   # 转发目标：本机 websockify
        proxy_http_version 1.1;             # WebSocket 握手需要 HTTP/1.1 的 Upgrade 机制，
                                            # nginx 默认对后端用 1.0，必须手动升
        proxy_set_header Upgrade $http_upgrade;     # 把浏览器的"升级为 WebSocket"请求头
        proxy_set_header Connection "upgrade";      # 原样传给后端——缺了这两行握手必失败，
                                                    # 这是 WS 反代最常见的坑
        proxy_read_timeout 3600s;           # 空闲超时放大到 1 小时：默认 60s 无数据就掐
        proxy_send_timeout 3600s;           # 连接，对"挂着不动"的游戏长连接是灾难
    }
}
```

证书用 certbot 一条命令：`certbot --nginx -d mario.example.com`（certbot 是 Let's Encrypt 的官方客户端，`--nginx` 让它自动改 nginx 配置挂载证书并续期，`-d` 指定域名；执行前先把域名 DNS 解析到这台 VPS）。

### systemd 服务单元（样例）

systemd 是 Linux 的服务管理器：把进程注册成"服务"，开机自启、崩溃自动拉起。两个单元文件逐字段解释：

```ini
# /etc/systemd/system/mario-server.service —— 游戏服务端
[Unit]
Description=GameEngine headless server   # 人读的描述
After=network.target                     # 网络就绪后才启动本服务

[Service]
WorkingDirectory=/opt/mario              # 工作目录：服务端在这里找 ./Asset 等资源
ExecStart=/opt/mario/GameEngineServer    # 要跑的程序（无头服务端构建产物）
Restart=always                           # 进程无论怎么退出都自动重启

[Install]
WantedBy=multi-user.target               # 开机到多用户模式（正常默认目标）即自启
```

```ini
# /etc/systemd/system/mario-bridge.service —— websockify 桥
[Unit]
Description=websockify WS-TCP bridge
After=network.target mario-server.service  # 排序约束：先起游戏服务端再起桥
                                           #（桥先起也没事，这里只是顺序好看）

[Service]
ExecStart=/usr/bin/websockify 8081 127.0.0.1:6666   # 同 S1 的裸命令，但不加 --web：
                                                    # 页面 nginx 发，桥只当桥
Restart=always

[Install]
WantedBy=multi-user.target
```

改完单元文件先 `systemctl daemon-reload`（让 systemd 重新读配置）再启用：`systemctl enable --now mario-server mario-bridge`（`enable` = 开机自启，`--now` = 现在立刻启动，等价于再执行一遍 `start`）。看状态/日志：`systemctl status mario-server`、`journalctl -u mario-bridge -f`。

> **现状备注**：服务端当前仅验证过 Windows 构建（路径解析走 Win32 API）。Linux VPS
> 部署前需先完成 Linux 构建适配（`getExeDir` 的 `/proc/self/exe` 分支已就绪，但未实测）；
> Windows VPS 等价方案：NSSM 或任务计划程序把两个进程注册为服务，拓扑不变。

## 端口与配置速查

| 项 | 默认值 | 配置键 | 说明 |
|---|---|---|---|
| 游戏服务端 | 6666 | `network.port` | 桌面直连 & 桥的目标端口 |
| websockify 桥 | 8081 | `network.webBridgePort` | 仅网页客户端使用 |
| 网页客户端地址 | `127.0.0.1` | `network.serverIp` | 三种形态：`auto`（同源自动）/ 完整 `ws(s)://` URL / `IP`（拼桥端口） |
| 桌面客户端地址 | `127.0.0.1` | `network.serverIp` | `IP`（TCP 直连 `network.port`），不支持 auto/URL 语义 |

## 故障排查

| 现象 | 首查 |
|---|---|
| 网页点 Client 秒弹 CONNECTION LOST | 桥没起 / 防火墙没放行 / `serverIp` 与拓扑不匹配（auto 用于同端口，127.0.0.1 用于本机开发） |
| 页面打不开 | `start_bridge.ps1 -WebDir` 参数指向的目录里没有 `GameEngine.html`（先跑 `build_web.ps1`）；桥没跑起来 |
| 桌面能连、网页不能 | 桥目标端口 ≠ 服务端实际端口；F12 Console 看 WS 握手错误码 |
| 公网连不上 | https 页面必须 wss；nginx 缺 Upgrade 头透传；证书过期 |
| 双击 html 打不开 | 正常——`file://` 会拦截 wasm/data，必须走 http 服务器 |
