#!/usr/bin/env godot
# test_mqtt_perf.gd — MQTT g_Variable req/rsp performance benchmark
# Run: godot --headless --script modules/mqttmanager/test/test_mqtt_perf.gd

extends SceneTree

# OPC-UA enumeration
const SERVER_URL  = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH = "/"
const ROOT_PATH   = "virtual/main"
const MAX_DEPTH   = 3
const TIMEOUT_MS  = 3000

# MQTT broker + identity
const BROKER_HOST      = "192.167.200.1"
const BROKER_PORT      = 1883
const CLIENT_ID        = "perf_test_gd"
const REMOTE_CLIENT_ID = "svc_lxy4_winvm_001"
const TIMEOUT_SECS     = 5

# Benchmark iterations
const HIST_ITERS  = 30
const BATCH_ITERS = 10
const SAMPLE_SIZE = 5

var vm    # g_Variable singleton
var mqtt  # g_Mqtt singleton

var _candidate_paths: Array = []
# vars[] entry schema: {val_path, mqtt_name, type, eff_hl, eff_ll}
var vars: Array    = []
var skipped: Array = []


func _init() -> void:
	vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable singleton not found")
		quit(1)
		return

	mqtt = Engine.get_singleton("g_Mqtt")
	if mqtt == null:
		print("FAIL: g_Mqtt singleton not found")
		quit(1)
		return

	_phase0a_enumerate()
	if vars.is_empty():
		print("SKIP: no active variables after Phase 0A")
		vm.connect_close()
		quit(0)
		return

	if not _phase0b_mqtt_connect():
		print("SKIP: cannot connect to MQTT broker")
		vm.connect_close()
		mqtt.connect_close()
		quit(0)
		return

	var mb1 = _mb1_read_rtt()
	var mb2 = _mb2_write_rtt()
	var mb3 = _mb3_batch_sweep()
	var mb4 = _mb4_sig_overhead()
	_phase5_report(mb1, mb2, mb3, mb4)

	vm.connect_close()
	mqtt.connect_close()
	quit(0)


func _recurse_main(parent_path: String, browse_path: String, depth: int) -> void:
	if depth > MAX_DEPTH:
		return
	var children = vm.getChildrenOfPath(browse_path)
	for child in children:
		var cname = child["name"]
		var cls   = child["class"]
		var full  = parent_path + "/" + cname
		if cls == 2:
			if full.ends_with("/Value"):
				_candidate_paths.append(full)
		elif cls == 1:
			_recurse_main(full, full, depth + 1)


func _phase0a_build_vars() -> void:
	if _candidate_paths.is_empty():
		return

	for vp in _candidate_paths:
		var val = vm.readValue(vp)
		if val == null:
			skipped.append(vp)
			continue

		var vtype = typeof(val)

		if vtype == TYPE_BOOL:
			var mqtt_name = vp.replace("/Value", "").replace("/", ".")
			vars.append({
				"val_path":  vp,
				"mqtt_name": mqtt_name,
				"type":      TYPE_BOOL,
				"eff_hl":    1.0,
				"eff_ll":    0.0,
			})
			continue

		if vtype != TYPE_INT and vtype != TYPE_FLOAT:
			skipped.append(vp)
			continue

		var hlp = vp.replace("/Value", "/HL")
		var llp = vp.replace("/Value", "/LL")
		var hl  = vm.readValue(hlp)
		var ll  = vm.readValue(llp)
		if hl == null or ll == null:
			skipped.append(vp)
			continue

		var eff_hl = float(hl)
		var eff_ll = float(ll)
		if eff_hl <= eff_ll:
			eff_hl = eff_ll + 100.0

		var mqtt_name = vp.replace("/Value", "").replace("/", ".")
		vars.append({
			"val_path":  vp,
			"mqtt_name": mqtt_name,
			"type":      vtype,
			"eff_hl":    eff_hl,
			"eff_ll":    eff_ll,
		})


func _phase0a_enumerate() -> void:
	print("=== Phase 0A: enumerate + build vars ===")
	vm.setReadTimeoutMs(TIMEOUT_MS)
	var sc = vm.connect_start(SERVER_URL, OBJECT_PATH, true)
	print("g_Variable connect_start -> ", sc)
	if sc != 1 and sc != 2:
		print("SKIP: cannot connect to OPC-UA server (sc=", sc, ")")
		return

	var t0 = Time.get_ticks_msec()
	_recurse_main(ROOT_PATH, ROOT_PATH, 0)
	print("Found %d /Value candidates in %d ms" % [
		_candidate_paths.size(), Time.get_ticks_msec() - t0])

	_phase0a_build_vars()

	var bool_cnt := 0
	var int_cnt  := 0
	var flt_cnt  := 0
	for v in vars:
		match v.type:
			TYPE_BOOL:  bool_cnt += 1
			TYPE_INT:   int_cnt  += 1
			TYPE_FLOAT: flt_cnt  += 1
	print("Active: %d vars (BOOL=%d INT=%d FLOAT=%d)  Skipped: %d\n" % [
		vars.size(), bool_cnt, int_cnt, flt_cnt, skipped.size()])


func _phase0b_mqtt_connect() -> bool:
	print("=== Phase 0B: MQTT connect ===")
	var conf = {
		"@Enable":                "1",
		"@ClientID":              CLIENT_ID,
		"@ServerIP":              BROKER_HOST,
		"@ServerPort":            str(BROKER_PORT),
		"@User":                  "",
		"@Psw":                   "",
		"@TrustName":             "",
		"@KeyName":               "",
		"@KeepAlive":             "10",
		"@AutoReconnect":         "1",
		"@AutoReconnectInterval": "15",
	}
	mqtt.init_bydict(conf)
	var ok1 = mqtt.connect_start()
	if not ok1:
		print("connect_start FAILED")
		return false
	print("MQTT connected OK  client=%s  remote=%s\n" % [CLIENT_ID, REMOTE_CLIENT_ID])
	return true


# Returns a random value string for `entry` suitable for writeVariable.
# iter used for BOOL alternation only.
func _make_random_val(entry: Dictionary, iter: int) -> String:
	match entry.type:
		TYPE_BOOL:
			return "0" if iter % 2 == 0 else "1"
		TYPE_INT:
			var range_i = int(entry.eff_hl - entry.eff_ll)
			if range_i <= 0:
				return str(int(entry.eff_ll))
			return str(int(entry.eff_ll) + randi() % (range_i + 1))
		TYPE_FLOAT:
			return str(randf_range(entry.eff_ll, entry.eff_hl))
	return "0"


func _mean(arr: Array) -> float:
	if arr.is_empty():
		return 0.0
	var s := 0.0
	for v in arr:
		s += float(v)
	return s / float(arr.size())


# arr must be sorted ascending before calling.
func _percentile(sorted_arr: Array, p: float) -> float:
	if sorted_arr.is_empty():
		return 0.0
	var idx = clampi(int(float(sorted_arr.size()) * p), 0, sorted_arr.size() - 1)
	return float(sorted_arr[idx])


func _mb1_read_rtt() -> Dictionary:
	print("=== MB1: readVariable RTT (%d iters × %d vars) ===" % [HIST_ITERS, SAMPLE_SIZE])
	var sample = vars.slice(0, mini(SAMPLE_SIZE, vars.size()))
	var rows: Array = []

	print("  %-32s | %5s | %7s | %5s | %5s | %5s | %5s" % [
		"Variable", "min", "avg", "P50", "P95", "P99", "max"])
	print("  " + "-".repeat(82))

	for entry in sample:
		var latencies: Array = []
		var errors := 0
		for i in range(HIST_ITERS):
			var t0  = Time.get_ticks_msec()
			var rsp = mqtt.req_rsp(
				"req/g_Variable/readVariable",
				JSON.stringify({"name": entry.mqtt_name}),
				"rsp/g_Variable/readVariable",
				REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
			latencies.append(Time.get_ticks_msec() - t0)
			var parsed = JSON.parse_string(rsp)
			if parsed == null or not parsed.has("result"):
				errors += 1

		latencies.sort()
		var row = {
			"name":   entry.mqtt_name,
			"min":    latencies[0],
			"avg":    _mean(latencies),
			"p50":    _percentile(latencies, 0.50),
			"p95":    _percentile(latencies, 0.95),
			"p99":    _percentile(latencies, 0.99),
			"max":    latencies[-1],
			"errors": errors,
		}
		rows.append(row)
		var err_tag = (" ERR=%d" % errors) if errors > 0 else ""
		print("  %-32s | %5d | %7.1f | %5.0f | %5.0f | %5.0f | %5d%s" % [
			entry.mqtt_name, row.min, row.avg, row.p50, row.p95, row.p99, row.max, err_tag])

	print("")
	return {"rows": rows}


func _mb2_write_rtt() -> Dictionary:
	print("=== MB2: writeVariable RTT (%d iters × %d vars) ===" % [HIST_ITERS, SAMPLE_SIZE])
	var sample = vars.slice(0, mini(SAMPLE_SIZE, vars.size()))
	var rows: Array = []

	print("  %-32s | %5s | %7s | %5s | %5s | %5s | %5s | result" % [
		"Variable", "min", "avg", "P50", "P95", "P99", "max"])
	print("  " + "-".repeat(90))

	for entry in sample:
		var latencies: Array = []
		var last_result := ""
		var errors := 0
		for i in range(HIST_ITERS):
			var val = _make_random_val(entry, i)
			var t0  = Time.get_ticks_msec()
			var rsp = mqtt.req_rsp(
				"req/g_Variable/writeVariable",
				JSON.stringify({"name": entry.mqtt_name, "value": val}),
				"rsp/g_Variable/writeVariable",
				REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
			latencies.append(Time.get_ticks_msec() - t0)
			var parsed = JSON.parse_string(rsp)
			if parsed != null and parsed.has("result"):
				last_result = str(parsed["result"])
			else:
				errors += 1
				last_result = "ERR"

		latencies.sort()
		var row = {
			"name":        entry.mqtt_name,
			"min":         latencies[0],
			"avg":         _mean(latencies),
			"p50":         _percentile(latencies, 0.50),
			"p95":         _percentile(latencies, 0.95),
			"p99":         _percentile(latencies, 0.99),
			"max":         latencies[-1],
			"last_result": last_result,
			"errors":      errors,
		}
		rows.append(row)
		print("  %-32s | %5d | %7.1f | %5.0f | %5.0f | %5.0f | %5d | %s" % [
			entry.mqtt_name, row.min, row.avg, row.p50, row.p95, row.p99,
			row.max, last_result])

	print("")
	return {"rows": rows}


func _mb3_batch_sweep() -> Dictionary:
	print("=== MB3: writeVariables batch sweep (N=1/5/10/50, %d iters) ===" % BATCH_ITERS)
	var ns = [1, 5, 10, 50]
	var rows: Array = []

	print("  %4s | %7s | %5s | %5s | %10s | result" % ["N", "avg_ms", "P50", "P95", "writes/s"])
	print("  " + "-".repeat(57))

	for n in ns:
		var actual_n = mini(n, vars.size())
		if actual_n == 0:
			continue
		var entries_n = vars.slice(0, actual_n)
		var times: Array = []
		var last_result := ""
		var errors := 0

		for i in range(BATCH_ITERS):
			var names_n: Array = []
			var vals_n: Array  = []
			for entry in entries_n:
				names_n.append(entry.mqtt_name)
				vals_n.append(_make_random_val(entry, i))

			var t0  = Time.get_ticks_msec()
			var rsp = mqtt.req_rsp(
				"req/g_Variable/writeVariables",
				JSON.stringify({"names": names_n, "values": vals_n}),
				"rsp/g_Variable/writeVariables",
				REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
			times.append(Time.get_ticks_msec() - t0)
			var parsed = JSON.parse_string(rsp)
			if parsed != null and parsed.has("result"):
				last_result = str(parsed["result"])
			else:
				errors += 1
				last_result = "ERR"

		times.sort()
		var avg_ms       = _mean(times)
		var p50          = _percentile(times, 0.50)
		var p95          = _percentile(times, 0.95)
		var writes_per_s = float(actual_n) / (avg_ms / 1000.0) if avg_ms > 0.0 else 0.0

		var row = {
			"n":            actual_n,
			"avg_ms":       avg_ms,
			"p50":          p50,
			"p95":          p95,
			"writes_per_s": writes_per_s,
			"last_result":  last_result,
			"errors":       errors,
		}
		rows.append(row)
		print("  %4d | %7.1f | %5.0f | %5.0f | %10.1f | %s" % [
			actual_n, avg_ms, p50, p95, writes_per_s, last_result])

	print("")
	return {"rows": rows}


func _mb4_sig_overhead() -> Dictionary:
	print("=== MB4: writeVariableWithoutSig vs writeVariable (%d iters) ===" % BATCH_ITERS)
	if vars.is_empty():
		print("  SKIP: no vars")
		print("")
		return {}

	var entry = vars[0]
	var with_times: Array    = []
	var without_times: Array = []

	for i in range(BATCH_ITERS):
		var val = _make_random_val(entry, i)
		var t0  = Time.get_ticks_msec()
		mqtt.req_rsp(
			"req/g_Variable/writeVariable",
			JSON.stringify({"name": entry.mqtt_name, "value": val}),
			"rsp/g_Variable/writeVariable",
			REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
		with_times.append(Time.get_ticks_msec() - t0)

	for i in range(BATCH_ITERS):
		var val = _make_random_val(entry, i)
		var t0  = Time.get_ticks_msec()
		mqtt.req_rsp(
			"req/g_Variable/writeVariableWithoutSig",
			JSON.stringify({"name": entry.mqtt_name, "value": val}),
			"rsp/g_Variable/writeVariableWithoutSig",
			REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
		without_times.append(Time.get_ticks_msec() - t0)

	with_times.sort()
	without_times.sort()

	var with_p50    = _percentile(with_times, 0.50)
	var without_p50 = _percentile(without_times, 0.50)
	var overhead_ms = with_p50 - without_p50
	var overhead_pct = (overhead_ms / without_p50 * 100.0) if without_p50 > 0.0 else 0.0

	print("  var=%s" % entry.mqtt_name)
	print("  with_sig P50=%.0f ms | without_sig P50=%.0f ms | overhead=%.1f ms (%.1f%%)\n" % [
		with_p50, without_p50, overhead_ms, overhead_pct])

	return {
		"var_name":     entry.mqtt_name,
		"with_p50":     with_p50,
		"without_p50":  without_p50,
		"overhead_ms":  overhead_ms,
		"overhead_pct": overhead_pct,
	}


func _phase5_report(mb1: Dictionary, mb2: Dictionary, mb3: Dictionary, mb4: Dictionary) -> void:
	print("=== Phase 5: generating Markdown report ===")

	var date_str    = Time.get_date_string_from_system()
	var time_str    = Time.get_time_string_from_system()
	var exe_dir     = OS.get_executable_path().get_base_dir()
	var report_path = exe_dir.path_join(
		"../modules/mqttmanager/test/test_mqtt_perf_report_" + date_str + ".md")

	var L: Array = []

	L.append("# MQTT Performance Benchmark Report")
	L.append("")
	L.append("**Date:** " + date_str + " " + time_str)
	L.append("**Broker:** " + BROKER_HOST + ":" + str(BROKER_PORT))
	L.append("**Local ClientID:** " + CLIENT_ID)
	L.append("**Remote ClientID:** " + REMOTE_CLIENT_ID)
	L.append("**Enumerated vars:** %d active, %d skipped" % [vars.size(), skipped.size()])
	L.append("**SAMPLE_SIZE:** %d  |  HIST_ITERS: %d  |  BATCH_ITERS: %d" % [
		SAMPLE_SIZE, HIST_ITERS, BATCH_ITERS])
	L.append("")

	# MB1
	L.append("## MB1: readVariable RTT")
	L.append("")
	L.append("| Variable | min | avg | P50 | P95 | P99 | max (ms) |")
	L.append("|----------|-----|-----|-----|-----|-----|----------|")
	if mb1.has("rows"):
		for r in mb1.rows:
			L.append("| %s | %d | %.1f | %.0f | %.0f | %.0f | %d |" % [
				r.name, r.min, r.avg, r.p50, r.p95, r.p99, r.max])
	L.append("")

	# MB2
	L.append("## MB2: writeVariable RTT")
	L.append("")
	L.append("| Variable | min | avg | P50 | P95 | P99 | max (ms) | result |")
	L.append("|----------|-----|-----|-----|-----|-----|----------|--------|")
	if mb2.has("rows"):
		for r in mb2.rows:
			L.append("| %s | %d | %.1f | %.0f | %.0f | %.0f | %d | %s |" % [
				r.name, r.min, r.avg, r.p50, r.p95, r.p99, r.max, r.last_result])
	L.append("")

	# MB3
	L.append("## MB3: writeVariables Batch Sweep")
	L.append("")
	L.append("| N | avg_ms | P50 | P95 | writes/s | result |")
	L.append("|---|--------|-----|-----|----------|--------|")
	if mb3.has("rows"):
		for r in mb3.rows:
			L.append("| %d | %.1f | %.0f | %.0f | %.1f | %s |" % [
				r.n, r.avg_ms, r.p50, r.p95, r.writes_per_s, r.last_result])
	L.append("")

	# MB4
	L.append("## MB4: Signature Overhead")
	L.append("")
	L.append("| Variable | with_sig P50 | without_sig P50 | overhead_ms | overhead % |")
	L.append("|----------|-------------|-----------------|-------------|------------|")
	if not mb4.is_empty():
		L.append("| %s | %.0f ms | %.0f ms | %.1f | %.1f%% |" % [
			mb4.var_name, mb4.with_p50, mb4.without_p50, mb4.overhead_ms, mb4.overhead_pct])
	L.append("")

	# Conclusions
	L.append("## Conclusions")
	L.append("")
	var has_conclusion := false

	var mb1_avg_p95 := 0.0
	if mb1.has("rows") and not mb1.rows.is_empty():
		for r in mb1.rows:
			mb1_avg_p95 += r.p95
		mb1_avg_p95 /= float(mb1.rows.size())
	if mb1_avg_p95 > 2000.0:
		L.append("- **readVariable 延迟过高**: avg P95=%.0f ms > 2000 ms — 建议检查 broker 网络" % mb1_avg_p95)
		has_conclusion = true

	var mb2_avg_rtt := 0.0
	if mb2.has("rows") and not mb2.rows.is_empty():
		for r in mb2.rows:
			mb2_avg_rtt += r.avg
		mb2_avg_rtt /= float(mb2.rows.size())
		var mb2_single_wps = 1000.0 / mb2_avg_rtt if mb2_avg_rtt > 0.0 else 0.0
		if mb3.has("rows") and not mb3.rows.is_empty():
			var last_row = mb3.rows[-1]
			if last_row.writes_per_s > mb2_single_wps * 5.0:
				L.append("- **批量聚合效应显著**: MB3 N=%d writes/s (%.1f) > MB2 single × 5 (%.1f)" % [
					last_row.n, last_row.writes_per_s, mb2_single_wps * 5.0])
				has_conclusion = true

	if not mb4.is_empty() and mb4.overhead_ms > 100.0:
		L.append("- **签名校验开销显著**: overhead=%.1f ms > 100 ms" % mb4.overhead_ms)
		has_conclusion = true

	if not has_conclusion:
		L.append("- 所有指标在正常范围内")
	L.append("")

	var content = "\n".join(L) + "\n"
	var f = FileAccess.open(report_path, FileAccess.WRITE)
	if f == null:
		print("WARN: cannot write report (err=%d) — printing to stdout" % FileAccess.get_open_error())
		print(content)
		return
	f.store_string(content)
	f.close()
	print("Report written: ", report_path)
