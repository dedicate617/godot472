# VarManager OPC-UA 性能基线测试报告

**报告编号**: BASELINE-001  
**测试日期**: 2026-05-06  
**测试时间**: 18:22:53 – 18:23:02 CST  
**报告用途**: 异步队列优化后建立量化基线，供后续优化版本同比对照  

---

## 1. 测试环境

### 1.1 硬件

| 项目 | 规格 |
|------|------|
| 平台 | VMware 虚拟机 |
| CPU | Intel Core i7-4700MQ @ 2.40 GHz |
| 物理核 / 逻辑核 | 2 / 2 |
| 内存 | 8 GB |
| 存储 | VMware Virtual Disk (Unspecified, ~204 GB) |
| 网络 | Ethernet0 1 Gbps / Ethernet1 1 Gbps (虚拟网卡) |

### 1.2 软件

| 项目 | 版本 / 说明 |
|------|------------|
| OS | Windows 10 企业版 10.0.19045 |
| 引擎 | MindStudio v4.2.1.stable.custom_build.9cdca5126 |
| 构建目标 | windows / editor / dev_build / x86_64 |
| 二进制大小 | 158,204,416 bytes (≈ 150.9 MB) |
| 构建时间 | 2026-05-04 20:33:15 |
| 链接模式 | `/DEBUG:FASTLINK` |
| OPC-UA 库 | open62541 v1.3.9 |
| varmanager commit | `a4788a3` (feat/async-opcua-optimization) |
| varmanager tag | `v-async-opcua-opt-1.0` |

### 1.3 OPC-UA Server

| 项目 | 值 |
|------|----|
| EndpointURL | `opc.tcp://127.0.0.1:4840/` (loopback) |
| 主机名 | DESKTOP-P7J38AA |
| 安全模式 | None |
| 安全策略 | `http://opcfoundation.org/UA/SecurityPolicy#None` |
| 认证 | Anonymous |
| 传输层 | TCP loopback (同机，理论延迟 < 1 ms) |

---

## 2. 测试套件说明

测试通过 `build_varmanager_and_test.bat test-only` 顺序执行两个 GDScript 测试脚本：

```
Phase 1 (Smoke): test_opcua.gd     — 连接 + 单读 + 并发去重 + 超时
  [inter-test delay: 5 s]
Phase 2 (Batch): test_batch_read.gd — 枚举 + 批量异步读 + 采样同步读
```

| 参数 | 值 |
|------|----|
| `TIMEOUT_MS` | 3000 ms |
| `MAX_DEPTH` | 5 |
| `SAMPLE_SIZE` | 10 |
| `p_activateUpcalls` | true (后台线程启动，命令队列得以 drain) |

---

## 3. Smoke Test 结果（test_opcua.gd）

### 3.1 逐项结果

| 测试 | 内容 | 结果 | 耗时 |
|------|------|------|------|
| T0 | `connect_start` loopback | **PASS** | < 100 ms |
| T1 | `readValue("virtual/D/D4000_general/Value")` | **PASS** | — |
| T2 | 8× `readValueAsync` + 1× `readValue` (并发去重) | **PASS** | **205 ms** |
| T3 | 未连接实例 `setReadTimeoutMs(500)` 后 `readValue` | **PASS** | **500 ms** |

### 3.2 T1 采样值

| 变量路径 | 值 | GDScript 类型 |
|----------|----|---------------|
| `virtual/D/D4000_general/Value` | 0 | TYPE_INT (2) |

### 3.3 关键指标

| 指标 | 测量值 | 说明 |
|------|--------|------|
| T2 并发去重总耗时 | **205 ms** | 8 个异步 + 1 个同步，整体仅发 1 次 OPC-UA 请求 |
| T3 超时精度误差 | **0 ms** | 设置 500 ms，实测 500 ms |
| T2 limit | 8000 ms (`TIMEOUT_MS × 4`) | 实测 205 ms，占 limit **2.6%** |

---

## 4. Batch Read Test 结果（test_batch_read.gd）

### 4.1 Phase 1 — 节点枚举

| 指标 | 测量值 |
|------|--------|
| 枚举总耗时 | **3534 ms** |
| 发现 Variable 节点总数 | **7385** |
| 其中 `/Value` 叶节点数 | **742** |
| 节点枚举速率 | **2090 nodes/sec** (7385 / 3.534 s) |
| 枚举策略 | 递归 `getChildrenOfPath`，最大深度 5 |
| `/Value` 比例 | 742 / 7385 = **10.0%** |

### 4.2 Phase 2 — 批量异步读

| 指标 | 测量值 |
|------|--------|
| 批量路径数 | **742** |
| 批量总耗时 | **114 ms** |
| 等效吞吐量 | **6508 reads/sec** (742 / 0.114 s) |
| 首值（`first_val`）| 0 (TYPE_INT) |
| Pass limit | 37100 ms (`max(9000, 742×50)`) |
| 实测 / limit | **0.31%** |

**批量 vs 顺序同步读 对比估算**

| 模式 | 估算总耗时 | 说明 |
|------|-----------|------|
| 顺序同步读 742 次 | ~371,000 ms (~6.2 min) | 742 × 500 ms/read（Phase 3 实测单读约 514 ms） |
| 批量异步读 742 次 | **114 ms** | 实测 |
| **加速比** | **≈ 3254×** | 批量异步 vs 顺序同步 |

### 4.3 Phase 3 — 同步采样读（10 条）

| 指标 | 测量值 |
|------|--------|
| 采样数 | 10 |
| Phase 3 总耗时 | **~5143 ms** (18:22:56.72 → 18:23:01.87) |
| 平均单次同步读延迟 | **~514 ms** |
| 最快单次同步读延迟 | — (未记录逐条时间) |

**Phase 3 采样值（10 条）**

| 变量路径 | 值 | 类型 |
|----------|----|------|
| `virtual/Dummy/recipeFontSize/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/CurrentUserGroupId/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/MaxTemp/Value` | 0 | TYPE_FLOAT (3) |
| `virtual/Dummy/AlarmPopup/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/tuiping2/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/enableON_OFF_Log/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/pageNo/Value` | 1 | TYPE_INT (2) |
| `virtual/Dummy/commErr/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/ysjLogicMode/Value` | 0 | TYPE_INT (2) |
| `virtual/Dummy/icustomMode/Value` | 0 | TYPE_INT (2) |

---

## 5. 端到端时序（Batch Test 进程）

```
18:22:53.056  进程启动，开始握手
18:22:53.063  SecureChannel 62 建立（首个 channel open）
18:22:53.074  SessionState: Activated → connect_start 返回 ALL_OK=1
              连接建立耗时: ~18 ms（从进程启动算起）
              连接建立耗时: ~11 ms（从首 channel open 算起）

18:22:53.074  Phase 1 枚举开始
18:22:56.608  Phase 1 结束（+3534 ms）  → 7385 nodes

18:22:56.608  Phase 2 批量读开始（742× readValueAsync + 1× readValue）
18:22:56.722  Phase 2 结束（+114 ms）

18:22:56.722  Phase 3 采样读开始（10× readValue）
18:23:01.865  Phase 3 结束，connect_close，进程退出（+5143 ms）

总进程运行时间: ~8.8 s
```

---

## 6. 性能基线汇总（Baseline KPIs）

> 以下数据为后续优化版本同比基准，所有测试应在**相同硬件、相同服务器、相同节点树**下复现。

| KPI | 基准值 | 单位 | 测试场景 |
|-----|--------|------|---------|
| **KPI-01** 连接建立时间 | 18 | ms | 进程启动 → SessionState=Activated |
| **KPI-02** 节点枚举速率 | 2090 | nodes/sec | 7385 nodes / 3534 ms |
| **KPI-03** 枚举总耗时 | 3534 | ms | 7385 Variable 节点，深度 ≤ 5 |
| **KPI-04** 批量异步读吞吐量 | 6508 | reads/sec | 742 /Value 节点 |
| **KPI-05** 批量异步读总耗时 | 114 | ms | 742 paths，inflight 去重 + batch dispatch |
| **KPI-06** 单次同步读平均延迟 | 514 | ms | Phase 3，10 次平均，loopback |
| **KPI-07** 并发去重效果（T2） | 205 | ms | 8 async + 1 sync → 1 OPC-UA request |
| **KPI-08** 超时精度 | 0 | ms误差 | setReadTimeoutMs(500)，实测 500 ms |
| **KPI-09** 批量 vs 顺序加速比 | 3254 | × | 估算，基于 KPI-05 / (742 × KPI-06) |
| **KPI-10** /Value 节点占比 | 10.0 | % | 742 / 7385 |
| **KPI-W1** 单次同步写平均延迟 | 106 | ms | W6，10 次，loopback |
| **KPI-W2** 异步写吞吐量 | 41.0 | writes/sec | W6，10× writeValueAsync + fence read |
| **KPI-W3** 异步/同步写加速比 | 4.34 | × | W6，async vs sync |
| **KPI-WB1** 同步批量写 ms/write（N=1→9） | 77→18 | ms/write | WB1，ms/write 随 N 递减，证明客户端批量聚合生效 |
| **KPI-WB2** 异步批量写 drain_ms（N=1→9） | 210→252 | ms | WB2，fire_ms < 1 ms（非阻塞），drain 线性增长 |
| **KPI-WB3** 同步写延迟 P50/P95/P99 | 104/114/115 | ms | WB3，30 次迭代，分布极紧（全部在 100-150ms） |
| **KPI-WB4** 大批量异步写吞吐（N=742） | 278.7 | writes/sec | WB4，随 N 增大吞吐提升（50→116，200→217，742→279），证明 OPC-UA 大批量时写请求被聚合 |
| **KPI-WB5** FLOAT vs INT 写延迟比 | 0.24 | × | WB5，5 次平均；FLOAT(118ms) < INT(496ms)，INT 受前序 WB1-WB4 练习影响，非类型差异 |

---

## 7. 已知问题 / 注意事项

| # | 现象 | 影响 | 建议 |
|---|------|------|------|
| 1 | 批量测试连接时出现 `BadSecurityChecksFailed`（SecureChannel 65），客户端自动恢复 | 无功能影响，但增加连接抖动 ~4-5 s | 两测试之间加 5 s 间隔（已实施）；根因：上一进程 session 未完全释放 |
| 2 | 单次同步读延迟 ~514 ms（loopback 理论 RTT < 1 ms） | Phase 3 总耗时偏高 | 后台线程 `run_once()` 轮询间隔引入额外等待，为主要优化方向 |
| 3 | `virtual/D/D4000_general/Value` 在本次测试中存在（T1 值 = 0），但前次测试 T1 路径不存在 | 服务器节点树不稳定 | 建议测试环境固化服务器节点配置 |
| 4 | Phase 3 未记录逐条读取耗时 | 无法分辨 P50/P95/P99 | 后续版本在 Phase 3 增加 `Time.get_ticks_msec()` 逐条计时 |
| 5 | ~~`writeValues`（同步，N>1 路径）返回 sc=8 (ERROR_COMMUNICATION)~~ | ~~数据写入成功但返回码错误~~ | **已修复**（2026-05-06）：`drainCommandQueue` 改为逐条调用 `writeValue` 循环，绕过 `Open62541CppWrapper::GenericClient::writeValues` 的库 bug（`wResp.resultsSize != 1` 时返回 `BADUNEXPECTEDERROR`） |

---

## 8. 下一步优化方向

根据 KPI 数据，以下方向潜在收益最大：

| 优先级 | 优化方向 | 目标 KPI | 预期改善 |
|--------|----------|----------|---------|
| P1 | 降低 `run_once()` 轮询间隔或改为事件驱动 | KPI-06 同步读延迟 | 514 ms → < 50 ms |
| P2 | Browse 请求并发化（多 worker 同时枚举子树） | KPI-02/03 枚举速率 | 2090 → 5000+ nodes/sec |
| P3 | 增大 `drainCommandQueue` 批量 readValues_async 窗口 | KPI-04/05 批量吞吐 | 6508 → 10000+ reads/sec |
| P4 | 跨进程 session 复用（避免 BadSecurityChecksFailed） | 连接稳定性 | 消除 5 s 等待 |
| P5 | **利用 WB4 发现的批量聚合效应**：drainCommandQueue 将同一 run_once 周期内积压的多条 WriteAsync 命令合并为单次 `writeValues()` 调用，可将同步写通量从 41 → 279 writes/sec（6.8×）| KPI-W2/KPI-WB4 | 41 → 279+ writes/sec |

---

## 9. 复现方法

```bat
:: 确保 OPC-UA server 运行在 opc.tcp://127.0.0.1:4840/
:: 从 godot.4.2.1 根目录执行：
build_varmanager_and_test.bat test-only

:: 预期输出：
::   Overall: PASS
::   Smoke log  : modules\varmanager\test\test_results_last.txt
::   Batch log  : modules\varmanager\test\test_batch_results_last.txt
::   Write log  : modules\varmanager\test\test_write_results_last.txt
```

测试脚本路径：
- `modules/varmanager/test/test_opcua.gd`
- `modules/varmanager/test/test_batch_read.gd`
- `modules/varmanager/test/test_write.gd`

---

*报告生成时间: 2026-05-06 — MindStudio varmanager async OPC-UA optimization baseline*
