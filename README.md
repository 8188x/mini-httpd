# ⚡ mini-httpd — 从零手写的 C 语言 HTTP 静态文件服务器

一个从零用 C 语言实现的 HTTP/1.1 静态文件服务器，基于 **kqueue（macOS/BSD）**
事件驱动模型。支持 GET/HEAD 请求、MIME 类型识别、目录遍历防护、并发连接处理。

---

## 项目背景

这是哈工大深圳 085404 计算机技术考研复试准备的第二个项目。

**目标：** 通过从零实现 HTTP 服务器，深入理解计算机网络（TCP/HTTP）、
操作系统（IO 多路复用、进程线程模型）和 C 语言系统编程。

---

## 核心特性

- **事件驱动架构** — 单线程 kqueue 事件循环，无锁并发
- **HTTP/1.1 协议** — 支持 GET/HEAD 方法，完整请求解析
- **静态文件服务** — 自动识别 25+ 种文件 MIME 类型
- **路径安全** — 防止目录遍历攻击（`..` 和绝对路径拦截）
- **错误处理** — 完善的 400/403/404/413/500/501 错误页
- **命令行参数** — `-p` 指定端口，`-r` 指定根目录

---

## 快速开始

```bash
# 编译
make

# 运行（默认端口 8080，根目录 www/）
make run

# 或手动指定
./mini-httpd -p 3000 -r /path/to/static/files
```

然后在浏览器打开 http://localhost:8080

---

## 项目架构

```
http-server/
├── Makefile          # 编译构建
├── README.md         # 本文档
├── www/              # 测试用静态文件
│   ├── index.html
│   └── 404.html
└── src/
    ├── main.c        # 入口：参数解析、信号处理
    ├── server.h      # 服务器核心头文件
    ├── server.c      # TCP socket + kqueue 事件循环
    ├── http.h        # HTTP 协议头文件
    ├── http.c        # HTTP 请求解析 + 响应构建 + 客户端处理
    ├── static.h      # 静态文件服务头文件
    ├── static.c      # 静态文件读取、路径安全校验
    ├── mime.h        # MIME 类型头文件
    └── mime.c        # 扩展名 → MIME 类型映射表
```

### 模块说明

| 模块 | 职责 |
|------|------|
| `main.c` | 命令行参数解析（`-p` 端口, `-r` 根目录），信号处理 |
| `server.c` | TCP socket 初始化、kqueue 事件循环、连接接受与分发 |
| `http.c` | HTTP 请求解析（请求行 + 头部字段解析）、响应构建、客户端主处理逻辑 |
| `static.c` | 静态文件查找与读取、URL 解码、路径安全性检查 |
| `mime.c` | 根据文件扩展名返回对应的 Content-Type |

### 请求处理流程

```
浏览器请求 → TCP 连接 → kqueue 通知 → accept() → read() →
http_parse() 解析请求 → static_serve() 读取文件 →
http_build_response() 构建响应 → write() 发送 → close()
```

---

## 技术原理

### 事件驱动（kqueue）

macOS/BSD 提供的高性能 IO 多路复用机制。与传统 `select()` / `poll()` 相比：

| 特性 | select | poll | kqueue |
|------|--------|------|--------|
| 时间复杂度 | O(n) | O(n) | O(1) |
| 最大连接数 | FD_SETSIZE 限制 | 无限制 | 无限制 |
| 事件过滤 | 有限 | 有限 | **丰富（读写/信号/定时器等）** |

kqueue 的优势在于它只返回就绪的事件，而不是让应用遍历所有 fd。
对于高并发场景，这是关键性能优化。

### HTTP 协议实现

- 严格解析请求行（`METHOD /path HTTP/1.1\r\n`）
- 解析任意数量的头部字段（key: value 格式）
- 构建符合 RFC 7230 的响应（状态行 + 头部 + 空行 + 消息体）
- 自动设置 Content-Type 和 Content-Length

### 安全措施

- 拦截包含 `..` 的路径（防止目录穿越）
- 拒绝绝对路径请求（防止读取任意系统文件）
- null 字节检查（防止字符串截断攻击）
- 文件大小限制 16MB（防止 OOM）

---

## 性能测试

与 Python 标准库 `http.server` 的简单对比：

```bash
# 使用 wrk 或 ab 压测
# wrk -t4 -c100 -d10s http://localhost:8080/

# mini-httpd（预期）:
#   Requests/sec: ~15,000+（C + kqueue 事件驱动）
#
# Python http.server（对比）:
#   Requests/sec: ~3,000-5,000（纯 Python + select 模型）
```

> 注：具体性能取决于硬件和系统负载，建议在自己的机器上实测。

---

## 复试面试可能的问题

| 问题 | 参考答案要点 |
|------|-------------|
| 为什么用 kqueue 不用 select？ | O(1) vs O(n)，无连接数限制，事件过滤更丰富 |
| HTTP 请求解析最难处理的是什么？ | 边界情况：空行结束头部、URL 编码、chunked 编码 |
| 怎么防止目录遍历攻击？ | `..` 拦截 + 绝对路径拒绝 + null 字节检查 |
| 单线程事件驱动有什么优缺点？ | 优点：无锁、无上下文切换；缺点：阻塞操作会阻塞所有连接 |
| 如果客户端不发送完整请求怎么办？ | 目前使用阻塞 read，改进方向：缓冲 + 超时 + 状态机 |

---

## 待改进 / 扩展方向

- [ ] 支持 HTTP Keep-Alive（Connection: keep-alive）
- [ ] 支持 POST 请求和动态路由（CGI）
- [ ] 内部错误页改为从文件读取
- [ ] 增加 access log
- [ ] 支持 ETag / If-Modified-Since 缓存
- [ ] 迁移到 epoll（Linux）或 iocp（Windows）实现跨平台

---

*时间：2026-06-26 | 哈工大深圳 085404 复试项目 #2*
