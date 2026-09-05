# MQTT Full Read/Write Test Report

**Date:** 2026-05-11 18:22:38
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

**Summary:** Total=58  OK=58  ERR=0  |  avg latency=67.1 ms

| # | Variable | Type | avg_ms | Latest Value | Status |
|---|----------|------|--------|--------------|--------|
| 1 | virtual.main.M131 | BOOL | 803.3 | 1:false | OK |
| 2 | virtual.main.D4500_z4 | INT | 52.3 | 2:0 | OK |
| 3 | virtual.main.debug4 | INT | 36.0 | 3:0 | OK |
| 4 | virtual.main.D2503 | INT | 32.3 | 2:0 | OK |
| 5 | virtual.main.current.maxr | FLOAT | 61.0 | 38:0 | OK |
| 6 | virtual.main.current.maxspeed | FLOAT | 160.7 | 38:0 | OK |
| 7 | virtual.main.current.maxxg | FLOAT | 47.0 | 38:0 | OK |
| 8 | virtual.main.current.maxzno | INT | 35.0 | 2:0 | OK |
| 9 | virtual.main.D4620_25000 | INT | 36.7 | 2:0 | OK |
| 10 | virtual.main.LockSecs | INT | 55.3 | 3:0 | OK |
| 11 | virtual.main.D4224 | INT | 107.7 | 2:0 | OK |
| 12 | virtual.main.D5090 | INT | 51.7 | 2:0 | OK |
| 13 | virtual.main.M130_ | BOOL | 35.3 | 1:false | OK |
| 14 | virtual.main.D4500_general | INT | 74.0 | 2:0 | OK |
| 15 | virtual.main.debug3 | INT | 30.0 | 3:0 | OK |
| 16 | virtual.main.D200 | INT | 38.7 | 2:0 | OK |
| 17 | virtual.main.zzlock | INT | 146.3 | 2:0 | OK |
| 18 | virtual.main.M190 | BOOL | 44.3 | 1:false | OK |
| 19 | virtual.main.D4620 | INT | 46.0 | 2:500 | OK |
| 20 | virtual.main.Lockmin | INT | 41.3 | 3:0 | OK |
| 21 | virtual.main.D4190 | FLOAT | 26.7 | 38:0 | OK |
| 22 | virtual.main.D500 | INT | 102.0 | 2:0 | OK |
| 23 | virtual.main.D4650 | INT | 36.0 | 2:0 | OK |
| 24 | virtual.main.M130 | BOOL | 54.0 | 1:false | OK |
| 25 | virtual.main.D4500 | INT | 30.7 | 2:0 | OK |
| 26 | virtual.main.debug2 | INT | 35.3 | 3:0 | OK |
| 27 | virtual.main.D136 | FLOAT | 13.7 | 38:0 | OK |
| 28 | virtual.main.toggle1s | INT | 50.0 | 2:0 | OK |
| 29 | virtual.main.M137 | BOOL | 29.0 | 1:false | OK |
| 30 | virtual.main.D4606 | INT | 110.3 | 2:0 | OK |
| 31 | virtual.main.debugMode | INT | 45.7 | 3:0 | OK |
| 32 | virtual.main.D4130 | INT | 37.7 | 2:0 | OK |
| 33 | virtual.main.D4851 | INT | 36.0 | 2:0 | OK |
| 34 | virtual.main.D4632 | INT | 37.3 | 2:0 | OK |
| 35 | virtual.main.M121 | BOOL | 47.0 | 1:false | OK |
| 36 | virtual.main.D4252 | FLOAT | 131.7 | 38:0 | OK |
| 37 | virtual.main.debug | INT | 41.0 | 3:0 | OK |
| 38 | virtual.main.D1040 | INT | 35.0 | 2:0 | OK |
| 39 | virtual.main.runtoggle1s | INT | 53.7 | 2:0 | OK |
| 40 | virtual.main.M135 | BOOL | 29.7 | 1:false | OK |
| 41 | virtual.main.D4602 | INT | 40.0 | 2:0 | OK |
| 42 | virtual.main.debug6 | INT | 15.3 | 3:0 | OK |
| 43 | virtual.main.D30 | INT | 133.7 | 2:0 | OK |
| 44 | virtual.main.D4850 | INT | 31.7 | 2:0 | OK |
| 45 | virtual.main.D4620_general | INT | 40.3 | 2:0 | OK |
| 46 | virtual.main.M120_ | BOOL | 47.7 | 1:false | OK |
| 47 | virtual.main.D4250 | FLOAT | 50.3 | 38:0 | OK |
| 48 | virtual.main.D9998 | INT | 28.0 | 2:0 | OK |
| 49 | virtual.main.D1020 | INT | 128.3 | 2:0 | OK |
| 50 | virtual.main.M132 | BOOL | 37.3 | 1:false | OK |
| 51 | virtual.main.D4500_z9 | INT | 35.3 | 2:0 | OK |
| 52 | virtual.main.debug5 | INT | 43.0 | 3:0 | OK |
| 53 | virtual.main.D2504 | INT | 51.3 | 2:0 | OK |
| 54 | virtual.main.D4620_8000 | INT | 30.3 | 2:0 | OK |
| 55 | virtual.main.M120 | BOOL | 115.7 | 1:false | OK |
| 56 | virtual.main.D4226 | INT | 48.3 | 2:0 | OK |
| 57 | virtual.main.D5508 | FLOAT | 36.3 | 38:0 | OK |
| 58 | virtual.main.D10000 | INT | 59.0 | 2:0 | OK |

## Phase 2: Write Simulation Results

**Summary:** Total=58  OK=58  FAIL=0  ERR=0  |  avg latency=69.5 ms

| # | Variable | Type | Range | Last Written Value | avg_ms | Result | Status |
|---|----------|------|-------|--------------------|--------|--------|--------|
| 1 | virtual.main.M131 | BOOL | [false, true] | 0 | 66.7 | 1 | OK |
| 2 | virtual.main.D4500_z4 | INT | [1, 4] | 4 | 119.0 | 1 | OK |
| 3 | virtual.main.debug4 | INT | [0, 4294967295] | 561176174 | 59.3 | 1 | OK |
| 4 | virtual.main.D2503 | INT | [-32768, 32767] | 1294 | 50.7 | 1 | OK |
| 5 | virtual.main.current.maxr | FLOAT | [FLT_MIN, FLT_MAX] | -693.86050387... | 30.7 | 1 | OK |
| 6 | virtual.main.current.maxspeed | FLOAT | [FLT_MIN, FLT_MAX] | -425.08476593... | 46.7 | 1 | OK |
| 7 | virtual.main.current.maxxg | FLOAT | [FLT_MIN, FLT_MAX] | -808.28998486... | 37.0 | 1 | OK |
| 8 | virtual.main.current.maxzno | INT | [1, 10] | 7 | 153.7 | 1 | OK |
| 9 | virtual.main.D4620_25000 | INT | [100, 65000] | 19189 | 32.3 | 1 | OK |
| 10 | virtual.main.LockSecs | INT | [0, 4294967295] | 3148841267 | 43.3 | 1 | OK |
| 11 | virtual.main.D4224 | INT | [-32768, 32767] | -15790 | 34.7 | 1 | OK |
| 12 | virtual.main.D5090 | INT | [-32768, 32767] | -29701 | 59.0 | 1 | OK |
| 13 | virtual.main.M130_ | BOOL | [false, true] | 0 | 34.3 | 1 | OK |
| 14 | virtual.main.D4500_general | INT | [1, 20] | 13 | 125.3 | 1 | OK |
| 15 | virtual.main.debug3 | INT | [0, 4294967295] | 476077597 | 59.7 | 1 | OK |
| 16 | virtual.main.D200 | INT | [-32768, 32767] | -23275 | 57.3 | 1 | OK |
| 17 | virtual.main.zzlock | INT | [0, 1] | 0 | 49.3 | 1 | OK |
| 18 | virtual.main.M190 | BOOL | [false, true] | 0 | 150.0 | 1 | OK |
| 19 | virtual.main.D4620 | INT | [100, 20000] | 19455 | 45.0 | 1 | OK |
| 20 | virtual.main.Lockmin | INT | [0, 4294967295] | 4028697328 | 77.7 | 1 | OK |
| 21 | virtual.main.D4190 | FLOAT | [1, 2.7] | 1.37364128926072 | 60.3 | 1 | OK |
| 22 | virtual.main.D500 | INT | [1, 3] | 1 | 153.7 | 1 | OK |
| 23 | virtual.main.D4650 | INT | [-2147483648, 2147483647] | -2147483648 | 43.7 | 1 | OK |
| 24 | virtual.main.M130 | BOOL | [false, true] | 0 | 60.0 | 1 | OK |
| 25 | virtual.main.D4500 | INT | [1, 10] | 10 | 77.7 | 1 | OK |
| 26 | virtual.main.debug2 | INT | [0, 4294967295] | 3926017635 | 98.7 | 1 | OK |
| 27 | virtual.main.D136 | FLOAT | [FLT_MIN, FLT_MAX] | 511.495290470523 | 36.0 | 1 | OK |
| 28 | virtual.main.toggle1s | INT | [-32768, 32767] | 26205 | 70.7 | 1 | OK |
| 29 | virtual.main.M137 | BOOL | [false, true] | 0 | 57.0 | 1 | OK |
| 30 | virtual.main.D4606 | INT | [-2147483648, 2147483647] | -2147483648 | 90.3 | 1 | OK |
| 31 | virtual.main.debugMode | INT | [0, 4294967295] | 3243536459 | 171.7 | 1 | OK |
| 32 | virtual.main.D4130 | INT | [0, 1] | 1 | 35.0 | 1 | OK |
| 33 | virtual.main.D4851 | INT | [-32768, 32767] | -16644 | 33.7 | 1 | OK |
| 34 | virtual.main.D4632 | INT | [-2147483648, 2147483647] | -2147483648 | 47.7 | 1 | OK |
| 35 | virtual.main.M121 | BOOL | [false, true] | 0 | 44.7 | 1 | OK |
| 36 | virtual.main.D4252 | FLOAT | [FLT_MIN, FLT_MAX] | -754.00941790... | 106.0 | 1 | OK |
| 37 | virtual.main.debug | INT | [0, 4294967295] | 1222871658 | 32.3 | 1 | OK |
| 38 | virtual.main.D1040 | INT | [-32768, 32767] | -24390 | 55.7 | 1 | OK |
| 39 | virtual.main.runtoggle1s | INT | [-32768, 32767] | 19444 | 35.7 | 1 | OK |
| 40 | virtual.main.M135 | BOOL | [false, true] | 0 | 36.0 | 1 | OK |
| 41 | virtual.main.D4602 | INT | [-2147483648, 2147483647] | -2147483647 | 38.3 | 1 | OK |
| 42 | virtual.main.debug6 | INT | [0, 4294967295] | 114941184 | 106.0 | 1 | OK |
| 43 | virtual.main.D30 | INT | [-32768, 32767] | 10486 | 84.3 | 1 | OK |
| 44 | virtual.main.D4850 | INT | [-32768, 32767] | -25664 | 61.0 | 1 | OK |
| 45 | virtual.main.D4620_general | INT | [100, 65000] | 10022 | 59.3 | 1 | OK |
| 46 | virtual.main.M120_ | BOOL | [false, true] | 0 | 53.3 | 1 | OK |
| 47 | virtual.main.D4250 | FLOAT | [FLT_MIN, FLT_MAX] | 971.272996754468 | 203.7 | 1 | OK |
| 48 | virtual.main.D9998 | INT | [-2147483648, 2147483647] | -2147483647 | 46.0 | 1 | OK |
| 49 | virtual.main.D1020 | INT | [-32768, 32767] | -28188 | 49.3 | 1 | OK |
| 50 | virtual.main.M132 | BOOL | [false, true] | 0 | 52.0 | 1 | OK |
| 51 | virtual.main.D4500_z9 | INT | [1, 9] | 5 | 165.7 | 1 | OK |
| 52 | virtual.main.debug5 | INT | [0, 4294967295] | 3947146152 | 38.0 | 1 | OK |
| 53 | virtual.main.D2504 | INT | [-32768, 32767] | 12536 | 47.0 | 1 | OK |
| 54 | virtual.main.D4620_8000 | INT | [100, 16000] | 14805 | 39.7 | 1 | OK |
| 55 | virtual.main.M120 | BOOL | [false, true] | 0 | 127.7 | 1 | OK |
| 56 | virtual.main.D4226 | INT | [-32768, 32767] | -1470 | 49.3 | 1 | OK |
| 57 | virtual.main.D5508 | FLOAT | [100, 20000] | 13438.8663363734 | 48.3 | 1 | OK |
| 58 | virtual.main.D10000 | INT | [-2147483648, 2147483647] | -2147483647 | 84.3 | 1 | OK |

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total variables tested | 58 |
| Read OK | 58 / 58 |
| Read ERR | 0 / 58 |
| Write OK (result=1) | 58 / 58 |
| Write FAIL (result=0) | 0 / 58 |
| Write ERR (no response) | 0 / 58 |
| Avg read latency | 67.1 ms |
| Avg write latency | 69.5 ms |

## Conclusions

- **读测试覆盖率高**: 100% 变量响应正常 (58/58)
- **写入仿真成功**: 100% 变量写入返回 result=1

