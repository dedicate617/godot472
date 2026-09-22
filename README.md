# MindSCADA Engine Extension (mindscada-engine-ext)

Godot 4.7.2 工业级工控 / SCADA 引擎外置模块与多平台构建编排工程。

---

## 一、项目定位

本项目为 **MindSCADA** 引擎的外置扩展与编排仓库。通过将自定义 C++ 模块、引擎定制补丁、预编译第三方依赖以及自动化构建流程从 Godot 官方核心源码中彻底解耦，实现：

1. **非侵入式扩展**：依托 SCons 的 `custom_modules` 机制加载核心业务模块。WebAssembly 平台自动裁减原生窗口模块（`qtwindow`、`webview`）并启用轻量存储与通信回退层。
2. **多版本适配性**：补丁以标准 patch 形式版本化维护（如 `patches/4.7.2/`），便于随 Godot 引擎上游快速升级演进。
3. **跨平台全矩阵编排**：统一管理 **Windows x64**、**Linux ARM (ARM32/ARM64)** 及 **Web (WASM32 Threads)** 平台的预编译库依赖和自动化构建矩阵。
4. **工业 SCADA 专属资产命名**：构建过程支持 `prog_name=mindscada` 参数，编译资产与静态站点模板以 `mindscada.web.*` 和 `mindscada.html` 专属前缀发布。

---

## 二、目录结构

```text
mindscada-engine-ext/
├── modules/              # 外置 C++ 模块源码目录 (通过 custom_modules 挂载编译)
│   ├── varmanager/       # 实时变量与测点管理模块
│   ├── mqttmanager/      # 工业级 MQTT 客户端与遥测通信模块
│   ├── kvmanager/        # 工业时序/KV缓存 (Web端自动启用轻量序列化存储回退)
│   ├── qtwindow/         # 原生 Qt 多窗口嵌入模块 (Web 端自动裁减)
│   └── webview/          # 原生 WebView2/CEF 浏览器嵌入模块 (Web 端自动裁减)
├── patches/              # Godot 引擎源码补丁集
│   └── 4.7.2/            # 针对 Godot 4.7.2 版本的原子 patch 文件
├── deps/                 # 多平台第三方预编译依赖与头文件骨架
│   ├── windows_x64/      # Windows 64位平台第三方库与包含头文件
│   ├── linux_arm32/      # Linux ARM32 (armv7hf) 嵌入式平台第三方库
│   └── linux_arm64/      # Linux ARM64 (aarch64) 平台第三方库
├── scripts/              # 自动化构建、模块同步、依赖拉取及补丁应用脚本
│   ├── sync_upstream.py  # 引擎上游同步与补丁应用驱动
│   ├── build.py          # 跨平台统一编译驱动器 (支持 SCons Cache 与多目标矩阵)
│   └── package.py        # 产物收集、发布打包与 SHA256 校验清单生成器
├── build_all.bat         # Windows 一键编译与打包脚本 (支持 Windows / Web / All)
├── build_all.sh          # Linux / Bash / WSL 一键编译与打包脚本
├── .github/              # GitHub Actions CI/CD 工作流配置
│   └── workflows/        # 多架构编译、测试与发布自动化流水线 (build-matrix.yml)
├── dist/                 # 最终生成的安装包、导出模板 (.tpz) 及校验和清单
└── scratch/              # 本地测试脚本、临时构建日志 (已在 .gitignore 中忽略)
```

---

## 三、一键本地编译指南 (One-Click Build)

工程根目录下提供了 `build_all.bat`（Windows）与 `build_all.sh`（Linux / WSL / Git Bash），支持一键完成指定平台的 **多目标编译** 与 **产物分发打包**。

### 1. 编译 Web 平台版本 (WASM32 Threads)
自动编译 Web 的 `template_debug` 与 `template_release` 双版本模板，并归档生成导出模板包及独立静态站点预览包：
```cmd
# Windows CMD / PowerShell
build_all.bat --web

# Linux / WSL / Git Bash
./build_all.sh --web
```
- **生成产物**：
  - `dist/MindSCADA_v4.7.2_export_templates_web.tpz`（包含 `web_debug.zip` 与 `web_release.zip`）
  - `dist/MindSCADA_v4.7.2_Web_bundle.zip`（包含 `mindscada.html`, `mindscada.js`, `mindscada.wasm` 等即开即用静态站点文件）

### 2. 编译 Windows 平台版本 (x86_64)
编译 Windows Editor Release、Template Debug 及 Template Release 三大目标，并自动打包分发包：
```cmd
# Windows CMD / PowerShell (未加参数时默认构建 Windows 目标)
build_all.bat
# 或显式指定
build_all.bat --windows

# Linux / WSL / Git Bash
./build_all.sh --windows
```

### 3. 一键编译所有支持的平台 (Windows + Web)
依次串行编译 Windows 及 Web 平台全目标，并生成完整的发布包和全局 SHA256 校验和清单：
```cmd
build_all.bat --all
# 或
./build_all.sh --all
```

### 4. 常用高级编译选项 (参数自动透传)
`build_all` 脚本支持透传通用编译控制参数：
```cmd
# 演练模式 (Dry-Run)：仅打印规划与命令，不进行实际 SCons 编译
build_all.bat --web --dry-run

# 指定并发核心数 (如 8 线程并行)：
build_all.bat --web -j 8

# 禁用 SCons 编译缓存：
build_all.bat --web --no-cache

# 查看命令行帮助：
build_all.bat --help
```

---

## 四、底层 Python 驱动脚本使用详解

如需细粒度控制单个构建环节，可直接调用 `scripts/` 目录下的核心工具链：

### 1. 跨平台编译驱动器 (`scripts/build.py`)

```bash
# 1. 编译 Web 双版本 (Debug + Release)：
python scripts/build.py --platform web --arch wasm32 --dist

# 2. 仅编译 Web 单个目标：
python scripts/build.py --platform web --arch wasm32 --target template_debug
python scripts/build.py --platform web --arch wasm32 --target template_release

# 3. 编译 Windows 完整分发目标 (Editor + Templates)：
python scripts/build.py --platform windows --arch x86_64 --dist

# 4. 交叉编译 Linux ARM32 / ARM64 模板：
python scripts/build.py --platform linux --arch arm32 --target template_release
python scripts/build.py --platform linux --arch arm64 --target template_release

# 5. 自定义 SCons 缓存路径与上限 (默认 20 GiB)：
python scripts/build.py --platform web --cache-path .cache/scons --cache-limit 30
```

### 2. 产物收集与发布打包器 (`scripts/package.py`)

```bash
# 打包 Web 平台模板与独立站点包：
python scripts/package.py --platform web --version 4.7.2

# 打包 Windows 平台编辑器及模板：
python scripts/package.py --platform windows --version 4.7.2

# 扫描并打包当前目录所有已编译平台的产物并输出 checksums.sha256：
python scripts/package.py --platform all --version 4.7.2
```

---

## 五、Web WASM 本地多线程调试服务器

由于 Web 导出版本开启了多线程（`threads=yes` / `SharedArrayBuffer`），浏览器安全策略要求必须在带有 `COOP` 和 `COEP` 响应头的环境下加载。工程内置了专用的本地静态调试服务器：

```bash
# 启动本地多线程 Web 服务器 (监听 8090 端口，映射 Web 导出产物目录)：
python scratch/dev_server.py --port 8090 --dir ../godot.4.7.2/bin/.web_zip
```

浏览器访问：
- **Web 应用主页**：`http://localhost:8090/mindscada.html`
- **离线就绪主页**：`http://localhost:8090/mindscada.offline.html`

响应头已自动注入：
- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`

---

## 六、CI/CD 自动化流水线 (GitHub Actions)

工程通过 `.github/workflows/build-matrix.yml` 实现了全自动多架构云端编译与发布：

| 矩阵 Job | 构建环境 | 架构 | 产出物 |
| :--- | :--- | :--- | :--- |
| `windows-x64` | `windows-latest` | x86_64 | `MindStudio_v*.zip` (编辑器), `MindSCADA_v*_export_templates_windows.tpz` |
| `web-wasm32` | `ubuntu-latest` | wasm32 | `MindSCADA_v*_export_templates_web.tpz` (Debug/Release), `MindSCADA_v*_Web_bundle.zip` |
| `linux-arm32` | `ubuntu-latest` | arm32 | `MindSCADA_v*_export_templates_pi32.tpz` |
| `linux-arm64` | `ubuntu-latest` | arm64 | `MindSCADA_v*_export_templates_pi64.tpz` |

- **代码推送触发**：推送到 `master`/`main` 分支时自动触发全矩阵编译并生成 CI 构建制品。
- **Release 发布触发**：推送版本 Tag（如 `git tag v4.7.2-20260923 && git push origin --tags`）时，流水线各节点自动将构建制品与 `checksums.sha256` 发布到 GitHub Releases。
