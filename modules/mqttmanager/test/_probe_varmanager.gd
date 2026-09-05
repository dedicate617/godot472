#!/usr/bin/env godot
# Probe available g_Variable methods in the compiled binary
extends SceneTree

func _init() -> void:
	var vm = Engine.get_singleton("g_Variable")
	if vm == null:
		print("FAIL: g_Variable not found")
		quit(1)
		return
	var methods = vm.get_method_list()
	var names: Array = []
	for m in methods:
		names.append(m["name"])
	names.sort()
	print("=== g_Variable methods (%d total) ===" % names.size())
	for n in names:
		if not n.begins_with("_"):
			print("  ", n)
	quit(0)
