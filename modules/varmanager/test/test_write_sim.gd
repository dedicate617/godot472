#!/usr/bin/env godot
# test_write_sim.gd — OPC-UA virtual/main/* write simulation benchmark
# Run: godot --headless --script modules/varmanager/test/test_write_sim.gd
#
# Phases:
#   0A  Enumerate virtual/main/* /Value nodes
#   0B  Batch async read Value/HL/LL; build vars[] dict
#   1   10-round linear benchmark (sync vs async)
#   2   30-second random simulation
#   3   Write Markdown report to disk

extends SceneTree

const SERVER_URL   = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH  = "/"
const ROOT_PATH    = "virtual/main"
const MAX_DEPTH    = 3
const TIMEOUT_MS   = 3000
const BENCH_ROUNDS = 10
const SIM_SECONDS  = 30

var vm
var vars: Array    = []   # active variable dicts (see schema above)
var skipped: Array = []   # val_paths that were skipped
var _candidate_paths: Array = []   # /Value paths found before HL/LL validation


func _init() -> void:
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

	_phase0a_enumerate()
	if _candidate_paths.is_empty():
		print("SKIP: no /Value nodes found under ", ROOT_PATH)
		vm.connect_close()
		quit(0)
		return

	_phase0b_build_vars()
	if vars.is_empty():
		print("SKIP: no active writable variables after HL/LL validation")
		vm.connect_close()
		quit(0)
		return

	var ph1 = _phase1_benchmark()
	var ph2 = _phase2_simulation()
	_phase3_report(ph1, ph2)

	vm.connect_close()
	quit(0)


func _recurse_main(parent_path: String, browse_path: String, depth: int) -> void:
	if depth > MAX_DEPTH:
		return
	var children = vm.getChildrenOfPath(browse_path)
	for child in children:
		var name = child["name"]
		var cls  = child["class"]
		var full = parent_path + "/" + name
		if cls == 2:
			if full.ends_with("/Value"):
				_candidate_paths.append(full)
		elif cls == 1:
			_recurse_main(full, full, depth + 1)


func _phase0a_enumerate() -> void:
	print("=== Phase 0A: enumerate virtual/main /Value nodes ===")
	var t0 = Time.get_ticks_msec()
	_recurse_main(ROOT_PATH, ROOT_PATH, 0)
	var enum_ms = Time.get_ticks_msec() - t0
	print("Found %d /Value candidates under %s in %d ms\n" % [
		_candidate_paths.size(), ROOT_PATH, enum_ms])


func _phase0b_build_vars() -> void:
	print("=== Phase 0B: batch read Value/HL/LL, build vars[] ===")
	if _candidate_paths.is_empty():
		return

	# Fire all async reads upfront (inflight dedup coalesces identical paths)
	var hl_paths: Array = []
	var ll_paths: Array = []
	for vp in _candidate_paths:
		var hlp = vp.replace("/Value", "/HL")
		var llp = vp.replace("/Value", "/LL")
		hl_paths.append(hlp)
		ll_paths.append(llp)
		vm.readValueAsync(vp)
		vm.readValueAsync(hlp)
		vm.readValueAsync(llp)

	# Fence: wait for at least the first /Value result to drain the batch
	vm.readValue(_candidate_paths[0])

	var t0 = Time.get_ticks_msec()

	# Drain all results (served from inflight cache after fence)
	for i in range(_candidate_paths.size()):
		var vp  = _candidate_paths[i]
		var val = vm.readValue(vp)

		if val == null:
			skipped.append(vp)
			continue

		var vtype = typeof(val)

		if vtype == TYPE_BOOL:
			vars.append({
				"val_path":    vp,
				"type":        TYPE_BOOL,
				"hl":          null,
				"ll":          null,
				"eff_hl":      1.0,
				"eff_ll":      0.0,
				"write_count": 0,
			})
			continue

		if vtype != TYPE_INT and vtype != TYPE_FLOAT:
			skipped.append(vp)
			continue

		# INT or FLOAT — use server-computed HL/LL directly
		var hl = vm.readValue(hl_paths[i])
		var ll = vm.readValue(ll_paths[i])

		if hl == null or ll == null:
			skipped.append(vp)
			continue

		var eff_hl = float(hl)
		var eff_ll = float(ll)
		var step   = (eff_hl - eff_ll) / 9.0 if eff_hl != eff_ll else 1.0

		vars.append({
			"val_path":    vp,
			"type":        vtype,
			"hl":          hl,
			"ll":          ll,
			"eff_hl":      eff_hl,
			"eff_ll":      eff_ll,
			"step":        step,
			"step_idx":    0,
			"write_count": 0,
		})

	var build_ms  = Time.get_ticks_msec() - t0
	var bool_cnt  = 0
	var int_cnt   = 0
	var float_cnt = 0
	for v in vars:
		match v.type:
			TYPE_BOOL:  bool_cnt  += 1
			TYPE_INT:   int_cnt   += 1
			TYPE_FLOAT: float_cnt += 1

	print("Active: %d vars (BOOL=%d INT=%d FLOAT=%d)  Skipped: %d  build_ms: %d\n" % [
		vars.size(), bool_cnt, int_cnt, float_cnt, skipped.size(), build_ms])


func _make_linear_vals(step_idx: int) -> Array:
	var vals: Array = []
	for v in vars:
		match v.type:
			TYPE_BOOL:
				vals.append(step_idx % 2 == 1)
			TYPE_INT:
				var vf = v.eff_ll + v.step * step_idx
				vals.append(int(round(vf)))
			TYPE_FLOAT:
				vals.append(v.eff_ll + v.step * step_idx)
			_:
				vals.append(0)
	return vals


func _make_random_vals() -> Array:
	var vals: Array = []
	for v in vars:
		match v.type:
			TYPE_BOOL:
				vals.append(randi() % 2 == 1)
			TYPE_INT:
				var range_i = int(v.eff_hl - v.eff_ll)
				if range_i <= 0:
					vals.append(int(v.eff_ll))
				else:
					vals.append(int(v.eff_ll) + randi() % (range_i + 1))
			TYPE_FLOAT:
				vals.append(randf_range(v.eff_ll, v.eff_hl))
			_:
				vals.append(0)
	return vals


func _phase1_benchmark() -> Dictionary:
	print("=== Phase 1: 10-round linear benchmark (sync vs async) ===")
	var n = vars.size()
	var val_paths: Array = []
	for v in vars:
		val_paths.append(v.val_path)

	var results: Array = []
	var total_sync_ws := 0.0

	print("  Round | Vars | sync_ms | sync_w/s | async_ms | async_w/s | speedup")
	print("  ------+------+---------+----------+----------+-----------+--------")

	for i in range(BENCH_ROUNDS):
		var vals = _make_linear_vals(i)

		# 1A: synchronous batch write
		var t0  = Time.get_ticks_msec()
		var sc  = vm.writeValues(val_paths, vals)
		var sync_ms = Time.get_ticks_msec() - t0
		var sync_ws = float(n) / (float(sync_ms) / 1000.0) if sync_ms > 0 else 0.0

		# 1B: async batch write + fence read
		vm.writeValuesAsync(val_paths, vals)
		var ta = Time.get_ticks_msec()
		vm.readValue(val_paths[0])   # fence: blocks until async write drains
		var async_ms = Time.get_ticks_msec() - ta
		var async_ws = float(n) / (float(async_ms) / 1000.0) if async_ms > 0 else 0.0

		var speedup = async_ws / sync_ws if sync_ws > 0.0 else 0.0
		print("  %5d | %4d | %7d | %8.1f | %8d | %9.1f | %.2fx  sc=%d" % [
			i, n, sync_ms, sync_ws, async_ms, async_ws, speedup, sc])

		results.append({
			"round":    i,
			"sync_ms":  sync_ms,
			"sync_ws":  sync_ws,
			"async_ms": async_ms,
			"async_ws": async_ws,
			"sc":       sc,
		})
		total_sync_ws += sync_ws

	var avg_sync_ws = total_sync_ws / BENCH_ROUNDS
	print("  Avg sync writes/sec: %.1f\n" % avg_sync_ws)

	return {"results": results, "avg_sync_ws": avg_sync_ws}


func _phase2_simulation() -> Dictionary:
	print("=== Phase 2: %d-second random simulation ===" % SIM_SECONDS)
	var n = vars.size()
	var val_paths: Array = []
	for v in vars:
		val_paths.append(v.val_path)

	var deadline     = Time.get_ticks_msec() + SIM_SECONDS * 1000
	var round_count  := 0
	var total_writes := 0
	var drain_ms_list: Array = []
	var t_start      = Time.get_ticks_msec()

	while Time.get_ticks_msec() < deadline:
		var vals = _make_random_vals()
		vm.writeValuesAsync(val_paths, vals)
		var t0 = Time.get_ticks_msec()
		vm.readValue(val_paths[0])   # fence: blocks until async write drains
		drain_ms_list.append(Time.get_ticks_msec() - t0)
		round_count  += 1
		total_writes += n

	var actual_sec     = float(Time.get_ticks_msec() - t_start) / 1000.0
	var writes_per_sec = float(total_writes) / actual_sec if actual_sec > 0.0 else 0.0

	drain_ms_list.sort()
	var cnt       = drain_ms_list.size()
	var p50       = drain_ms_list[int(cnt * 0.50)] if cnt > 0 else 0
	var p95       = drain_ms_list[maxi(0, int(cnt * 0.95))] if cnt > 0 else 0
	var drain_max = drain_ms_list[cnt - 1] if cnt > 0 else 0

	print("  Rounds: %d | Total writes: %d | writes/sec: %.1f" % [
		round_count, total_writes, writes_per_sec])
	print("  drain P50=%d ms  P95=%d ms  max=%d ms\n" % [p50, p95, drain_max])

	return {
		"round_count":    round_count,
		"total_writes":   total_writes,
		"writes_per_sec": writes_per_sec,
		"drain_p50":      p50,
		"drain_p95":      p95,
		"drain_max":      drain_max,
		"actual_sec":     actual_sec,
	}


func _phase3_report(ph1: Dictionary, ph2: Dictionary) -> void:
	print("=== Phase 3: generating Markdown report ===")

	var date_str    = Time.get_date_string_from_system()
	var time_str    = Time.get_time_string_from_system()
	var exe_dir     = OS.get_executable_path().get_base_dir()
	var report_path = exe_dir.path_join(
		"../modules/varmanager/test/test_write_sim_report_" + date_str + ".md")

	var lines: Array = []

	# --- Header ---
	lines.append("# Write Simulation Report")
	lines.append("")
	lines.append("**Date:** " + date_str + " " + time_str)
	lines.append("**Server:** " + SERVER_URL)
	lines.append("**Variables:** %d active, %d skipped" % [vars.size(), skipped.size()])
	lines.append("")

	# --- Variable Inventory ---
	lines.append("## Variable Inventory")
	lines.append("")
	lines.append("| Path | Type | LL | HL | Eff Range | Step |")
	lines.append("|------|------|----|----|-----------|------|")
	for v in vars:
		var tname: String
		match v.type:
			TYPE_BOOL:  tname = "BOOL"
			TYPE_INT:   tname = "INT"
			TYPE_FLOAT: tname = "FLOAT"
			_:          tname = "?"
		if v.type == TYPE_BOOL:
			lines.append("| %s | %s | — | — | [false, true] | — |" % [v.val_path, tname])
		else:
			var s_ell: String = str(v.eff_ll)
			var s_ehl: String = str(v.eff_hl)
			var s_stp: String = str(v.step)
			var row: String = "| " + str(v.val_path) + " | " + tname + " | " + str(v.ll) + " | " + str(v.hl) + " | [" + s_ell + ", " + s_ehl + "] | " + s_stp + " |"
			lines.append(row)
	if not skipped.is_empty():
		lines.append("")
		lines.append("### Skipped Variables")
		lines.append("")
		for sp in skipped:
			lines.append("- `%s` (offline or HL/LL null)" % sp)
	lines.append("")

	# --- Phase 1 Table ---
	lines.append("## Phase 1: Benchmark (10 Rounds, Sync vs Async)")
	lines.append("")
	lines.append("| Round | Vars | sync_ms | sync_w/s | async_ms | async_w/s | speedup | sc |")
	lines.append("|-------|------|---------|----------|----------|-----------|---------|----|")
	if ph1.has("results"):
		for r in ph1.results:
			var sp = r.async_ws / r.sync_ws if r.sync_ws > 0.0 else 0.0
			lines.append("| %d | %d | %d | %.1f | %d | %.1f | %.2f× | %d |" % [
				r.round, vars.size(), r.sync_ms, r.sync_ws,
				r.async_ms, r.async_ws, sp, r.sc])
	lines.append("")
	if ph1.has("avg_sync_ws"):
		lines.append("**Avg sync writes/sec:** %.1f" % ph1.avg_sync_ws)
	lines.append("")

	# --- Phase 2 KPI ---
	lines.append("## Phase 2: 30-Second Simulation KPI")
	lines.append("")
	lines.append("| KPI | Value |")
	lines.append("|-----|-------|")
	if not ph2.is_empty():
		lines.append("| Total rounds | %d |" % ph2.round_count)
		lines.append("| Total writes | %d |" % ph2.total_writes)
		lines.append("| writes/sec | %.1f |" % ph2.writes_per_sec)
		lines.append("| Actual duration | %.1f s |" % ph2.actual_sec)
		lines.append("| drain P50 | %d ms |" % ph2.drain_p50)
		lines.append("| drain P95 | %d ms |" % ph2.drain_p95)
		lines.append("| drain max | %d ms |" % ph2.drain_max)
	lines.append("")

	# --- Conclusions ---
	lines.append("## Conclusions")
	lines.append("")
	var p1_sync_ws = ph1.get("avg_sync_ws", 0.0)
	var p2_wps     = ph2.get("writes_per_sec", 0.0)
	var p95        = ph2.get("drain_p95", 0)
	if p2_wps > p1_sync_ws * 2.0:
		lines.append("- **批量聚合效应显著**: Phase 2 writes/sec (%.1f) > Phase 1 sync × 2 (%.1f)" % [
			p2_wps, p1_sync_ws * 2.0])
	else:
		lines.append("- 批量聚合效应不明显: Phase 2 %.1f writes/sec ≤ Phase 1 sync × 2 %.1f" % [
			p2_wps, p1_sync_ws * 2.0])
	if p95 > 1000:
		lines.append("- **队列背压警告**: drain P95 = %d ms > 1000 ms — 建议减小批量写入窗口或提高 run_once 频率" % p95)
	else:
		lines.append("- 队列背压正常: drain P95 = %d ms ≤ 1000 ms" % p95)
	lines.append("")

	# --- Write file ---
	var content = "\n".join(lines) + "\n"
	var f = FileAccess.open(report_path, FileAccess.WRITE)
	if f == null:
		print("WARN: cannot write report (err=%d) — printing to stdout" % FileAccess.get_open_error())
		print(content)
		return
	f.store_string(content)
	f.close()
	print("Report written: ", report_path)
