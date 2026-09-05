#!/usr/bin/env godot
# test_opcua.gd — Quick OPC-UA connectivity + concurrency smoke test
# Run: godot --headless --script modules/varmanager/test/test_opcua.gd
# or:  godot --headless --script res://modules/varmanager/test/test_opcua.gd

extends SceneTree

const SERVER_URL   = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH  = "/"
const VAR_PATH     = "virtual/D/D4000_general/Value"  # Value child node under D4000_general
const TIMEOUT_MS   = 2000

func _init():
	var vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable singleton not found")
		quit(1)
		return

	# ── T0: Connect ──────────────────────────────────────────────────────────
	print("=== T0: connect to ", SERVER_URL, " ===")
	vm.setReadTimeoutMs(TIMEOUT_MS)
	vm.setWriteTimeoutMs(TIMEOUT_MS)
	var sc = vm.connect_start(SERVER_URL, OBJECT_PATH, true)
	print("connect_start -> ", sc)
	if sc != 1 and sc != 2:   # ALL_OK=1  CONNECTED=2
		print("SKIP: cannot connect (sc=", sc, ")")
		quit(0)
		return
	print("PASS: connected")

	# ── T0b: NodeId probe ───────────────────────────────────────────────────
	print("\n=== T0b: getNodeId diagnostics ===")
	for p in ["virtual", "virtual/D", "virtual/D/D4000_general", "virtual/D/D4000_general/Value"]:
		var nid = vm.getNodeId(p)
		print("  getNodeId(", p, ") = ", nid)

	# ── T1: Single readValue ─────────────────────────────────────────────────
	print("\n=== T1: readValue(", VAR_PATH, ") ===")
	var v = vm.readValue(VAR_PATH)
	print("value = ", v, "  type = ", typeof(v))
	if typeof(v) == TYPE_NIL:
		print("NOTE: variable not found — T2–T4 will be skipped")

	# ── T2: Concurrent reads (dedup + batch) ─────────────────────────────────
	print("\n=== T2: 8 concurrent readValueAsync then readValue ===")
	var t0 = Time.get_ticks_msec()
	for i in range(8):
		vm.readValueAsync(VAR_PATH)
	var v2 = vm.readValue(VAR_PATH)   # sync — should dedup with the above
	var elapsed = Time.get_ticks_msec() - t0
	print("elapsed=%d ms  value=%s" % [elapsed, str(v2)])
	if elapsed < TIMEOUT_MS * 4:
		print("PASS: T2 completed within reasonable time")
	else:
		print("FAIL: T2 too slow (", elapsed, " ms)")

	# ── T3: Timeout without connection ───────────────────────────────────────
	print("\n=== T3: timeout_behavior ===")
	var vm2 = VarManager.new()
	vm2.setReadTimeoutMs(500)
	var t1 = Time.get_ticks_msec()
	var vt = vm2.readValue(VAR_PATH)
	var elapsed2 = Time.get_ticks_msec() - t1
	if typeof(vt) == TYPE_NIL and elapsed2 < 700:
		print("PASS: T3 timed out correctly in %d ms" % elapsed2)
	else:
		print("FAIL: T3 value=%s elapsed=%d ms" % [str(vt), elapsed2])

	# ── Done ─────────────────────────────────────────────────────────────────
	print("\n=== done ===")
	vm.connect_close()
	quit(0)
