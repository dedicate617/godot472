# Web WASM Architecture Design (mindscada-engine-ext)

## 1. Overview
This specification outlines the architecture for compiling and deploying the Godot 4.7.2 engine with `mindscada-engine-ext` custom modules to the Web (WASM/HTML5). The design prioritizes rapid delivery by leveraging Emscripten's networking proxies, while ensuring data persistence through Godot's native APIs.

## 2. Architecture by Layer

### 2.1 UI & View Layer
*   **Constraint**: WebAssembly cannot spawn native OS windows or embed Chromium embedded frameworks (CEF).
*   **Design**: 
    *   Introduce feature flags in `scripts/build.py` and `SCsub`.
    *   When `platform == "web"`, completely skip the compilation of `qtwindow` and `webview` modules.
    *   Rely purely on Godot native UI nodes.

### 2.2 Storage Layer (`kvmanager`)
*   **Constraint**: MMKV relies heavily on POSIX shared memory and native `mmap`, which are incompatible with Emscripten's virtual filesystem (IDBFS) limitations for inter-process communication.
*   **Design (Engine-Native Fallback)**:
    *   Use `#ifdef JAVASCRIPT_ENABLED` to conditionally branch the implementation.
    *   **Desktop**: Retain native `MMKV`.
    *   **Web**: Implement `GodotFileKVCache`.
        *   **Memory Cache**: Keep a `HashMap<String, Variant>` in memory for 0ms latency on `get()`/`set()`.
        *   **Debounced Binary Persistence**: Flag writes as dirty. Use a timer/process hook (e.g., every 500ms) to dump the cache to `user://mindscada_kv.bin` using Godot's binary `FileAccess::store_var()`, bypassing slow text/JSON serialization.
        *   Emscripten will automatically proxy the `user://` writes to the browser's IndexedDB.

### 2.3 Network Layer (MQTT & OPC-UA)
*   **Constraint**: Browsers block raw TCP/UDP sockets (port 1883, 4840). 
*   **Design (Phase 1: Transparent Proxy Gateway)**:
    *   **C/C++ Clients**: No changes to `paho-mqtt` or `open62541` code. They will continue to open standard TCP sockets.
    *   **Emscripten Routing**: Compile Godot WASM relying on Emscripten's POSIX socket emulation. It intercepts C++ TCP calls and wraps them in WebSockets.
    *   **Infrastructure Requirement**: Deploy a proxy gateway (e.g., `websockify` or Nginx stream proxy) alongside the web server. It receives the WS connections, unwraps them, and proxies standard TCP to the internal OPC-UA/MQTT servers.
    *   *(Future Phase 2: Native WS compilation into the C++ clients to remove the proxy dependency).*

### 2.4 Concurrency & Deployment constraints
*   **Constraint**: Network modules rely on background threads to prevent blocking the Godot main loop.
*   **Design**:
    *   Compile Godot with `threads=yes`.
    *   **HTTP Server Requirement**: The web server serving the `.html` and `.wasm` files MUST inject the following headers, or the browser will block multi-threading:
        ```http
        Cross-Origin-Opener-Policy: same-origin
        Cross-Origin-Embedder-Policy: require-corp
        ```

### 2.5 Build System & Toolchain
*   Update `scripts/build.py` to handle `platform="web"` and `arch="wasm32"`.
*   Create a new dependency directory: `deps/web_wasm32/lib`.
*   Cross-compile the core C dependencies (`open62541`, `paho-mqtt`, `mbedtls`, `spdlog`) using `emcc`/`em++` into the new deps folder.

## 3. Implementation Plan Scope
This spec covers the initial Phase 1 target (Transparent Proxy). Subsequent migration to Phase 2 (Native WS) will be planned separately once the core WASM infrastructure is stable.
