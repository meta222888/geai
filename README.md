# Geai

**Geai = Gemini Windows Client / Gemini Windows 客户端**

Geai 是一个小巧的原生 Windows Gemini 客户端。目标是：不用 Electron、不用 Qt、不带复杂运行时，使用 C++ Win32 + WinHTTP 实现一个轻量桌面聊天工具。

## 功能

- 原生 Win32 GUI，小巧、启动快
- Gemini API Key 设置
- API Base URL 设置，可直接使用官方 Gemini API 或自建代理
- HTTP/HTTPS 代理设置
- 本地会话记录，JSONL 自动保存
- 左侧会话列表，最近聊天在最上面
- 右键删除会话
- 客户端自动提取 Gemini 正文，不直接显示完整 JSON
- Deno Deploy 免费代理示例
- Deno 代理认证示例，避免代理被公开滥用
- CMake + GitHub Actions Windows 构建
- Windows `.bat` 一键调试运行 / Release 打包脚本

## 使用 Deno 认证代理

推荐使用认证版代理：

```text
deno-proxy/authenticated-main.ts
```

Deno Deploy 环境变量设置：

```text
GEMINI_API_KEY=你的 Gemini API Key
GEAI_PROXY_TOKEN=你自己设置的一串随机密码
```

Geai 客户端 Settings：

```text
API Base = https://你的-deno-地址
API Key = GEAI_PROXY_TOKEN 的值
Model = gemini-2.5-flash
```

说明：

- 直接使用 Gemini 官方 API 时，API Key 就是 Gemini API Key
- 使用 Deno 代理时，API Key 作为代理认证 token 使用
- 客户端会通过 `X-Geai-Token` 请求头发送认证 token
- Deno 代理会在服务端使用 `GEMINI_API_KEY` 调用 Gemini

## 开发环境运行

```bat
git pull
scripts\clean.bat
scripts\dev-run.bat
```

## 打包 Release

```bat
scripts\release-package.bat
```

打包结果：

```text
dist\Geai-windows-x64.zip
```

## 开发环境要求

- Windows 10/11
- Visual Studio 2022 或 Visual Studio 2026
- Desktop development with C++
- C++ CMake tools for Windows
- Windows 10/11 SDK
- CMake

如果提示 `cmake 不是内部或外部命令`，需要安装 CMake 并重新打开终端。

## 配置文件位置

```text
%APPDATA%\Geai\config.ini
```

会话记录位置：

```text
%APPDATA%\Geai\sessions\
```

## 手动构建

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成文件：

```text
build/Release/Geai.exe
```

## Deno 本地运行认证代理

```bash
cd deno-proxy
export GEMINI_API_KEY="你的 Gemini API Key"
export GEAI_PROXY_TOKEN="你自己设置的一串随机密码"
deno run --allow-net --allow-env authenticated-main.ts
```

Windows PowerShell：

```powershell
cd deno-proxy
$env:GEMINI_API_KEY="你的 Gemini API Key"
$env:GEAI_PROXY_TOKEN="你自己设置的一串随机密码"
deno run --allow-net --allow-env authenticated-main.ts
```

本地 API Base：

```text
http://localhost:8000
```

## 项目结构

```text
Geai/
├── src/
│   ├── main.cpp
│   └── resource.rc
├── scripts/
│   ├── dev-run.bat
│   ├── release-package.bat
│   ├── dev-run-vs2026.bat
│   ├── release-package-vs2026.bat
│   └── clean.bat
├── deno-proxy/
│   ├── main.ts
│   ├── authenticated-main.ts
│   └── deno.json
├── docs/
│   └── ROADMAP.md
├── .github/workflows/build.yml
├── CMakeLists.txt
├── README.md
└── LICENSE
```

## 后续建议功能

- 流式输出
- Markdown 渲染
- 多模型切换
- 会话搜索
- 系统提示词模板
- 托盘常驻
- 快捷键呼出
- 导出 Markdown / JSON
- 本地加密保存 API Key

## License

MIT
