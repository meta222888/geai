# Geai

**Geai = Gemini Windows Client / Gemini Windows 客户端**

Geai 是一个小巧的原生 Windows Gemini 客户端。目标是：不用 Electron、不用 Qt、不带复杂运行时，使用 C++ Win32 + WinHTTP 实现一个轻量桌面聊天工具。

## 功能

- 原生 Win32 GUI，小巧、启动快
- Gemini API Key 设置
- API Base URL 设置，可直接使用官方 Gemini API 或自建代理
- HTTP/HTTPS 代理设置
- 本地会话记录，JSONL 保存
- 一键新会话、保存会话
- Deno Deploy 免费代理示例
- CMake + GitHub Actions Windows 构建
- Windows `.bat` 一键调试运行 / Release 打包脚本

## 技术栈

- C++20
- Win32 API
- WinHTTP
- CMake
- Deno Deploy proxy sample

## 开发环境要求

推荐安装：

- Windows 10/11
- Visual Studio 2022 或 Visual Studio 2026
- Desktop development with C++
- C++ CMake tools for Windows
- Windows 10/11 SDK
- CMake

如果提示 `cmake 不是内部或外部命令`，可以安装：

```powershell
winget install Kitware.CMake
```

安装后重新打开终端。

## 一键脚本

先拉取最新代码：

```bat
git pull
```

### 调试运行，默认 VS 2022

```bat
scripts\dev-run.bat
```

这个脚本会自动：

1. 检查 CMake
2. 配置 Debug 构建目录
3. 编译 Debug 版本
4. 启动 `build\Debug\Geai.exe`

### 打包 Release，默认 VS 2022

```bat
scripts\release-package.bat
```

打包结果：

```text
dist\Geai-windows-x64.zip
```

包内包含：

- `Geai.exe`
- `README.md`
- `LICENSE`
- `deno-proxy/`

### Visual Studio 2026 脚本

如果你的 CMake 支持 `Visual Studio 18 2026` 生成器，可以使用：

```bat
scripts\dev-run-vs2026.bat
scripts\release-package-vs2026.bat
```

如果 VS 2026 脚本失败，通常是当前 CMake 还没有识别 `Visual Studio 18 2026`，可以改用默认脚本，或直接用 Visual Studio 打开项目文件夹。

### 清理构建文件

```bat
scripts\clean.bat
```

会删除：

```text
build/
dist/
```

## 手动构建

### Windows + Visual Studio 2022

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

生成文件：

```text
build/Release/Geai.exe
```

### Debug 手动运行

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\Geai.exe
```

## 使用

首次启动后点击 **Settings**：

- API Key：你的 Gemini API Key
- API Base：默认 `https://generativelanguage.googleapis.com`
- Model：默认 `gemini-1.5-flash`
- Proxy：可选，例如 `http://127.0.0.1:7890`

配置文件保存在：

```text
%APPDATA%\Geai\config.ini
```

会话记录保存在：

```text
%APPDATA%\Geai\sessions\
```

## 自建免费 Deno Gemini 代理

如果你不想在客户端直接暴露 Gemini API Key，可以用 Deno Deploy 创建一个免费代理。

### 1. 创建 Deno Deploy 项目

打开 Deno Deploy，新建项目。

### 2. 添加环境变量

在项目环境变量中添加：

```text
GEMINI_API_KEY=你的 Gemini API Key
```

### 3. 部署代理代码

把仓库中的 `deno-proxy/main.ts` 直接部署到 Deno Deploy。

### 4. 在 Geai 中设置 API Base

假设你的 Deno Deploy 地址是：

```text
https://your-project.deno.dev
```

那么 Geai 设置中：

```text
API Base = https://your-project.deno.dev
API Key = 留空或任意字符串
```

客户端会请求：

```text
/v1beta/models/{model}:generateContent
```

代理会自动转发到 Gemini 官方 API，并使用服务端环境变量中的 `GEMINI_API_KEY`。

## Deno 本地运行代理

```bash
cd deno-proxy
export GEMINI_API_KEY="你的 Gemini API Key"
deno task dev
```

Windows PowerShell 可以这样设置环境变量：

```powershell
$env:GEMINI_API_KEY="你的 Gemini API Key"
deno task dev
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
