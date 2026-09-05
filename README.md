# mindscada-engine-ext

Godot 4.7.2 工业级工控/SCADA 引擎外置模块与多平台构建编排工程。

## 项目定位

本项目为 `MindSCADA` 引擎的外置扩展与编排仓库。通过将自定义 C++ 模块、引擎定制补丁、预编译第三方依赖以及自动化构建流程从 Godot 官方核心源码中彻底解耦，实现：
1. **非侵入式扩展**：依托 SCons 的 `custom_modules` 机制加载核心业务模块。
2. **多版本适配性**：补丁以标准 patch 形式版本化维护（如 `patches/4.7.2/`），便于随引擎上游快速升级演进。
3. **跨平台 CI/CD 编排**：统一管理 Windows x64 及 Linux ARM (ARM32/ARM64) 平台的预编译库依赖和自动化构建矩阵。

---

## 目录结构与职责

```text
mindscada-engine-ext/
├── modules/              # 外置 C++ 模块源码目录 (通过 custom_modules 挂载编译)
├── patches/              # Godot 引擎源码补丁
│   └── 4.7.2/            # 针对 Godot 4.7.2 版本的定制 patch 文件
├── deps/                 # 多平台第三方预编译依赖与头文件骨架
│   ├── windows_x64/      # Windows 64位平台第三方库与包含头文件
│   ├── linux_arm32/      # Linux ARM32 (armv7hf) 嵌入式平台第三方库
│   └── linux_arm64/      # Linux ARM64 (aarch64) 平台第三方库
├── scripts/              # 自动化构建、模块同步、依赖拉取及补丁应用脚本
├── .github/              # GitHub Actions CI/CD 工作流配置
│   └── workflows/        # 多架构编译、测试与发布自动化流水线
└── scratch/              # 本地测试脚本、临时构建日志 (已在 .gitignore 中忽略)
```

---

## 规范与约定

- **解耦原则**：所有自定义功能、组件封装均应置于 `modules/` 下；若非必要修改引擎源码，尽量不使用侵入式 patch。
- **构建规范**：编译时使用 SCons 参数指定 `custom_modules="<path_to_mindscada-engine-ext>/modules"`。
- **环境隔离**：所有本地临时测试脚本、临时日志均须保存在 `scratch/` 目录下，严禁污染工程根目录。
