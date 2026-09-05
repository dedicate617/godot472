#!/usr/bin/env godot
# test_write.gd — OPC-UA write correctness + performance test
# Run: godot --headless --script modules/varmanager/test/test_write.gd
#
# Phases:
#   W1  Single sync writeValue + readback verify
#   W2  Write ordering: two consecutive writes, verify final value
#   W3  Batch sync writeValues (N paths) + readback verify all
#   W4  Single writeValueAsync (fire) + readValue verify
#   W5  Batch writeValuesAsync (N fire) + readValues verify all
#   W6  Throughput benchmark: sequential sync vs batch async

extends SceneTree

const SERVER_URL   = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH  = "/"
const TIMEOUT_MS   = 3000

# Primary single-write test variable (INT)
const WRITE_VAR    = "virtual/Dummy/pageNo/Value"

# Batch write targets — all INT, under virtual/Dummy
const BATCH_VARS   = [
	"virtual/Dummy/recipeFontSize/Value",
	"virtual/Dummy/CurrentUserGroupId/Value",
	"virtual/Dummy/AlarmPopup/Value",
	"virtual/Dummy/commErr/Value",
	"virtual/Dummy/ysjLogicMode/Value",
]

# Throughput benchmark iterations
const BENCH_ITERS  = 10

var vm
var _pass_count := 0
var _fail_count := 0


func _init():
	vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable singleton not found")
		quit(1)
		return

	vm.setWriteTimeoutMs(TIMEOUT_MS)
	vm.setReadTimeoutMs(TIMEOUT_MS)

	var sc = vm.connect_start(SERVER_URL, OBJECT_PATH, true)
	print("connect_start -> ", sc)
	if sc != 1 and sc != 2:
		print("SKIP: cannot connect (sc=", sc, ")")
		quit(0)
		return
	print("Connected OK\n")

	_run_w1()
	_run_w2()
	_run_w3()
	_run_w4()
	_run_w5()
	_run_w6()

	print("\n=== Summary: %d PASS  %d FAIL ===" % [_pass_count, _fail_count])
	print("=== Overall: %s ===" % ("PASS" if _fail_count == 0 else "FAIL"))
	vm.connect_close()
	quit(0 if _fail_count == 0 else 1)


# ── helpers ────────────────────────────────────────────────────────────────────

func _check(label: String, ok: bool) -> void:
	if ok:
		_pass_count += 1
		print("  PASS: ", label)
	else:
		_fail_count += 1
		print("  FAIL: ", label)


func _read(path: String) -> Variant:
	return vm.readValue(path)


func _write(path: String, val) -> int:
	return vm.writeValue(path, val)


# ── W1  Single sync write + verify ────────────────────────────────────────────

func _run_w1() -> void:
	print("=== W1: single sync writeValue + readback verify ===")
	var orig = _read(WRITE_VAR)
	print("  original value: ", orig, "  (type=", typeof(orig), ")")

	var TEST_VAL = 42

	var t0 = Time.get_ticks_msec()
	var sc = _write(WRITE_VAR, TEST_VAL)
	var write_ms = Time.get_ticks_msec() - t0
	print("  writeValue(%s, %d) -> sc=%d  elapsed=%d ms" % [WRITE_VAR, TEST_VAL, sc, write_ms])

	var t1 = Time.get_ticks_msec()
	var readback = _read(WRITE_VAR)
	var read_ms = Time.get_ticks_msec() - t1
	print("  readback -> %s  (type=%d)  elapsed=%d ms" % [str(readback), typeof(readback), read_ms])

	_check("W1-sc:   writeValue returned ALL_OK(1)", sc == 1)
	_check("W1-data: readback == %d" % TEST_VAL, readback == TEST_VAL)

	# Restore
	_write(WRITE_VAR, orig if orig != null else 0)
	print("  restored -> ", orig)
	print()


# ── W2  Write ordering ────────────────────────────────────────────────────────

func _run_w2() -> void:
	print("=== W2: write ordering — two consecutive writes, verify final value ===")
	var orig = _read(WRITE_VAR)

	var sc1 = _write(WRITE_VAR, 10)
	var sc2 = _write(WRITE_VAR, 20)
	var final_val = _read(WRITE_VAR)
	print("  write(10)->sc=%d  write(20)->sc=%d  readback=%s" % [sc1, sc2, str(final_val)])

	_check("W2-sc:   both writes ALL_OK(1)", sc1 == 1 and sc2 == 1)
	_check("W2-data: final value == 20 (ordering preserved)", final_val == 20)

	_write(WRITE_VAR, orig if orig != null else 0)
	print()


# ── W3  Batch sync writeValues + verify ───────────────────────────────────────

func _run_w3() -> void:
	print("=== W3: batch sync writeValues (%d paths) + readback verify ===" % BATCH_VARS.size())

	# Save originals
	var originals: Array = []
	for p in BATCH_VARS:
		originals.append(_read(p))

	# Write test values: 100, 101, 102, ...
	var test_vals: Array = []
	for i in range(BATCH_VARS.size()):
		test_vals.append(100 + i)

	var t0 = Time.get_ticks_msec()
	var sc = vm.writeValues(BATCH_VARS, test_vals)
	var write_ms = Time.get_ticks_msec() - t0
	print("  writeValues(%d paths) -> sc=%d  elapsed=%d ms" % [BATCH_VARS.size(), sc, write_ms])
	print("  throughput: %.1f writes/sec" % (BATCH_VARS.size() / (write_ms / 1000.0) if write_ms > 0 else 0.0))

	_check("W3-sc: writeValues returned ALL_OK(1)", sc == 1)

	# Verify all
	var all_ok = true
	for i in range(BATCH_VARS.size()):
		var v = _read(BATCH_VARS[i])
		var ok = (v == test_vals[i])
		if not ok:
			print("  MISMATCH: %s expected=%d got=%s" % [BATCH_VARS[i], test_vals[i], str(v)])
			all_ok = false
	_check("W3-data: all %d readbacks match written values" % BATCH_VARS.size(), all_ok)

	# Restore
	vm.writeValues(BATCH_VARS, originals)
	print()


# ── W4  Single writeValueAsync + readValue verify ─────────────────────────────

func _run_w4() -> void:
	print("=== W4: single writeValueAsync (fire) + readValue verify ===")
	var orig = _read(WRITE_VAR)

	var TEST_VAL = 77

	var t0 = Time.get_ticks_msec()
	var sc = vm.writeValueAsync(WRITE_VAR, TEST_VAL)
	var post_ms = Time.get_ticks_msec() - t0
	print("  writeValueAsync post elapsed=%d ms  sc=%d  (returns immediately)" % [post_ms, sc])

	# readValue blocks until the write (ahead of it in queue) completes + read resolves
	var t1 = Time.get_ticks_msec()
	var readback = _read(WRITE_VAR)
	var total_ms = Time.get_ticks_msec() - t1
	print("  readback -> %s  total elapsed=%d ms" % [str(readback), total_ms])

	_check("W4-sc:   writeValueAsync returned ALL_OK(1)", sc == 1)
	_check("W4-post: writeValueAsync returned in < 50 ms (non-blocking)", post_ms < 50)
	_check("W4-data: readback == %d" % TEST_VAL, readback == TEST_VAL)

	_write(WRITE_VAR, orig if orig != null else 0)
	print()


# ── W5  Batch writeValuesAsync + readValues verify ────────────────────────────

func _run_w5() -> void:
	print("=== W5: batch writeValuesAsync (%d paths, fire) + readValues verify ===" % BATCH_VARS.size())

	var originals: Array = []
	for p in BATCH_VARS:
		originals.append(_read(p))

	var test_vals: Array = []
	for i in range(BATCH_VARS.size()):
		test_vals.append(200 + i)

	var t0 = Time.get_ticks_msec()
	var sc = vm.writeValuesAsync(BATCH_VARS, test_vals)
	var post_ms = Time.get_ticks_msec() - t0
	print("  writeValuesAsync(%d paths) post elapsed=%d ms  sc=%d" % [BATCH_VARS.size(), post_ms, sc])

	# readValues blocks until all N read commands (queued after the write command) resolve
	var t1 = Time.get_ticks_msec()
	var readbacks = vm.readValues(BATCH_VARS)
	var total_ms = Time.get_ticks_msec() - t1
	print("  readValues(%d paths) elapsed=%d ms" % [BATCH_VARS.size(), total_ms])

	_check("W5-sc:   writeValuesAsync returned ALL_OK(1)", sc == 1)
	_check("W5-post: writeValuesAsync returned in < 50 ms", post_ms < 50)

	var all_ok = true
	for i in range(BATCH_VARS.size()):
		var v = readbacks[i] if i < readbacks.size() else null
		if v != test_vals[i]:
			print("  MISMATCH: %s expected=%d got=%s" % [BATCH_VARS[i], test_vals[i], str(v)])
			all_ok = false
	_check("W5-data: all %d readbacks match written values" % BATCH_VARS.size(), all_ok)

	vm.writeValues(BATCH_VARS, originals)
	print()


# ── W6  Throughput benchmark ──────────────────────────────────────────────────

func _run_w6() -> void:
	print("=== W6: throughput benchmark (%d iterations) ===" % BENCH_ITERS)
	var orig = _read(WRITE_VAR)

	# W6-A: sequential sync writeValue
	var t0 = Time.get_ticks_msec()
	for i in range(BENCH_ITERS):
		_write(WRITE_VAR, i)
	var sync_ms = Time.get_ticks_msec() - t0
	var sync_per = float(sync_ms) / BENCH_ITERS
	var sync_tput = BENCH_ITERS / (sync_ms / 1000.0) if sync_ms > 0 else 0.0
	print("  [sync]  %d× writeValue  total=%d ms  avg=%.1f ms/write  tput=%.1f writes/sec" % [
		BENCH_ITERS, sync_ms, sync_per, sync_tput])

	# W6-B: batch async writeValueAsync (all fire) + one readValue to wait for drain
	var t1 = Time.get_ticks_msec()
	for i in range(BENCH_ITERS):
		vm.writeValueAsync(WRITE_VAR, i)
	# A readValue queued after all writes acts as a fence — resolves when writes+read are done
	var fence_val = _read(WRITE_VAR)
	var async_ms = Time.get_ticks_msec() - t1
	var async_per = float(async_ms) / BENCH_ITERS
	var async_tput = BENCH_ITERS / (async_ms / 1000.0) if async_ms > 0 else 0.0
	print("  [async] %d× writeValueAsync + fence readValue  total=%d ms  avg=%.1f ms/write  tput=%.1f writes/sec" % [
		BENCH_ITERS, async_ms, async_per, async_tput])
	print("  fence readback = %s  (expect %d)" % [str(fence_val), BENCH_ITERS - 1])

	_check("W6-fence: fence readback == last written value (%d)" % (BENCH_ITERS - 1),
		fence_val == BENCH_ITERS - 1)

	var speedup = float(sync_ms) / async_ms if async_ms > 0 else 0.0
	print("  async speedup vs sync: %.2fx" % speedup)
	print("  KPI-W1: sync write avg latency  = %.1f ms" % sync_per)
	print("  KPI-W2: async write throughput  = %.1f writes/sec" % async_tput)
	print("  KPI-W3: async/sync speedup      = %.2fx" % speedup)

	_write(WRITE_VAR, orig if orig != null else 0)
	print()
