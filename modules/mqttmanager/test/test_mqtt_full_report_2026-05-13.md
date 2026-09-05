# MQTT Full Read/Write Test Report

**Date:** 2026-05-13 14:40:02
**Broker:** 192.167.200.1:1883
**Local ClientID:** perf_test_gd
**Remote ClientID:** svc_lxy4_winvm_001
**Variables:** 58 (sourced from test_write_sim_report_2026-05-08.md)
**READ_ITERS:** 3  |  **WRITE_ITERS:** 3

## Variable Inventory

| # | Variable | Type | Range |
|---|----------|------|-------|
| 1 | virtual.main.M131 | BOOL | [false, true] |
| 2 | virtual.main.D4500_z4 | INT | [1, 4] |
| 3 | virtual.main.debug4 | INT | [0, 4294967295] |
| 4 | virtual.main.D2503 | INT | [-32768, 32767] |
| 5 | virtual.main.current.maxr | FLOAT | [FLT_MIN, FLT_MAX] |
| 6 | virtual.main.current.maxspeed | FLOAT | [FLT_MIN, FLT_MAX] |
| 7 | virtual.main.current.maxxg | FLOAT | [FLT_MIN, FLT_MAX] |
| 8 | virtual.main.current.maxzno | INT | [1, 10] |
| 9 | virtual.main.D4620_25000 | INT | [100, 65000] |
| 10 | virtual.main.LockSecs | INT | [0, 4294967295] |
| 11 | virtual.main.D4224 | INT | [-32768, 32767] |
| 12 | virtual.main.D5090 | INT | [-32768, 32767] |
| 13 | virtual.main.M130_ | BOOL | [false, true] |
| 14 | virtual.main.D4500_general | INT | [1, 20] |
| 15 | virtual.main.debug3 | INT | [0, 4294967295] |
| 16 | virtual.main.D200 | INT | [-32768, 32767] |
| 17 | virtual.main.zzlock | INT | [0, 1] |
| 18 | virtual.main.M190 | BOOL | [false, true] |
| 19 | virtual.main.D4620 | INT | [100, 20000] |
| 20 | virtual.main.Lockmin | INT | [0, 4294967295] |
| 21 | virtual.main.D4190 | FLOAT | [1, 2.7] |
| 22 | virtual.main.D500 | INT | [1, 3] |
| 23 | virtual.main.D4650 | INT | [-2147483648, 2147483647] |
| 24 | virtual.main.M130 | BOOL | [false, true] |
| 25 | virtual.main.D4500 | INT | [1, 10] |
| 26 | virtual.main.debug2 | INT | [0, 4294967295] |
| 27 | virtual.main.D136 | FLOAT | [FLT_MIN, FLT_MAX] |
| 28 | virtual.main.toggle1s | INT | [-32768, 32767] |
| 29 | virtual.main.M137 | BOOL | [false, true] |
| 30 | virtual.main.D4606 | INT | [-2147483648, 2147483647] |
| 31 | virtual.main.debugMode | INT | [0, 4294967295] |
| 32 | virtual.main.D4130 | INT | [0, 1] |
| 33 | virtual.main.D4851 | INT | [-32768, 32767] |
| 34 | virtual.main.D4632 | INT | [-2147483648, 2147483647] |
| 35 | virtual.main.M121 | BOOL | [false, true] |
| 36 | virtual.main.D4252 | FLOAT | [FLT_MIN, FLT_MAX] |
| 37 | virtual.main.debug | INT | [0, 4294967295] |
| 38 | virtual.main.D1040 | INT | [-32768, 32767] |
| 39 | virtual.main.runtoggle1s | INT | [-32768, 32767] |
| 40 | virtual.main.M135 | BOOL | [false, true] |
| 41 | virtual.main.D4602 | INT | [-2147483648, 2147483647] |
| 42 | virtual.main.debug6 | INT | [0, 4294967295] |
| 43 | virtual.main.D30 | INT | [-32768, 32767] |
| 44 | virtual.main.D4850 | INT | [-32768, 32767] |
| 45 | virtual.main.D4620_general | INT | [100, 65000] |
| 46 | virtual.main.M120_ | BOOL | [false, true] |
| 47 | virtual.main.D4250 | FLOAT | [FLT_MIN, FLT_MAX] |
| 48 | virtual.main.D9998 | INT | [-2147483648, 2147483647] |
| 49 | virtual.main.D1020 | INT | [-32768, 32767] |
| 50 | virtual.main.M132 | BOOL | [false, true] |
| 51 | virtual.main.D4500_z9 | INT | [1, 9] |
| 52 | virtual.main.debug5 | INT | [0, 4294967295] |
| 53 | virtual.main.D2504 | INT | [-32768, 32767] |
| 54 | virtual.main.D4620_8000 | INT | [100, 16000] |
| 55 | virtual.main.M120 | BOOL | [false, true] |
| 56 | virtual.main.D4226 | INT | [-32768, 32767] |
| 57 | virtual.main.D5508 | FLOAT | [100, 20000] |
| 58 | virtual.main.D10000 | INT | [-2147483648, 2147483647] |

## Phase 1: Read Test Results

**Summary:** Total=58  OK=58  ERR=0  |  avg latency=5015.8 ms

| # | Variable | Type | avg_ms | Latest Value | Status |
|---|----------|------|--------|--------------|--------|
| 1 | virtual.main.M131 | BOOL | 5752.3 | 0 | OK |
| 2 | virtual.main.D4500_z4 | INT | 5004.0 | 0 | OK |
| 3 | virtual.main.debug4 | INT | 5002.7 | 0 | OK |
| 4 | virtual.main.D2503 | INT | 5002.3 | 0 | OK |
| 5 | virtual.main.current.maxr | FLOAT | 5002.7 | 0 | OK |
| 6 | virtual.main.current.maxspeed | FLOAT | 5004.3 | 0 | OK |
| 7 | virtual.main.current.maxxg | FLOAT | 5003.7 | 0 | OK |
| 8 | virtual.main.current.maxzno | INT | 5005.3 | 0 | OK |
| 9 | virtual.main.D4620_25000 | INT | 5010.3 | 0 | OK |
| 10 | virtual.main.LockSecs | INT | 5001.3 | 0 | OK |
| 11 | virtual.main.D4224 | INT | 5005.7 | 0 | OK |
| 12 | virtual.main.D5090 | INT | 5002.3 | 0 | OK |
| 13 | virtual.main.M130_ | BOOL | 5010.0 | 0 | OK |
| 14 | virtual.main.D4500_general | INT | 5002.0 | 0 | OK |
| 15 | virtual.main.debug3 | INT | 5001.3 | 0 | OK |
| 16 | virtual.main.D200 | INT | 5001.3 | 0 | OK |
| 17 | virtual.main.zzlock | INT | 5002.0 | 0 | OK |
| 18 | virtual.main.M190 | BOOL | 5001.0 | 0 | OK |
| 19 | virtual.main.D4620 | INT | 5002.0 | 0 | OK |
| 20 | virtual.main.Lockmin | INT | 5002.7 | 0 | OK |
| 21 | virtual.main.D4190 | FLOAT | 5001.0 | 0 | OK |
| 22 | virtual.main.D500 | INT | 5001.7 | 0 | OK |
| 23 | virtual.main.D4650 | INT | 5002.0 | 0 | OK |
| 24 | virtual.main.M130 | BOOL | 5001.3 | 0 | OK |
| 25 | virtual.main.D4500 | INT | 5001.7 | 0 | OK |
| 26 | virtual.main.debug2 | INT | 5005.7 | 0 | OK |
| 27 | virtual.main.D136 | FLOAT | 5001.7 | 0 | OK |
| 28 | virtual.main.toggle1s | INT | 5002.3 | 0 | OK |
| 29 | virtual.main.M137 | BOOL | 5002.0 | 0 | OK |
| 30 | virtual.main.D4606 | INT | 5001.3 | 0 | OK |
| 31 | virtual.main.debugMode | INT | 5001.3 | 0 | OK |
| 32 | virtual.main.D4130 | INT | 5004.0 | 0 | OK |
| 33 | virtual.main.D4851 | INT | 5002.7 | 0 | OK |
| 34 | virtual.main.D4632 | INT | 5002.7 | 0 | OK |
| 35 | virtual.main.M121 | BOOL | 5002.7 | 0 | OK |
| 36 | virtual.main.D4252 | FLOAT | 5001.3 | 0 | OK |
| 37 | virtual.main.debug | INT | 5003.3 | 0 | OK |
| 38 | virtual.main.D1040 | INT | 5003.3 | 0 | OK |
| 39 | virtual.main.runtoggle1s | INT | 5001.7 | 0 | OK |
| 40 | virtual.main.M135 | BOOL | 5004.0 | 0 | OK |
| 41 | virtual.main.D4602 | INT | 5001.7 | 0 | OK |
| 42 | virtual.main.debug6 | INT | 5001.3 | 0 | OK |
| 43 | virtual.main.D30 | INT | 5001.0 | 0 | OK |
| 44 | virtual.main.D4850 | INT | 5004.3 | 0 | OK |
| 45 | virtual.main.D4620_general | INT | 5001.0 | 0 | OK |
| 46 | virtual.main.M120_ | BOOL | 5010.0 | 0 | OK |
| 47 | virtual.main.D4250 | FLOAT | 5002.3 | 0 | OK |
| 48 | virtual.main.D9998 | INT | 5003.7 | 0 | OK |
| 49 | virtual.main.D1020 | INT | 5001.7 | 0 | OK |
| 50 | virtual.main.M132 | BOOL | 5003.7 | 0 | OK |
| 51 | virtual.main.D4500_z9 | INT | 5003.0 | 0 | OK |
| 52 | virtual.main.debug5 | INT | 5002.7 | 0 | OK |
| 53 | virtual.main.D2504 | INT | 5001.0 | 0 | OK |
| 54 | virtual.main.D4620_8000 | INT | 5004.0 | 0 | OK |
| 55 | virtual.main.M120 | BOOL | 5002.7 | 0 | OK |
| 56 | virtual.main.D4226 | INT | 5001.7 | 0 | OK |
| 57 | virtual.main.D5508 | FLOAT | 5001.3 | 0 | OK |
| 58 | virtual.main.D10000 | INT | 5002.0 | 0 | OK |

## Phase 2: Write Simulation Results

**Summary:** Total=58  OK=28  FAIL=30  ERR=0  |  avg latency=2623.9 ms

| # | Variable | Type | Range | Last Written Value | avg_ms | Result | Status |
|---|----------|------|-------|--------------------|--------|--------|--------|
| 1 | virtual.main.M131 | BOOL | [false, true] | 0 | 5001.7 | 0 | FAIL |
| 2 | virtual.main.D4500_z4 | INT | [1, 4] | 2 | 5001.0 | 0 | FAIL |
| 3 | virtual.main.debug4 | INT | [0, 4294967295] | 3176441220 | 5001.7 | 0 | FAIL |
| 4 | virtual.main.D2503 | INT | [-32768, 32767] | 11159 | 5001.7 | 0 | FAIL |
| 5 | virtual.main.current.maxr | FLOAT | [FLT_MIN, FLT_MAX] | 93.2380679012024 | 5001.0 | 0 | FAIL |
| 6 | virtual.main.current.maxspeed | FLOAT | [FLT_MIN, FLT_MAX] | -327.81138510... | 5005.0 | 0 | FAIL |
| 7 | virtual.main.current.maxxg | FLOAT | [FLT_MIN, FLT_MAX] | -304.27218483... | 5006.3 | 0 | FAIL |
| 8 | virtual.main.current.maxzno | INT | [1, 10] | 7 | 5000.7 | 0 | FAIL |
| 9 | virtual.main.D4620_25000 | INT | [100, 65000] | 28854 | 5002.3 | 0 | FAIL |
| 10 | virtual.main.LockSecs | INT | [0, 4294967295] | 2018685185 | 5001.7 | 0 | FAIL |
| 11 | virtual.main.D4224 | INT | [-32768, 32767] | -6022 | 5001.7 | 0 | FAIL |
| 12 | virtual.main.D5090 | INT | [-32768, 32767] | -30829 | 5021.0 | 0 | FAIL |
| 13 | virtual.main.M130_ | BOOL | [false, true] | 0 | 5007.7 | 0 | FAIL |
| 14 | virtual.main.D4500_general | INT | [1, 20] | 9 | 5001.7 | 0 | FAIL |
| 15 | virtual.main.debug3 | INT | [0, 4294967295] | 1963576811 | 5001.3 | 0 | FAIL |
| 16 | virtual.main.D200 | INT | [-32768, 32767] | 12299 | 5001.7 | 0 | FAIL |
| 17 | virtual.main.zzlock | INT | [0, 1] | 0 | 5001.3 | 0 | FAIL |
| 18 | virtual.main.M190 | BOOL | [false, true] | 0 | 5001.7 | 0 | FAIL |
| 19 | virtual.main.D4620 | INT | [100, 20000] | 3679 | 5001.7 | 0 | FAIL |
| 20 | virtual.main.Lockmin | INT | [0, 4294967295] | 508820947 | 5001.3 | 0 | FAIL |
| 21 | virtual.main.D4190 | FLOAT | [1, 2.7] | 1.41177101930424 | 5003.7 | 0 | FAIL |
| 22 | virtual.main.D500 | INT | [1, 3] | 1 | 5001.3 | 0 | FAIL |
| 23 | virtual.main.D4650 | INT | [-2147483648, 2147483647] | -2147483648 | 5002.3 | 0 | FAIL |
| 24 | virtual.main.M130 | BOOL | [false, true] | 0 | 5005.0 | 0 | FAIL |
| 25 | virtual.main.D4500 | INT | [1, 10] | 4 | 5002.0 | 0 | FAIL |
| 26 | virtual.main.debug2 | INT | [0, 4294967295] | 2549553072 | 5001.3 | 0 | FAIL |
| 27 | virtual.main.D136 | FLOAT | [FLT_MIN, FLT_MAX] | -726.74827216... | 5002.7 | 0 | FAIL |
| 28 | virtual.main.toggle1s | INT | [-32768, 32767] | -993 | 5001.0 | 0 | FAIL |
| 29 | virtual.main.M137 | BOOL | [false, true] | 0 | 5003.3 | 0 | FAIL |
| 30 | virtual.main.D4606 | INT | [-2147483648, 2147483647] | -2147483648 | 5001.7 | 0 | FAIL |
| 31 | virtual.main.debugMode | INT | [0, 4294967295] | 238468883 | 74.7 | 1 | OK |
| 32 | virtual.main.D4130 | INT | [0, 1] | 0 | 69.3 | 1 | OK |
| 33 | virtual.main.D4851 | INT | [-32768, 32767] | -16074 | 103.0 | 1 | OK |
| 34 | virtual.main.D4632 | INT | [-2147483648, 2147483647] | -2147483647 | 82.3 | 1 | OK |
| 35 | virtual.main.M121 | BOOL | [false, true] | 0 | 70.0 | 1 | OK |
| 36 | virtual.main.D4252 | FLOAT | [FLT_MIN, FLT_MAX] | -179.65901178... | 76.3 | 1 | OK |
| 37 | virtual.main.debug | INT | [0, 4294967295] | 1697559876 | 73.7 | 1 | OK |
| 38 | virtual.main.D1040 | INT | [-32768, 32767] | -29637 | 83.7 | 1 | OK |
| 39 | virtual.main.runtoggle1s | INT | [-32768, 32767] | -8423 | 56.0 | 1 | OK |
| 40 | virtual.main.M135 | BOOL | [false, true] | 0 | 65.3 | 1 | OK |
| 41 | virtual.main.D4602 | INT | [-2147483648, 2147483647] | -2147483648 | 72.7 | 1 | OK |
| 42 | virtual.main.debug6 | INT | [0, 4294967295] | 2231570066 | 59.3 | 1 | OK |
| 43 | virtual.main.D30 | INT | [-32768, 32767] | 25771 | 61.3 | 1 | OK |
| 44 | virtual.main.D4850 | INT | [-32768, 32767] | -1421 | 59.0 | 1 | OK |
| 45 | virtual.main.D4620_general | INT | [100, 65000] | 5733 | 60.7 | 1 | OK |
| 46 | virtual.main.M120_ | BOOL | [false, true] | 0 | 75.0 | 1 | OK |
| 47 | virtual.main.D4250 | FLOAT | [FLT_MIN, FLT_MAX] | 925.547932114654 | 83.3 | 1 | OK |
| 48 | virtual.main.D9998 | INT | [-2147483648, 2147483647] | -2147483648 | 74.3 | 1 | OK |
| 49 | virtual.main.D1020 | INT | [-32768, 32767] | 18612 | 81.0 | 1 | OK |
| 50 | virtual.main.M132 | BOOL | [false, true] | 0 | 80.0 | 1 | OK |
| 51 | virtual.main.D4500_z9 | INT | [1, 9] | 2 | 71.0 | 1 | OK |
| 52 | virtual.main.debug5 | INT | [0, 4294967295] | 1036757689 | 97.3 | 1 | OK |
| 53 | virtual.main.D2504 | INT | [-32768, 32767] | 31013 | 76.3 | 1 | OK |
| 54 | virtual.main.D4620_8000 | INT | [100, 16000] | 10016 | 69.3 | 1 | OK |
| 55 | virtual.main.M120 | BOOL | [false, true] | 0 | 77.3 | 1 | OK |
| 56 | virtual.main.D4226 | INT | [-32768, 32767] | -7516 | 83.7 | 1 | OK |
| 57 | virtual.main.D5508 | FLOAT | [100, 20000] | 19893.0378084747 | 77.3 | 1 | OK |
| 58 | virtual.main.D10000 | INT | [-2147483648, 2147483647] | -2147483647 | 84.3 | 1 | OK |

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total variables tested | 58 |
| Read OK | 58 / 58 |
| Read ERR | 0 / 58 |
| Write OK (result=1) | 28 / 58 |
| Write FAIL (result=0) | 30 / 58 |
| Write ERR (no response) | 0 / 58 |
| Avg read latency | 5015.8 ms |
| Avg write latency | 2623.9 ms |

## Conclusions

- **读测试覆盖率高**: 100% 变量响应正常 (58/58)
- **写入仿真部分成功**: 48% 变量写入成功 (result=1)，52% 失败 (result=0)
- **读延迟过高**: avg=5016 ms > 2000 ms — 建议检查 broker 网络
- **写延迟过高**: avg=2624 ms > 2000 ms

