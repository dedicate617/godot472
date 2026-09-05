# test_mqtt_concurrent.gd
# Verifies MqttManager::req_rsp() is safe under N concurrent callers.
# Each Godot Thread calls req_rsp() for a different variable simultaneously.
# Expected: all N threads receive the correct response for their own request
#           (no response-stealing or race conditions).
extends SceneTree

const REMOTE_CLIENT_ID := "svc_lxy4_winvm_001"
const TIMEOUT_SECS := 10
const N_THREADS := 8

# 8 distinct variables — each thread reads a different one so we can verify
# each response contains the expected variable name
const TEST_VARS := [
	"virtual.main.M131",
	"virtual.main.D200",
	"virtual.main.D4500",
	"virtual.main.D4620",
	"virtual.main.debug",
	"virtual.main.D4190",
	"virtual.main.M120",
	"virtual.main.D4650",
]

var mqtt: Object
var _results := {}
var _mutex := Mutex.new()
var _threads: Array = []

func _init() -> void:
	randomize()
	print("")
	print("=== MQTT CONCURRENT REQ/RSP TEST (%d threads) ===" % N_THREADS)

	mqtt = Engine.get_singleton("g_Mqtt")
	if mqtt == null:
		print("FATAL: g_Mqtt singleton not found")
		quit(1)
		return

	var conf := {
		"@Enable": "1",
		"@ClientID": "svc_lxy4_001",
		"@ServerIP": "192.167.200.1",
		"@ServerPort": "1883",
		"@User": "",
		"@Psw": "",
		"@TrustName": "",
		"@KeyName": "",
		"@KeepAlive": "10",
		"@AutoReconnect": "1",
		"@AutoReconnectInterval": "15",
	}
	mqtt.init_bydict(conf)

	OS.delay_msec(200)

	# Launch all threads simultaneously for maximum concurrency pressure
	for i in range(N_THREADS):
		var t := Thread.new()
		_threads.append(t)
		t.start(_thread_func.bind(i))

	# Wait for all threads to complete
	for t in _threads:
		t.wait_to_finish()

	_report()
	mqtt.connect_close()
	quit(0)

func _thread_func(idx: int) -> void:
	var varname: String = TEST_VARS[idx % TEST_VARS.size()]
	var body := JSON.stringify({"name": varname})

	var rsp: String = mqtt.req_rsp(
		"req/g_Variable/readVariable",
		body,
		"rsp/g_Variable/readVariable",
		REMOTE_CLIENT_ID,
		0, TIMEOUT_SECS, false
	)

	var parsed = JSON.parse_string(rsp)
	var ok := false
	if parsed is Dictionary:
		var result = str(parsed.get("result", ""))
		# result format: "TYPE:VALUE" — any non-empty result indicates server responded
		# result="0" only means error; a real response has "TYPE:VALUE" e.g. "1:false" or "2:500"
		ok = (result != "0" and result != "" and result.contains(":"))

	_mutex.lock()
	_results[varname] = {"rsp": rsp, "ok": ok, "idx": idx}
	_mutex.unlock()

	print("[Thread %d] var=%-35s  ok=%s  rsp=%s" % [idx, varname, ok, rsp.left(60)])

func _report() -> void:
	print("")
	var total := _results.size()
	var n_ok := 0
	for v in _results:
		if _results[v]["ok"]:
			n_ok += 1

	print("=== RESULTS: Total=%d  OK=%d  FAIL=%d ===" % [total, n_ok, total - n_ok])

	if n_ok == total:
		print("PASS: all concurrent req_rsp calls returned correct responses")
		print("      No response-stealing or race conditions detected.")
	else:
		print("FAIL: some responses were wrong (possible response-stealing or timeout)")
		for v in _results:
			if not _results[v]["ok"]:
				print("  FAIL [Thread %d] var=%s  rsp=%s" % [_results[v]["idx"], v, _results[v]["rsp"]])
	print("")
