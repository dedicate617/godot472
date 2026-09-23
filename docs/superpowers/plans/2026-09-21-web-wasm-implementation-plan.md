# Web WASM Engine Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the Phase 1 Web WASM export architecture for mindscada-engine-ext, featuring UI stripping, proxy networking, and IndexedDB-backed Godot KV storage.

**Architecture:** We use Emscripten to compile Godot and C++ modules to WebAssembly. The `qtwindow` and `webview` modules are stripped via feature flags. The `kvmanager` falls back to `GodotFileKVCache` using in-memory HashMaps and binary `FileAccess` flushes. Networking relies on Emscripten's POSIX proxy to a WebSockify gateway.

**Tech Stack:** C++, Godot 4.7.2, Python (Build Script), Emscripten (emsdk).

## Global Constraints

- Platform target must be `web`, arch must be `wasm32`.
- Avoid touching read-only Godot upstream files (modifications go in `mindscada-engine-ext` `scripts/` and `modules/`).
- Web client must run with `threads=yes` (SharedArrayBuffer enabled).

---

### Task 1: Build System Feature Flags (UI Stripping)

**Files:**
- Modify: `scripts/build.py`
- Modify: `modules/qtwindow/SCsub`
- Modify: `modules/webview/SCsub`

**Interfaces:**
- Produces: Build script capable of accepting `--platform=web` and correctly bypassing UI modules.

- [ ] **Step 1: Update `scripts/build.py` module discovery**

In `scripts/build.py`, intercept the custom modules list generation.
Modify `scripts/build.py` to filter out unsupported modules for Web.

```python
# Assuming there is a function that collects custom modules
def get_custom_modules(platform):
    modules = ["kvmanager", "mqttmanager", "varmanager"]
    if platform != "web":
        modules.extend(["qtwindow", "webview"])
    return modules
```
*(Agent Note: Adapt this logic to the actual structure of `scripts/build.py`'s module loading loop)*

- [ ] **Step 2: Add early return to `qtwindow/SCsub`**

Modify `modules/qtwindow/SCsub` to early exit if platform is web:
```python
Import('env')
if env["platform"] == "web":
    Return()
```

- [ ] **Step 3: Add early return to `webview/SCsub`**

Modify `modules/webview/SCsub` to early exit if platform is web:
```python
Import('env')
if env["platform"] == "web":
    Return()
```

- [ ] **Step 4: Verify build configuration**

Run: `python scripts/build.py --platform=web --arch=wasm32 --dry-run`
Expected: Build output logs should NOT mention compiling `qtwindow` or `webview`.

- [ ] **Step 5: Commit changes**

```bash
git add scripts/build.py modules/qtwindow/SCsub modules/webview/SCsub
git commit -m "build(web): strip qtwindow and webview modules for wasm target"
```

---

### Task 2: Storage Layer (kvmanager) Web Fallback

**Files:**
- Modify: `modules/kvmanager/SCsub`
- Modify: `modules/kvmanager/kvmanager.h`
- Modify: `modules/kvmanager/kvmanager.cpp`

**Interfaces:**
- Produces: A WebAssembly-compatible KV storage using Godot's `FileAccess`, bypassing MMKV.

- [ ] **Step 1: Modify `kvmanager/SCsub` to exclude MMKV on Web**

```python
Import('env')
if env["platform"] == "web":
    # Do not append MMKV include paths or libraries
    pass
else:
    # Existing MMKV setup code...
```
*(Agent Note: Adapt to the existing SCsub file)*

- [ ] **Step 2: Define `GodotFileKVCache` in `kvmanager.h`**

```cpp
#ifdef JAVASCRIPT_ENABLED
#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

class KVManager {
private:
    HashMap<String, Variant> memory_cache;
    bool is_dirty = false;
    uint64_t last_flush_time = 0;
    String save_path = "user://mindscada_kv.bin";

    void flush_to_disk();
    void load_from_disk();
public:
    KVManager();
    void set(const String &key, const Variant &val);
    Variant get(const String &key);
    void process(float delta); // To be called from Godot _process or timer
};
#else
// Existing MMKV class declaration...
#endif
```

- [ ] **Step 3: Implement `GodotFileKVCache` in `kvmanager.cpp`**

```cpp
#ifdef JAVASCRIPT_ENABLED

KVManager::KVManager() {
    load_from_disk();
}

void KVManager::set(const String &key, const Variant &val) {
    memory_cache[key] = val;
    is_dirty = true;
}

Variant KVManager::get(const String &key) {
    if (memory_cache.has(key)) {
        return memory_cache[key];
    }
    return Variant();
}

void KVManager::flush_to_disk() {
    if (!is_dirty) return;
    Ref<FileAccess> f = FileAccess::open(save_path, FileAccess::WRITE);
    if (f.is_valid()) {
        f->store_32(memory_cache.size());
        for (const KeyValue<String, Variant> &E : memory_cache) {
            f->store_pascal_string(E.key);
            f->store_var(E.value);
        }
        f->close(); // Triggers Emscripten IDBFS sync
        is_dirty = false;
        last_flush_time = OS::get_singleton()->get_ticks_msec();
    }
}

void KVManager::load_from_disk() {
    Ref<FileAccess> f = FileAccess::open(save_path, FileAccess::READ);
    if (f.is_valid()) {
        uint32_t size = f->get_32();
        for (uint32_t i = 0; i < size; i++) {
            String k = f->get_pascal_string();
            Variant v = f->get_var();
            memory_cache[k] = v;
        }
        f->close();
    }
}

void KVManager::process(float delta) {
    if (is_dirty && OS::get_singleton()->get_ticks_msec() - last_flush_time > 500) {
        flush_to_disk();
    }
}
#endif
```

- [ ] **Step 4: Commit changes**

```bash
git add modules/kvmanager/
git commit -m "feat(web): implement GodotFileKVCache fallback for wasm without mmkv"
```

---

### Task 3: Emscripten Link Flags & WebServer Sandbox Setup

**Files:**
- Modify: `scripts/build.py`
- Create: `scratch/dev_server.py`

**Interfaces:**
- Produces: Correct SCons linker flags for Emscripten networking and threading, and a local dev server with COOP/COEP headers.

- [ ] **Step 1: Add Emscripten Link Flags to `build.py`**

In `scripts/build.py`, where link flags are configured for platform `web`:
```python
if platform == "web":
    # Enable WebSockets proxy emulation and Threading support
    cmd.append('linkflags="-s WEBSOCKET_URL=wss://localhost:8080/ -s WEBSOCKIFY=1 -s PTHREADS=1"')
    cmd.append("threads=yes")
```
*(Agent Note: WEBSOCKET_URL should be parameterized or configurable, hardcoded here for testing setup)*

- [ ] **Step 2: Create local test server with COOP/COEP headers**

Create `scratch/dev_server.py`:
```python
import http.server
import socketserver

PORT = 8000
class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()

with socketserver.TCPServer(("", PORT), Handler) as httpd:
    print(f"Serving with COOP/COEP on port {PORT}")
    httpd.serve_forever()
```

- [ ] **Step 3: Commit changes**

```bash
git add scripts/build.py scratch/dev_server.py
git commit -m "build(web): add emscripten threading flags and local dev server"
```
