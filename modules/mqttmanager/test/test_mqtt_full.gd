#!/usr/bin/env godot
# test_mqtt_full.gd — Complete MQTT read+write test for all write_sim inventory variables
# Variable inventory sourced from test_write_sim_report_2026-05-08.md
# Run: godot --headless --script modules/mqttmanager/test/test_mqtt_full.gd
extends SceneTree

const BROKER_HOST      = "192.167.200.1"
const BROKER_PORT      = 1883
const CLIENT_ID        = "perf_test_gd"
const REMOTE_CLIENT_ID = "svc_lxy4_winvm_001"
const TIMEOUT_SECS     = 5

const READ_ITERS  = 3   # iterations per variable for read test
const WRITE_ITERS = 3   # iterations per variable for write test

var mqtt

# Variable inventory: {name, type, eff_ll, eff_hl}
# Sourced from test_write_sim_report_2026-05-08.md (58 active variables)
var _vars: Array = []


func _build_vars() -> void:
	var B := TYPE_BOOL
	var I := TYPE_INT
	var F := TYPE_FLOAT
	var FM := 3.4028234663852886e+38   # FLT_MAX (unconstrained float range marker)
	_vars = [
		{"name": "virtual.main.M131",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4500_z4",         "type": I, "eff_ll": 1.0,           "eff_hl": 4.0},
		{"name": "virtual.main.debug4",           "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D2503",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.current.maxr",     "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.current.maxspeed", "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.current.maxxg",    "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.current.maxzno",   "type": I, "eff_ll": 1.0,           "eff_hl": 10.0},
		{"name": "virtual.main.D4620_25000",      "type": I, "eff_ll": 100.0,         "eff_hl": 65000.0},
		{"name": "virtual.main.LockSecs",         "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D4224",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D5090",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.M130_",            "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4500_general",    "type": I, "eff_ll": 1.0,           "eff_hl": 20.0},
		{"name": "virtual.main.debug3",           "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D200",             "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.zzlock",           "type": I, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.M190",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4620",            "type": I, "eff_ll": 100.0,         "eff_hl": 20000.0},
		{"name": "virtual.main.Lockmin",          "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D4190",            "type": F, "eff_ll": 1.0,           "eff_hl": 2.70000004768372},
		{"name": "virtual.main.D500",             "type": I, "eff_ll": 1.0,           "eff_hl": 3.0},
		{"name": "virtual.main.D4650",            "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
		{"name": "virtual.main.M130",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4500",            "type": I, "eff_ll": 1.0,           "eff_hl": 10.0},
		{"name": "virtual.main.debug2",           "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D136",             "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.toggle1s",         "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.M137",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4606",            "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
		{"name": "virtual.main.debugMode",        "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D4130",            "type": I, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4851",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D4632",            "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
		{"name": "virtual.main.M121",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4252",            "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.debug",            "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D1040",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.runtoggle1s",      "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.M135",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4602",            "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
		{"name": "virtual.main.debug6",           "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D30",              "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D4850",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D4620_general",    "type": I, "eff_ll": 100.0,         "eff_hl": 65000.0},
		{"name": "virtual.main.M120_",            "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4250",            "type": F, "eff_ll": -FM,           "eff_hl": FM},
		{"name": "virtual.main.D9998",            "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
		{"name": "virtual.main.D1020",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.M132",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4500_z9",         "type": I, "eff_ll": 1.0,           "eff_hl": 9.0},
		{"name": "virtual.main.debug5",           "type": I, "eff_ll": 0.0,           "eff_hl": 4294967295.0},
		{"name": "virtual.main.D2504",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D4620_8000",       "type": I, "eff_ll": 100.0,         "eff_hl": 16000.0},
		{"name": "virtual.main.M120",             "type": B, "eff_ll": 0.0,           "eff_hl": 1.0},
		{"name": "virtual.main.D4226",            "type": I, "eff_ll": -32768.0,      "eff_hl": 32767.0},
		{"name": "virtual.main.D5508",            "type": F, "eff_ll": 100.0,         "eff_hl": 20000.0},
		{"name": "virtual.main.D10000",           "type": I, "eff_ll": -2147483648.0, "eff_hl": 2147483647.0},
	]


func _type_name(t: int) -> String:
	match t:
		TYPE_BOOL:  return "BOOL"
		TYPE_INT:   return "INT"
		TYPE_FLOAT: return "FLOAT"
	return "?"


func _range_str(entry: Dictionary) -> String:
	if entry.type == TYPE_BOOL:
		return "[false, true]"
	var FM := 3.4028234663852886e+38
	if entry.eff_hl >= FM * 0.9:
		return "[FLT_MIN, FLT_MAX]"
	if entry.eff_ll <= -FM * 0.9:
		return "[FLT_MIN, FLT_MAX]"
	return "[%s, %s]" % [_fmt_num(entry.eff_ll), _fmt_num(entry.eff_hl)]


func _fmt_num(v: float) -> String:
	if abs(v) >= 1e9:
		return "%.0f" % v
	if v == float(int(v)):
		return str(int(v))
	return str(snappedf(v, 0.0001))


func _make_write_val(entry: Dictionary, iter: int) -> String:
	match entry.type:
		TYPE_BOOL:
			return "0" if iter % 2 == 0 else "1"
		TYPE_INT:
			# Full uint32 range [0, 4294967295]: int() overflows to -1, use randi() directly
			if entry.eff_hl >= 4294967295.0:
				return str(randi())
			var lo: int = int(entry.eff_ll)
			var hi: int = int(entry.eff_hl)
			return str(randi_range(lo, hi))
		TYPE_FLOAT:
			var lo: float = entry.eff_ll
			var hi: float = entry.eff_hl
			var FM := 3.4028234663852886e+38
			if hi - lo > 1e10:
				lo = -1000.0; hi = 1000.0
			return str(randf_range(lo, hi))
	return "0"


func _mean(arr: Array) -> float:
	if arr.is_empty():
		return 0.0
	var s := 0.0
	for v in arr:
		s += float(v)
	return s / float(arr.size())


func _init() -> void:
	_build_vars()

	mqtt = Engine.get_singleton("g_Mqtt")
	if mqtt == null:
		print("FAIL: g_Mqtt singleton not found")
		quit(1)
		return

	print("=== Phase 0: MQTT connect ===")
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
	var ok = mqtt.connect_start()
	if not ok:
		print("SKIP: cannot connect to MQTT broker %s:%d" % [BROKER_HOST, BROKER_PORT])
		quit(0)
		return
	print("MQTT connected  client=%s  remote=%s\n" % [CLIENT_ID, REMOTE_CLIENT_ID])

	var read_results  = _phase1_read_test()
	var write_results = _phase2_write_test()
	_phase3_report(read_results, write_results)

	mqtt.connect_close()
	quit(0)


func _phase1_read_test() -> Array:
	print("=== Phase 1: Read Test (%d vars × %d iters) ===" % [_vars.size(), READ_ITERS])
	var results: Array = []
	var ok_count := 0
	var err_count := 0

	for idx in range(_vars.size()):
		var entry = _vars[idx]
		var latencies: Array = []
		var last_result := ""
		var errors := 0

		for i in range(READ_ITERS):
			var t0 = Time.get_ticks_msec()
			var rsp = mqtt.req_rsp(
				"req/g_Variable/readVariable",
				JSON.stringify({"name": entry.name}),
				"rsp/g_Variable/readVariable",
				REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
			latencies.append(Time.get_ticks_msec() - t0)
			var parsed = JSON.parse_string(rsp)
			if parsed != null and parsed.has("result"):
				last_result = str(parsed["result"])
			else:
				errors += 1
				last_result = "ERR"

		var avg_ms = _mean(latencies)
		var status = "OK" if errors < READ_ITERS else "ERR"
		if status == "OK":
			ok_count += 1
		else:
			err_count += 1

		results.append({
			"name":        entry.name,
			"type":        entry.type,
			"avg_ms":      avg_ms,
			"last_result": last_result,
			"errors":      errors,
			"status":      status,
		})
		print("  [%2d/%d] %-40s  avg=%.1f ms  result=%-16s  %s" % [
			idx + 1, _vars.size(), entry.name, avg_ms, last_result, status])

	print("  Read summary: OK=%d  ERR=%d\n" % [ok_count, err_count])
	return results


func _phase2_write_test() -> Array:
	print("=== Phase 2: Write Simulation (%d vars × %d iters) ===" % [_vars.size(), WRITE_ITERS])
	var results: Array = []
	var ok_count  := 0
	var err_count := 0

	for idx in range(_vars.size()):
		var entry = _vars[idx]
		var latencies: Array = []
		var last_result := ""
		var last_val    := ""
		var errors      := 0

		for i in range(WRITE_ITERS):
			var val = _make_write_val(entry, i)
			var t0  = Time.get_ticks_msec()
			var rsp = mqtt.req_rsp(
				"req/g_Variable/writeVariable",
				JSON.stringify({"name": entry.name, "value": val}),
				"rsp/g_Variable/writeVariable",
				REMOTE_CLIENT_ID, 0, TIMEOUT_SECS, false)
			latencies.append(Time.get_ticks_msec() - t0)
			last_val = val
			var parsed = JSON.parse_string(rsp)
			if parsed != null and parsed.has("result"):
				last_result = str(parsed["result"])
			else:
				errors += 1
				last_result = "ERR"

		var avg_ms = _mean(latencies)
		var status = "OK" if last_result == "1" else ("ERR" if errors > 0 else "FAIL")
		if status == "OK":
			ok_count += 1
		else:
			err_count += 1

		results.append({
			"name":        entry.name,
			"type":        entry.type,
			"avg_ms":      avg_ms,
			"last_val":    last_val,
			"last_result": last_result,
			"errors":      errors,
			"status":      status,
		})
		print("  [%2d/%d] %-40s  val=%-14s  avg=%.1f ms  result=%-4s  %s" % [
			idx + 1, _vars.size(), entry.name, last_val, avg_ms, last_result, status])

	print("  Write summary: OK=%d  ERR/FAIL=%d\n" % [ok_count, err_count])
	return results


func _phase3_report(read_results: Array, write_results: Array) -> void:
	print("=== Phase 3: Generating Markdown report ===")

	var date_str = Time.get_date_string_from_system()
	var time_str = Time.get_time_string_from_system()
	var exe_dir  = OS.get_executable_path().get_base_dir()
	var report_path = exe_dir.path_join(
		"../modules/mqttmanager/test/test_mqtt_full_report_" + date_str + ".md")

	var L: Array = []

	# Header
	L.append("# MQTT Full Read/Write Test Report")
	L.append("")
	L.append("**Date:** " + date_str + " " + time_str)
	L.append("**Broker:** " + BROKER_HOST + ":" + str(BROKER_PORT))
	L.append("**Local ClientID:** " + CLIENT_ID)
	L.append("**Remote ClientID:** " + REMOTE_CLIENT_ID)
	L.append("**Variables:** %d (sourced from test_write_sim_report_2026-05-08.md)" % _vars.size())
	L.append("**READ_ITERS:** %d  |  **WRITE_ITERS:** %d" % [READ_ITERS, WRITE_ITERS])
	L.append("")

	# Variable Inventory
	L.append("## Variable Inventory")
	L.append("")
	L.append("| # | Variable | Type | Range |")
	L.append("|---|----------|------|-------|")
	for i in range(_vars.size()):
		var e = _vars[i]
		L.append("| %d | %s | %s | %s |" % [i + 1, e.name, _type_name(e.type), _range_str(e)])
	L.append("")

	# Phase 1: Read Results
	L.append("## Phase 1: Read Test Results")
	L.append("")
	var read_ok  := 0
	var read_err := 0
	var read_latencies: Array = []
	for r in read_results:
		if r.status == "OK": read_ok += 1
		else: read_err += 1
		read_latencies.append(r.avg_ms)

	L.append("**Summary:** Total=%d  OK=%d  ERR=%d  |  " % [
		read_results.size(), read_ok, read_err] +
		"avg latency=%.1f ms" % _mean(read_latencies))
	L.append("")
	L.append("| # | Variable | Type | avg_ms | Latest Value | Status |")
	L.append("|---|----------|------|--------|--------------|--------|")
	for i in range(read_results.size()):
		var r = read_results[i]
		var e = _vars[i]
		var val_disp = r.last_result if r.last_result.length() <= 20 else r.last_result.substr(0, 17) + "..."
		L.append("| %d | %s | %s | %.1f | %s | %s |" % [
			i + 1, r.name, _type_name(e.type), r.avg_ms, val_disp, r.status])
	L.append("")

	# Phase 2: Write Results
	L.append("## Phase 2: Write Simulation Results")
	L.append("")
	var write_ok   := 0
	var write_fail := 0
	var write_err  := 0
	var write_latencies: Array = []
	for r in write_results:
		match r.status:
			"OK":   write_ok   += 1
			"FAIL": write_fail += 1
			"ERR":  write_err  += 1
		write_latencies.append(r.avg_ms)

	L.append("**Summary:** Total=%d  OK=%d  FAIL=%d  ERR=%d  |  " % [
		write_results.size(), write_ok, write_fail, write_err] +
		"avg latency=%.1f ms" % _mean(write_latencies))
	L.append("")
	L.append("| # | Variable | Type | Range | Last Written Value | avg_ms | Result | Status |")
	L.append("|---|----------|------|-------|--------------------|--------|--------|--------|")
	for i in range(write_results.size()):
		var r = write_results[i]
		var e = _vars[i]
		var val_disp = r.last_val if r.last_val.length() <= 16 else r.last_val.substr(0, 13) + "..."
		L.append("| %d | %s | %s | %s | %s | %.1f | %s | %s |" % [
			i + 1, r.name, _type_name(e.type), _range_str(e), val_disp,
			r.avg_ms, r.last_result, r.status])
	L.append("")

	# Summary Statistics
	L.append("## Summary Statistics")
	L.append("")
	L.append("| Metric | Value |")
	L.append("|--------|-------|")
	L.append("| Total variables tested | %d |" % _vars.size())
	L.append("| Read OK | %d / %d |" % [read_ok, read_results.size()])
	L.append("| Read ERR | %d / %d |" % [read_err, read_results.size()])
	L.append("| Write OK (result=1) | %d / %d |" % [write_ok, write_results.size()])
	L.append("| Write FAIL (result=0) | %d / %d |" % [write_fail, write_results.size()])
	L.append("| Write ERR (no response) | %d / %d |" % [write_err, write_results.size()])
	L.append("| Avg read latency | %.1f ms |" % _mean(read_latencies))
	L.append("| Avg write latency | %.1f ms |" % _mean(write_latencies))
	L.append("")

	# Conclusions
	L.append("## Conclusions")
	L.append("")
	var read_ok_pct  = 100.0 * float(read_ok)  / float(read_results.size())
	var write_ok_pct = 100.0 * float(write_ok) / float(write_results.size())
	var read_avg     = _mean(read_latencies)
	var write_avg    = _mean(write_latencies)
	var has_conclusion := false

	if read_ok_pct >= 90.0:
		L.append("- **读测试覆盖率高**: %.0f%% 变量响应正常 (%d/%d)" % [
			read_ok_pct, read_ok, read_results.size()])
		has_conclusion = true
	elif read_ok_pct >= 50.0:
		L.append("- **读测试覆盖率一般**: %.0f%% 变量响应正常，%.0f%% 无响应" % [
			read_ok_pct, 100.0 - read_ok_pct])
		has_conclusion = true
	else:
		L.append("- **读测试覆盖率低**: 仅 %.0f%% 变量响应 — 建议检查远端服务 %s 是否在线" % [
			read_ok_pct, REMOTE_CLIENT_ID])
		has_conclusion = true

	if write_ok_pct >= 90.0:
		L.append("- **写入仿真成功**: %.0f%% 变量写入返回 result=1" % write_ok_pct)
		has_conclusion = true
	elif write_ok_pct > 0.0:
		L.append("- **写入仿真部分成功**: %.0f%% 变量写入成功 (result=1)，%.0f%% 失败 (result=0)" % [
			write_ok_pct, 100.0 * float(write_fail) / float(write_results.size())])
		has_conclusion = true
	else:
		L.append("- **写入仿真全部失败**: result 均为 0 或 ERR — 远端服务可能未处理 writeVariable 请求，或变量名格式不匹配")
		has_conclusion = true

	if read_avg > 2000.0:
		L.append("- **读延迟过高**: avg=%.0f ms > 2000 ms — 建议检查 broker 网络" % read_avg)
		has_conclusion = true
	elif read_avg < 10.0:
		L.append("- **读延迟极低 (%.1f ms)**: 可能为 broker 直接回包（远端服务未订阅），或本地服务极快响应" % read_avg)
		has_conclusion = true

	if write_avg > 2000.0:
		L.append("- **写延迟过高**: avg=%.0f ms > 2000 ms" % write_avg)
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
