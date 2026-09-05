# MQTT Performance Benchmark Report

**Date:** 2026-05-11 16:24:45
**Broker:** 192.167.200.1:1883
**Local ClientID:** perf_test_gd
**Remote ClientID:** svc_lxy4_winvm_001
**Enumerated vars:** 11 active, 48 skipped
**SAMPLE_SIZE:** 5  |  HIST_ITERS: 30  |  BATCH_ITERS: 10

## MB1: readVariable RTT

| Variable | min | avg | P50 | P95 | P99 | max (ms) |
|----------|-----|-----|-----|-----|-----|----------|
| virtual.main.M131 | 0 | 68.8 | 1 | 2 | 2033 | 2033 |
| virtual.main.M130_ | 0 | 1.0 | 1 | 2 | 3 | 3 |
| virtual.main.M190 | 0 | 0.7 | 1 | 1 | 1 | 1 |
| virtual.main.M130 | 0 | 0.7 | 1 | 1 | 2 | 2 |
| virtual.main.M137 | 0 | 0.7 | 1 | 1 | 2 | 2 |

## MB2: writeVariable RTT

| Variable | min | avg | P50 | P95 | P99 | max (ms) | result |
|----------|-----|-----|-----|-----|-----|----------|--------|
| virtual.main.M131 | 0 | 1.0 | 1 | 2 | 2 | 2 | 0 |
| virtual.main.M130_ | 0 | 0.6 | 1 | 1 | 1 | 1 | 0 |
| virtual.main.M190 | 0 | 1.1 | 1 | 3 | 3 | 3 | 0 |
| virtual.main.M130 | 0 | 0.8 | 1 | 2 | 2 | 2 | 0 |
| virtual.main.M137 | 0 | 1.0 | 1 | 3 | 3 | 3 | 0 |

## MB3: writeVariables Batch Sweep

| N | avg_ms | P50 | P95 | writes/s | result |
|---|--------|-----|-----|----------|--------|
| 1 | 0.6 | 1 | 1 | 1666.7 | 0 |
| 5 | 0.5 | 1 | 1 | 10000.0 | 0 |
| 10 | 1.4 | 1 | 3 | 7142.9 | 0 |
| 11 | 1.4 | 1 | 3 | 7857.1 | 0 |

## MB4: Signature Overhead

| Variable | with_sig P50 | without_sig P50 | overhead_ms | overhead % |
|----------|-------------|-----------------|-------------|------------|
| virtual.main.M131 | 1 ms | 1 ms | 0.0 | 0.0% |

## Conclusions

- **批量聚合效应显著**: MB3 N=11 writes/s (7857.1) > MB2 single × 5 (5639.1)

