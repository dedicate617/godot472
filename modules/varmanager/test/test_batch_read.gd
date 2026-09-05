#!/usr/bin/env godot
# test_batch_read.gd — Enumerate all OPC-UA variables then batch-read /Value nodes
# Run: godot --headless --script modules/varmanager/test/test_batch_read.gd

extends SceneTree

const SERVER_URL  = "opc.tcp://127.0.0.1:4840/"
const OBJECT_PATH = "/"
const ROOT_PATH   = ""       # browse from ObjectsFolder root
const MAX_DEPTH   = 5        # safety guard against deep trees
const TIMEOUT_MS  = 3000
const SAMPLE_SIZE = 10       # number of vars to sync-read in Phase 3 display

var vm
var all_vars: Array = []     # all Variable node paths
var value_vars: Array = []   # paths ending in /Value

func _init():
	vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable singleton not found")
		quit(1)
		return

	vm.setReadTimeoutMs(TIMEOUT_MS)
	var sc = vm.connect_start(SERVER_URL, OBJECT_PATH, true)
	print("connect_start -> ", sc)
	if sc != 1 and sc != 2:
		print("SKIP: cannot connect (sc=", sc, ")")
		quit(0)
		return
	print("Connected OK\n")

	# --- Phase 1: enumerate all Variable nodes recursively ---
	print("=== Phase 1: enumerating variable paths ===")
	var t_enum0 = Time.get_ticks_msec()
	_recurse("", ROOT_PATH, 0)
	var t_enum1 = Time.get_ticks_msec()

	# Filter /Value leaf nodes for the batch test
	for p in all_vars:
		if p.ends_with("/Value"):
			value_vars.append(p)

	print("Found %d total variables in %d ms" % [all_vars.size(), t_enum1 - t_enum0])
	print("  of which %d end in /Value (batch read targets)\n" % value_vars.size())

	if value_vars.is_empty():
		print("NOTE: no /Value nodes found, trying all vars")
		value_vars = all_vars.duplicate()

	if value_vars.is_empty():
		print("NOTE: no variables found, nothing to read")
		vm.connect_close()
		quit(0)
		return

	# --- Phase 2: batch async read all /Value nodes, then one sync read ---
	print("=== Phase 2: batch readValueAsync (%d paths) ===" % value_vars.size())
	var t0 = Time.get_ticks_msec()
	for p in value_vars:
		vm.readValueAsync(p)
	# Sync read the first var to wait for at least one to complete
	var first_val = vm.readValue(value_vars[0])
	var elapsed = Time.get_ticks_msec() - t0
	print("Batch elapsed: %d ms  (first_val=%s)" % [elapsed, str(first_val)])
	# For large batches, allow up to 2s per item is unreasonable. Allow up to 30s or timeout*3.
	var max_allowed = max(TIMEOUT_MS * 3, value_vars.size() * 50)
	if elapsed < max_allowed:
		print("PASS: batch completed within limit (%d ms)" % max_allowed)
	else:
		print("FAIL: batch too slow (%d ms > %d ms limit)" % [elapsed, max_allowed])

	# --- Phase 3: sync read a sample and display values ---
	print("\n=== Phase 3: sync readValue sample (%d of %d) ===" % [
		min(SAMPLE_SIZE, value_vars.size()), value_vars.size()])
	var sample = value_vars.slice(0, min(SAMPLE_SIZE, value_vars.size()))
	for p in sample:
		var v = vm.readValue(p)
		print("  %-60s = %-12s (type=%d)" % [p, str(v), typeof(v)])

	print("\n=== done ===")
	vm.connect_close()
	quit(0)


func _recurse(parent_path: String, browse_path: String, depth: int):
	if depth > MAX_DEPTH:
		return
	var children = vm.getChildrenOfPath(browse_path)
	for child in children:
		var name   = child["name"]
		var cls    = child["class"]
		var full   = (parent_path + "/" + name) if parent_path != "" else name
		# UA_NODECLASS_OBJECT=1, UA_NODECLASS_VARIABLE=2
		if cls == 2:
			all_vars.append(full)
		elif cls == 1:
			_recurse(full, full, depth + 1)
