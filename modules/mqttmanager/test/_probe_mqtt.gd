#!/usr/bin/env godot
extends SceneTree

func _init() -> void:
	var m = Engine.get_singleton("g_Mqtt")
	if m == null:
		print("FAIL: g_Mqtt not found")
		quit(1)
		return
	var methods = m.get_method_list()
	var names: Array = []
	for mm in methods:
		names.append(mm["name"])
	names.sort()
	print("=== g_Mqtt methods (%d total) ===" % names.size())
	for n in names:
		if not n.begins_with("_"):
			print("  ", n)
	quit(0)
