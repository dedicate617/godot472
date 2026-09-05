#!/usr/bin/env godot
# test_write_perf.gd — OPC-UA write performance bottleneck benchmark
# Run: godot --headless --script modules/varmanager/test/test_write_perf.gd
#
# Phases:
#   P0   Enumerate all /Value nodes + save originals for known paths
#   WB1  Sync writeValues batch size sweep  (N = 1,2,3,5,7,9 from KNOWN_INT_PATHS)
#   WB2  Async writeValuesAsync + fence drain sweep  (same N)
#   WB3  Per-item P50/P95/P99 latency histogram  (single path, 30 iters)
#   WB4  Large-scale async throughput  (N = 50, 200, and all /Value nodes)
#   WB5  FLOAT vs INT write latency comparison

extends SceneTree

const SERVER_URL  = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH = "/"
const MAX_DEPTH   = 5
const TIMEOUT_MS  = 3000
const HIST_ITERS  = 30

# 9 known-writable INT /Value paths confirmed in test_write.gd + batch_read Phase 3
const KNOWN_INT_PATHS = [
	"virtual/Dummy/pageNo/Value",
	"virtual/Dummy/recipeFontSize/Value",
	"virtual/Dummy/CurrentUserGroupId/Value",
	"virtual/Dummy/AlarmPopup/Value",
	"virtual/Dummy/commErr/Value",
	"virtual/Dummy/ysjLogicMode/Value",
	"virtual/Dummy/enableON_OFF_Log/Value",
	"virtual/Dummy/tuiping2/Value",
	"virtual/Dummy/icustomMode/Value",
]

# 1 known FLOAT /Value path (type=3 confirmed in batch_read Phase 3)
const KNOWN_FLOAT_PATHS = [
	"virtual/Dummy/MaxTemp/Value",
]

var vm
var value_vars: Array = []      # all enumerated /Value paths from server
var int_originals: Array = []   # saved originals for KNOWN_INT_PATHS
var float_originals: Array = [] # saved originals for KNOWN_FLOAT_PATHS

var _pass_count := 0
var _fail_count := 0


func _init():
	vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable singleton not found")
		quit(1)
		return
	vm.setReadTimeoutMs(TIMEOUT_MS)
	vm.setWriteTimeoutMs(TIMEOUT_MS)

	var sc = vm.connect_start(SERVER_URL, OBJECT_PATH, true)
	print("connect_start -> ", sc)
	if sc != 1 and sc != 2:
		print("SKIP: cannot connect (sc=", sc, ")")
		quit(0)
		return
	print("Connected OK\n")

	_phase0_setup()
	_run_wb1()
	_run_wb2()
	_run_wb3()
	_run_wb4()
	_run_wb5()

	print("\n=== Summary: %d PASS  %d FAIL ===" % [_pass_count, _fail_count])
	print("=== Overall: %s ===" % ("PASS" if _fail_count == 0 else "FAIL"))
	vm.connect_close()
	quit(0 if _fail_count == 0 else 1)


func _check(label: String, ok: bool) -> void:
	if ok:
		_pass_count += 1
		print("  PASS: ", label)
	else:
		_fail_count += 1
		print("  FAIL: ", label)


func _recurse(parent_path: String, browse_path: String, depth: int) -> void:
	if depth > MAX_DEPTH:
		return
	var children = vm.getChildrenOfPath(browse_path)
	for child in children:
		var name = child["name"]
		var cls  = child["class"]
		var full = (parent_path + "/" + name) if parent_path != "" else name
		if cls == 2:
			if full.ends_with("/Value"):
				value_vars.append(full)
		elif cls == 1:
			_recurse(full, full, depth + 1)


func _phase0_setup() -> void:
	print("=== Phase 0: enumerate /Value nodes + save originals ===")
	var t0 = Time.get_ticks_msec()
	_recurse("", "", 0)
	var enum_ms = Time.get_ticks_msec() - t0
	print("Enumerated %d /Value nodes in %d ms" % [value_vars.size(), enum_ms])

	for p in KNOWN_INT_PATHS:
		int_originals.append(vm.readValue(p))
	for p in KNOWN_FLOAT_PATHS:
		float_originals.append(vm.readValue(p))
	print("  Saved %d INT originals, %d FLOAT originals\n" % [
		int_originals.size(), float_originals.size()])


func _restore_known_ints() -> void:
	for i in range(KNOWN_INT_PATHS.size()):
		vm.writeValue(KNOWN_INT_PATHS[i], int_originals[i] if int_originals[i] != null else 0)


func _restore_known_floats() -> void:
	for i in range(KNOWN_FLOAT_PATHS.size()):
		vm.writeValue(KNOWN_FLOAT_PATHS[i], float_originals[i] if float_originals[i] != null else 0.0)


func _run_wb1() -> void:
	print("=== WB1: sync writeValues batch size sweep ===")
	print("  N  | total_ms | ms/write | writes/sec | sc")
	print("  ---+----------+----------+------------+---")

	for n in [1, 2, 3, 5, 7, 9]:
		var paths = KNOWN_INT_PATHS.slice(0, n)
		var vals: Array = []
		for i in range(n):
			vals.append(300 + i)

		var t0 = Time.get_ticks_msec()
		var sc = vm.writeValues(paths, vals)
		var ms = Time.get_ticks_msec() - t0
		var per  = float(ms) / n
		var tput = float(n) / (ms / 1000.0) if ms > 0 else 0.0
		print("  %2d | %8d | %8.1f | %10.1f | %d" % [n, ms, per, tput, sc])

		_restore_known_ints()

	print()
	print("  KPI-WB1: if ms/write ≈ constant → no server-side pipelining in sync path")
	print("           if ms/write decreasing → batch overhead amortized or pipelined\n")

func _run_wb2() -> void:
	print("=== WB2: async writeValuesAsync + fence drain sweep ===")
	print("  N  | fire_ms | drain_ms | total_ms | writes/sec")
	print("  ---+---------+----------+----------+-----------")

	for n in [1, 2, 3, 5, 7, 9]:
		var paths = KNOWN_INT_PATHS.slice(0, n)
		var vals: Array = []
		for i in range(n):
			vals.append(400 + i)

		var t0 = Time.get_ticks_msec()
		vm.writeValuesAsync(paths, vals)
		var fire_ms = Time.get_ticks_msec() - t0

		# readValue on paths[0] blocks until the async write (ahead in queue) + this read resolve
		var t1 = Time.get_ticks_msec()
		@warning_ignore("unused_variable")
		var _fence = vm.readValue(paths[0])
		var drain_ms = Time.get_ticks_msec() - t1

		var total_ms = Time.get_ticks_msec() - t0
		var tput = float(n) / (total_ms / 1000.0) if total_ms > 0 else 0.0
		print("  %2d | %7d | %8d | %8d | %10.1f" % [n, fire_ms, drain_ms, total_ms, tput])

		_restore_known_ints()

	print()
	print("  KPI-WB2: fire_ms should be < 5 ms (non-blocking)")
	print("           drain_ms ≈ N × (run_once interval) → baseline bottleneck confirmed\n")

func _run_wb3() -> void:
	print("=== WB3: per-item sync write latency histogram (%d iters) ===" % HIST_ITERS)

	var path = KNOWN_INT_PATHS[0]   # "virtual/Dummy/pageNo/Value"
	var latencies: Array = []

	for i in range(HIST_ITERS):
		var t0 = Time.get_ticks_msec()
		vm.writeValue(path, i % 200)
		latencies.append(Time.get_ticks_msec() - t0)

	latencies.sort()
	var p50 = latencies[int(HIST_ITERS * 0.50)]
	var p95 = latencies[int(HIST_ITERS * 0.95)]
	var p99 = latencies[mini(int(HIST_ITERS * 0.99), HIST_ITERS - 1)]
	var min_ms = latencies[0]
	var max_ms = latencies[HIST_ITERS - 1]
	var sum_ms := 0
	for v in latencies:
		sum_ms += v
	var avg_ms = float(sum_ms) / HIST_ITERS

	print("  min=%d  avg=%.1f  P50=%d  P95=%d  P99=%d  max=%d  (ms)" % [
		min_ms, avg_ms, p50, p95, p99, max_ms])

	# Emit raw distribution buckets (<50, 50-100, 100-150, 150-200, 200-300, >300 ms)
	var buckets = [0, 0, 0, 0, 0, 0]
	for v in latencies:
		if   v < 50:   buckets[0] += 1
		elif v < 100:  buckets[1] += 1
		elif v < 150:  buckets[2] += 1
		elif v < 200:  buckets[3] += 1
		elif v < 300:  buckets[4] += 1
		else:          buckets[5] += 1
	print("  Histogram: <50ms=%d  50-100=%d  100-150=%d  150-200=%d  200-300=%d  >300=%d" % [
		buckets[0], buckets[1], buckets[2], buckets[3], buckets[4], buckets[5]])

	print("  KPI-WB3: P50=%d ms  P95=%d ms  P99=%d ms" % [p50, p95, p99])
	_check("WB3: P50 sync write latency within timeout", p50 < TIMEOUT_MS)
	print()

	_restore_known_ints()

func _run_wb4() -> void:
	print("=== WB4: large-scale async writeValuesAsync throughput ===")

	if value_vars.is_empty():
		print("  SKIP: no enumerated /Value nodes (Phase 0 enumeration failed or returned 0)\n")
		return

	var actual_max = value_vars.size()
	var sweep: Array = []
	for candidate in [50, 200, actual_max]:
		if candidate <= actual_max and not sweep.has(candidate):
			sweep.append(candidate)

	print("  Testing N in %s  (server has %d /Value nodes)" % [str(sweep), actual_max])
	print("  NOTE: writes value=0 (INT) to all paths; FLOAT nodes may reject silently.")
	print("  N    | fire_ms | drain_ms | total_ms | writes/sec | readback[0]")
	print("  -----+---------+----------+----------+------------+------------")

	for n in sweep:
		var paths = value_vars.slice(0, n)
		var vals: Array = []
		for _i in range(n):
			vals.append(0)

		var t0 = Time.get_ticks_msec()
		vm.writeValuesAsync(paths, vals)
		var fire_ms = Time.get_ticks_msec() - t0

		var t1 = Time.get_ticks_msec()
		var fence_val = vm.readValue(paths[0])
		var drain_ms = Time.get_ticks_msec() - t1

		var total_ms = Time.get_ticks_msec() - t0
		var tput = float(n) / (total_ms / 1000.0) if total_ms > 0 else 0.0
		print("  %5d | %7d | %8d | %8d | %10.1f | %s" % [
			n, fire_ms, drain_ms, total_ms, tput, str(fence_val)])

	print()
	print("  KPI-WB4: throughput curve — if writes/sec ≈ constant across N, queue scales linearly.")
	print("           If throughput drops at large N, investigate queue lock contention.\n")

func _run_wb5() -> void:
	print("=== WB5: FLOAT vs INT write latency comparison ===")

	const ITERS = 5
	var int_path   = KNOWN_INT_PATHS[0]    # "virtual/Dummy/pageNo/Value"
	var float_path = KNOWN_FLOAT_PATHS[0]  # "virtual/Dummy/MaxTemp/Value"

	# INT: write 5 times
	var int_total := 0
	for i in range(ITERS):
		var t0 = Time.get_ticks_msec()
		vm.writeValue(int_path, i % 10)
		int_total += Time.get_ticks_msec() - t0
	var int_avg = float(int_total) / ITERS

	# FLOAT: write 5 times
	var float_total := 0
	for i in range(ITERS):
		var t0 = Time.get_ticks_msec()
		vm.writeValue(float_path, float(i) * 0.5)
		float_total += Time.get_ticks_msec() - t0
	var float_avg = float(float_total) / ITERS

	print("  INT   path: %s" % int_path)
	print("  FLOAT path: %s" % float_path)
	print("  INT   avg latency: %.1f ms  (%d iters)" % [int_avg, ITERS])
	print("  FLOAT avg latency: %.1f ms  (%d iters)" % [float_avg, ITERS])
	var ratio = float_avg / int_avg if int_avg > 0.0 else 0.0
	print("  FLOAT/INT ratio: %.2f  (1.00 = same; >1.10 = FLOAT slower)" % ratio)
	print("  KPI-WB5: INT_avg=%.1f ms  FLOAT_avg=%.1f ms  ratio=%.2f" % [int_avg, float_avg, ratio])

	_check("WB5: FLOAT write avg < TIMEOUT_MS", float_avg < TIMEOUT_MS)
	print()

	_restore_known_ints()
	_restore_known_floats()
