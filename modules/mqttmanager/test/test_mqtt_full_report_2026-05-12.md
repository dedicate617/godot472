# MQTT Full Read/Write Test Report

**Date:** 2026-05-12 13:04:44
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

**Summary:** Total=58  OK=58  ERR=0  |  avg latency=72.5 ms

| # | Variable | Type | avg_ms | Latest Value | Status |
|---|----------|------|--------|--------------|--------|
| 1 | virtual.main.M131 | BOOL | 754.0 | 1:false | OK |
| 2 | virtual.main.D4500_z4 | INT | 53.7 | 2:0 | OK |
| 3 | virtual.main.debug4 | INT | 52.7 | 3:0 | OK |
| 4 | virtual.main.D2503 | INT | 53.7 | 2:0 | OK |
| 5 | virtual.main.current.maxr | FLOAT | 53.0 | 38:0 | OK |
| 6 | virtual.main.current.maxspeed | FLOAT | 53.7 | 38:0 | OK |
| 7 | virtual.main.current.maxxg | FLOAT | 56.3 | 38:0 | OK |
| 8 | virtual.main.current.maxzno | INT | 62.7 | 2:0 | OK |
| 9 | virtual.main.D4620_25000 | INT | 57.7 | 2:0 | OK |
| 10 | virtual.main.LockSecs | INT | 65.0 | 3:0 | OK |
| 11 | virtual.main.D4224 | INT | 60.0 | 2:0 | OK |
| 12 | virtual.main.D5090 | INT | 61.0 | 2:0 | OK |
| 13 | virtual.main.M130_ | BOOL | 62.7 | 1:false | OK |
| 14 | virtual.main.D4500_general | INT | 61.7 | 2:0 | OK |
| 15 | virtual.main.debug3 | INT | 61.0 | 3:0 | OK |
| 16 | virtual.main.D200 | INT | 61.7 | 2:0 | OK |
| 17 | virtual.main.zzlock | INT | 60.0 | 2:0 | OK |
| 18 | virtual.main.M190 | BOOL | 65.0 | 1:false | OK |
| 19 | virtual.main.D4620 | INT | 60.0 | 2:0 | OK |
| 20 | virtual.main.Lockmin | INT | 61.0 | 3:0 | OK |
| 21 | virtual.main.D4190 | FLOAT | 64.0 | 38:0 | OK |
| 22 | virtual.main.D500 | INT | 65.7 | 2:0 | OK |
| 23 | virtual.main.D4650 | INT | 79.3 | 2:0 | OK |
| 24 | virtual.main.M130 | BOOL | 61.3 | 1:false | OK |
| 25 | virtual.main.D4500 | INT | 65.3 | 2:0 | OK |
| 26 | virtual.main.debug2 | INT | 65.3 | 3:0 | OK |
| 27 | virtual.main.D136 | FLOAT | 62.3 | 38:0 | OK |
| 28 | virtual.main.toggle1s | INT | 57.7 | 2:0 | OK |
| 29 | virtual.main.M137 | BOOL | 55.7 | 1:false | OK |
| 30 | virtual.main.D4606 | INT | 54.7 | 2:0 | OK |
| 31 | virtual.main.debugMode | INT | 61.7 | 3:0 | OK |
| 32 | virtual.main.D4130 | INT | 56.0 | 2:0 | OK |
| 33 | virtual.main.D4851 | INT | 54.7 | 2:0 | OK |
| 34 | virtual.main.D4632 | INT | 54.3 | 2:0 | OK |
| 35 | virtual.main.M121 | BOOL | 55.7 | 1:false | OK |
| 36 | virtual.main.D4252 | FLOAT | 52.3 | 38:0 | OK |
| 37 | virtual.main.debug | INT | 56.0 | 3:0 | OK |
| 38 | virtual.main.D1040 | INT | 61.0 | 2:0 | OK |
| 39 | virtual.main.runtoggle1s | INT | 60.7 | 2:0 | OK |
| 40 | virtual.main.M135 | BOOL | 57.0 | 1:false | OK |
| 41 | virtual.main.D4602 | INT | 61.0 | 2:0 | OK |
| 42 | virtual.main.debug6 | INT | 61.0 | 3:0 | OK |
| 43 | virtual.main.D30 | INT | 64.3 | 2:0 | OK |
| 44 | virtual.main.D4850 | INT | 62.0 | 2:0 | OK |
| 45 | virtual.main.D4620_general | INT | 61.7 | 2:0 | OK |
| 46 | virtual.main.M120_ | BOOL | 62.0 | 1:false | OK |
| 47 | virtual.main.D4250 | FLOAT | 60.7 | 38:0 | OK |
| 48 | virtual.main.D9998 | INT | 59.7 | 2:0 | OK |
| 49 | virtual.main.D1020 | INT | 64.0 | 2:0 | OK |
| 50 | virtual.main.M132 | BOOL | 61.0 | 1:false | OK |
| 51 | virtual.main.D4500_z9 | INT | 75.0 | 2:0 | OK |
| 52 | virtual.main.debug5 | INT | 68.7 | 3:0 | OK |
| 53 | virtual.main.D2504 | INT | 62.3 | 2:0 | OK |
| 54 | virtual.main.D4620_8000 | INT | 63.0 | 2:0 | OK |
| 55 | virtual.main.M120 | BOOL | 62.7 | 1:false | OK |
| 56 | virtual.main.D4226 | INT | 63.0 | 2:0 | OK |
| 57 | virtual.main.D5508 | FLOAT | 60.7 | 38:0 | OK |
| 58 | virtual.main.D10000 | INT | 55.7 | 2:0 | OK |

## Phase 2: Write Simulation Results

**Summary:** Total=58  OK=58  FAIL=0  ERR=0  |  avg latency=63.0 ms

| # | Variable | Type | Range | Last Written Value | avg_ms | Result | Status |
|---|----------|------|-------|--------------------|--------|--------|--------|
| 1 | virtual.main.M131 | BOOL | [false, true] | 0 | 81.3 | 1 | OK |
| 2 | virtual.main.D4500_z4 | INT | [1, 4] | 2 | 59.0 | 1 | OK |
| 3 | virtual.main.debug4 | INT | [0, 4294967295] | 4000197224 | 63.7 | 1 | OK |
| 4 | virtual.main.D2503 | INT | [-32768, 32767] | 32181 | 58.0 | 1 | OK |
| 5 | virtual.main.current.maxr | FLOAT | [FLT_MIN, FLT_MAX] | -391.01542606... | 61.7 | 1 | OK |
| 6 | virtual.main.current.maxspeed | FLOAT | [FLT_MIN, FLT_MAX] | -604.68270762... | 56.0 | 1 | OK |
| 7 | virtual.main.current.maxxg | FLOAT | [FLT_MIN, FLT_MAX] | -547.4474086155 | 55.0 | 1 | OK |
| 8 | virtual.main.current.maxzno | INT | [1, 10] | 10 | 56.7 | 1 | OK |
| 9 | virtual.main.D4620_25000 | INT | [100, 65000] | 2683 | 63.0 | 1 | OK |
| 10 | virtual.main.LockSecs | INT | [0, 4294967295] | 1265026434 | 63.3 | 1 | OK |
| 11 | virtual.main.D4224 | INT | [-32768, 32767] | -4020 | 63.7 | 1 | OK |
| 12 | virtual.main.D5090 | INT | [-32768, 32767] | -235 | 67.7 | 1 | OK |
| 13 | virtual.main.M130_ | BOOL | [false, true] | 0 | 62.7 | 1 | OK |
| 14 | virtual.main.D4500_general | INT | [1, 20] | 11 | 65.3 | 1 | OK |
| 15 | virtual.main.debug3 | INT | [0, 4294967295] | 2792833407 | 64.0 | 1 | OK |
| 16 | virtual.main.D200 | INT | [-32768, 32767] | -10090 | 70.7 | 1 | OK |
| 17 | virtual.main.zzlock | INT | [0, 1] | 1 | 63.7 | 1 | OK |
| 18 | virtual.main.M190 | BOOL | [false, true] | 0 | 62.7 | 1 | OK |
| 19 | virtual.main.D4620 | INT | [100, 20000] | 12292 | 72.7 | 1 | OK |
| 20 | virtual.main.Lockmin | INT | [0, 4294967295] | 1248408228 | 63.3 | 1 | OK |
| 21 | virtual.main.D4190 | FLOAT | [1, 2.7] | 1.30072348176451 | 63.3 | 1 | OK |
| 22 | virtual.main.D500 | INT | [1, 3] | 2 | 66.7 | 1 | OK |
| 23 | virtual.main.D4650 | INT | [-2147483648, 2147483647] | -2147483647 | 64.0 | 1 | OK |
| 24 | virtual.main.M130 | BOOL | [false, true] | 0 | 63.3 | 1 | OK |
| 25 | virtual.main.D4500 | INT | [1, 10] | 4 | 69.7 | 1 | OK |
| 26 | virtual.main.debug2 | INT | [0, 4294967295] | 2776940389 | 64.3 | 1 | OK |
| 27 | virtual.main.D136 | FLOAT | [FLT_MIN, FLT_MAX] | -614.74015084988 | 65.0 | 1 | OK |
| 28 | virtual.main.toggle1s | INT | [-32768, 32767] | 2964 | 73.0 | 1 | OK |
| 29 | virtual.main.M137 | BOOL | [false, true] | 0 | 58.3 | 1 | OK |
| 30 | virtual.main.D4606 | INT | [-2147483648, 2147483647] | -2147483647 | 57.3 | 1 | OK |
| 31 | virtual.main.debugMode | INT | [0, 4294967295] | 2321369752 | 44.3 | 1 | OK |
| 32 | virtual.main.D4130 | INT | [0, 1] | 1 | 54.0 | 1 | OK |
| 33 | virtual.main.D4851 | INT | [-32768, 32767] | -22124 | 61.0 | 1 | OK |
| 34 | virtual.main.D4632 | INT | [-2147483648, 2147483647] | -2147483648 | 57.0 | 1 | OK |
| 35 | virtual.main.M121 | BOOL | [false, true] | 0 | 55.7 | 1 | OK |
| 36 | virtual.main.D4252 | FLOAT | [FLT_MIN, FLT_MAX] | -868.3205803691 | 56.0 | 1 | OK |
| 37 | virtual.main.debug | INT | [0, 4294967295] | 618397362 | 56.0 | 1 | OK |
| 38 | virtual.main.D1040 | INT | [-32768, 32767] | 28318 | 56.3 | 1 | OK |
| 39 | virtual.main.runtoggle1s | INT | [-32768, 32767] | -13480 | 61.7 | 1 | OK |
| 40 | virtual.main.M135 | BOOL | [false, true] | 0 | 62.3 | 1 | OK |
| 41 | virtual.main.D4602 | INT | [-2147483648, 2147483647] | -2147483647 | 64.0 | 1 | OK |
| 42 | virtual.main.debug6 | INT | [0, 4294967295] | 2304703926 | 64.3 | 1 | OK |
| 43 | virtual.main.D30 | INT | [-32768, 32767] | -12167 | 63.0 | 1 | OK |
| 44 | virtual.main.D4850 | INT | [-32768, 32767] | -15837 | 65.7 | 1 | OK |
| 45 | virtual.main.D4620_general | INT | [100, 65000] | 32475 | 64.7 | 1 | OK |
| 46 | virtual.main.M120_ | BOOL | [false, true] | 0 | 72.7 | 1 | OK |
| 47 | virtual.main.D4250 | FLOAT | [FLT_MIN, FLT_MAX] | -641.50912250... | 65.3 | 1 | OK |
| 48 | virtual.main.D9998 | INT | [-2147483648, 2147483647] | -2147483648 | 62.3 | 1 | OK |
| 49 | virtual.main.D1020 | INT | [-32768, 32767] | -6600 | 67.7 | 1 | OK |
| 50 | virtual.main.M132 | BOOL | [false, true] | 0 | 62.7 | 1 | OK |
| 51 | virtual.main.D4500_z9 | INT | [1, 9] | 2 | 63.7 | 1 | OK |
| 52 | virtual.main.debug5 | INT | [0, 4294967295] | 218124284 | 64.7 | 1 | OK |
| 53 | virtual.main.D2504 | INT | [-32768, 32767] | 1635 | 64.3 | 1 | OK |
| 54 | virtual.main.D4620_8000 | INT | [100, 16000] | 8607 | 69.3 | 1 | OK |
| 55 | virtual.main.M120 | BOOL | [false, true] | 0 | 63.3 | 1 | OK |
| 56 | virtual.main.D4226 | INT | [-32768, 32767] | 26464 | 64.3 | 1 | OK |
| 57 | virtual.main.D5508 | FLOAT | [100, 20000] | 10447.3537129904 | 65.0 | 1 | OK |
| 58 | virtual.main.D10000 | INT | [-2147483648, 2147483647] | -2147483648 | 64.3 | 1 | OK |

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total variables tested | 58 |
| Read OK | 58 / 58 |
| Read ERR | 0 / 58 |
| Write OK (result=1) | 58 / 58 |
| Write FAIL (result=0) | 0 / 58 |
| Write ERR (no response) | 0 / 58 |
| Avg read latency | 72.5 ms |
| Avg write latency | 63.0 ms |

## Conclusions

- **读测试覆盖率高**: 100% 变量响应正常 (58/58)
- **写入仿真成功**: 100% 变量写入返回 result=1

