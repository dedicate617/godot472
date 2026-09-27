/* varmanager_web.cpp */

#include "varmanager.h"
#include "core/config/engine.h"
#include "core/os/os.h"
#include "core/io/json.h"
#include "scene/main/scene_tree.h"

VarManager *VarManager::singleton = nullptr;

VarManager *VarManager::get_singleton() {
	return singleton;
}

void VarManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("callback_varmanager_connect"));
	ADD_SIGNAL(MethodInfo("callback_varmanager_disconnect"));
	ADD_SIGNAL(MethodInfo("callback_varmanager_cache_ready", PropertyInfo(Variant::BOOL, "success")));
	ADD_SIGNAL(MethodInfo("varChanged", PropertyInfo(Variant::STRING, "varName"), PropertyInfo(Variant::OBJECT, "value")));
	ADD_SIGNAL(MethodInfo("varsChanged", PropertyInfo(Variant::ARRAY, "varNames"), PropertyInfo(Variant::ARRAY, "values")));
	ADD_SIGNAL(MethodInfo("eventTriggered", PropertyInfo(Variant::INT, "eventId"), PropertyInfo(Variant::DICTIONARY, "eventData")));
	ADD_SIGNAL(MethodInfo("varReadHisCallback", PropertyInfo(Variant::STRING, "varName"), PropertyInfo(Variant::ARRAY, "datas"), PropertyInfo(Variant::BOOL, "moreData")));
	ADD_SIGNAL(MethodInfo("methodInvoked", PropertyInfo(Variant::INT, "reqId"), PropertyInfo(Variant::ARRAY, "outputs")));

	ClassDB::bind_method(D_METHOD("setConnectAuthMode", "caMode"), &VarManager::setConnectAuthMode);
	ClassDB::bind_method(D_METHOD("getMqRefreshKVVarPath"), &VarManager::getMqRefreshKVVarPath);
	ClassDB::bind_method(D_METHOD("setUserPassword", "user", "password"), &VarManager::setUserPassword);
	ClassDB::bind_method(D_METHOD("connect_start", "address", "objectPath", "activateUpcalls"), &VarManager::connect_start, DEFVAL("opc.tcp://127.0.0.1:4840"), DEFVAL("Server"), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("connect_close"), &VarManager::connect_close);
	ClassDB::bind_method(D_METHOD("getStatus"), &VarManager::getStatus);
	ClassDB::bind_method(D_METHOD("getNodeId", "nodePath"), &VarManager::getNodeId);
	ClassDB::bind_method(D_METHOD("getChildrenOfPath", "nodePath"), &VarManager::getChildrenOfPath);
	ClassDB::bind_method(D_METHOD("readValue", "variablePath"), &VarManager::readValue);
	ClassDB::bind_method(D_METHOD("readNextValue", "variablePath"), &VarManager::readNextValue);
	ClassDB::bind_method(D_METHOD("readValueAsync", "variablePath"), &VarManager::readValueAsync);
	ClassDB::bind_method(D_METHOD("readValues", "variablePaths"), &VarManager::readValues);
	ClassDB::bind_method(D_METHOD("readValuesAsync", "variablePaths"), &VarManager::readValuesAsync);
	ClassDB::bind_method(D_METHOD("writeValue", "variablePath", "value"), &VarManager::writeValue);
	ClassDB::bind_method(D_METHOD("writeValues", "variablePaths", "values"), &VarManager::writeValues);
	ClassDB::bind_method(D_METHOD("writeValueAsync", "variablePath", "value"), &VarManager::writeValueAsync);
	ClassDB::bind_method(D_METHOD("writeValuesAsync", "variablePaths", "values"), &VarManager::writeValuesAsync);
	ClassDB::bind_method(D_METHOD("invokeMethod", "methodPath", "inputs", "outputs"), &VarManager::invokeMethod);
	ClassDB::bind_method(D_METHOD("invokeMethodAsync", "methodPath", "inputs", "outputs", "cbObject", "cbFuncName"), &VarManager::invokeMethodAsync, DEFVAL(Array()), DEFVAL(Variant()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("subscribe", "variablePath"), &VarManager::subscribe);
	ClassDB::bind_method(D_METHOD("unSubscribe", "variablePath"), &VarManager::unSubscribe);
	ClassDB::bind_method(D_METHOD("subscribes", "variablePaths", "aliasName"), &VarManager::subscribes);
	ClassDB::bind_method(D_METHOD("unSubscribes", "aliasName"), &VarManager::unSubscribes);
	ClassDB::bind_method(D_METHOD("subscribesById", "variablePaths", "id"), &VarManager::subscribesById);
	ClassDB::bind_method(D_METHOD("unSubscribesById", "id"), &VarManager::unSubscribesById);
	ClassDB::bind_method(D_METHOD("commitSubscribesById", "aliasName"), &VarManager::commitSubscribesById);
	ClassDB::bind_method(D_METHOD("commitUnSubscribesById", "aliasName"), &VarManager::commitUnSubscribesById);

	ClassDB::bind_method(D_METHOD("subscribeEvent", "eventSelections", "nodePath", "cbObject", "cbFuncName"), &VarManager::subscribeEvent, DEFVAL(Vector<String>()), DEFVAL(""), DEFVAL(Variant()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("unSubscribeEvent", "eventId"), &VarManager::unSubscribeEvent);

	ClassDB::bind_method(D_METHOD("readHistoryDataAsync", "variablePath", "startTime", "endTime", "returnBounds", "numValuesPerNode"), &VarManager::readHistoryDataAsync, DEFVAL(false), DEFVAL(10));

	ClassDB::bind_method(D_METHOD("setVarNotifyMode", "mode"), &VarManager::setVarNotifyMode, DEFVAL(VarManager::VarNotifyMode::SIGNAL));
	ClassDB::bind_method(D_METHOD("setTransferProtocol", "protocol"), &VarManager::setTransferProtocol, DEFVAL(VarManager::TransferProtocol::OPCUA));
	ClassDB::bind_method(D_METHOD("setMqttConf", "mqttConfDict"), &VarManager::setMqttConf);
	ClassDB::bind_method(D_METHOD("setAutoRefreshVarKV", "switch"), &VarManager::setAutoRefreshVarKV, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("initClientCache", "cachePath"), &VarManager::initClientCache, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("initClientCacheAsync", "cachePath"), &VarManager::initClientCacheAsync, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("initClientCacheQueued", "cachePath"), &VarManager::initClientCacheQueued, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("initCacheRuntime", "cachePath"), &VarManager::initCacheRuntime, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("setReadTimeoutMs", "ms"), &VarManager::setReadTimeoutMs);
	ClassDB::bind_method(D_METHOD("setWriteTimeoutMs", "ms"), &VarManager::setWriteTimeoutMs);
	ClassDB::bind_method(D_METHOD("setInvokeTimeoutMs", "ms"), &VarManager::setInvokeTimeoutMs);
	ClassDB::bind_method(D_METHOD("setRunPollIntervalMs", "ms"), &VarManager::setRunPollIntervalMs);
	ClassDB::bind_method(D_METHOD("poll"), &VarManager::poll);

	BIND_ENUM_CONSTANT(UNDEFINED);
	BIND_ENUM_CONSTANT(ALL_OK);
	BIND_ENUM_CONSTANT(CONNECTED);
	BIND_ENUM_CONSTANT(DISCONNECTED);
	BIND_ENUM_CONSTANT(WRONG_ID);
	BIND_ENUM_CONSTANT(NO_NEW_DATA);
	BIND_ENUM_CONSTANT(METHOD_BAD_CALL);
	BIND_ENUM_CONSTANT(METHOD_INPUT_ARGUMENT_COUNT_MISMATCH);
	BIND_ENUM_CONSTANT(ERROR_COMMUNICATION);

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(SIGNAL);
	BIND_ENUM_CONSTANT(CALL_DEFERRED);
	BIND_ENUM_CONSTANT(MESSAGEQUEUE);

	BIND_ENUM_CONSTANT(OPCUA);
	BIND_ENUM_CONSTANT(MQTT);
}

VarManager::VarManager() {
	singleton = this;
	m_status = 99; // DISCONNECTED
}

VarManager::~VarManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

Error VarManager::init() {
	return OK;
}

void VarManager::finish() {
	connect_close();
}

void VarManager::setConnectAuthMode(const int p_caMode) {}
String VarManager::getMqRefreshKVVarPath() { return ""; }
void VarManager::setUserPassword(const String &p_user, const String &p_password) {}

VarManager::UAStatusCode VarManager::connect_start(const String &p_address, const String &p_objectPath, const bool p_activateUpcalls) {
	m_server_address = p_address;
	m_handshake_sent = false;
	m_status = 99;

	// Extract hostname from opc.tcp://<host>:<port> or ws://<host>:<port>
	String host = "127.0.0.1";
	String addr = p_address;
	if (addr.begins_with("opc.tcp://")) {
		addr = addr.substr(10);
	} else if (addr.begins_with("ws://")) {
		addr = addr.substr(5);
	}
	int colon_pos = addr.find(":");
	int slash_pos = addr.find("/");
	if (colon_pos != -1) {
		host = addr.substr(0, colon_pos);
	} else if (slash_pos != -1) {
		host = addr.substr(0, slash_pos);
	} else if (!addr.is_empty()) {
		host = addr;
	}

	String gateway_url = "ws://" + host + ":8088/scada_ws";
	print_line("[VarManager-WS] Connecting to SCADA WebSocket Gateway: " + gateway_url + " (target: " + p_address + ")");

	m_ws_peer = WebSocketPeer::create();
	if (m_ws_peer.is_null()) {
		ERR_PRINT("[VarManager-WS] Failed to instantiate WebSocketPeer!");
		return DISCONNECTED;
	}

	Error err = m_ws_peer->connect_to_url(gateway_url);
	if (err != OK) {
		ERR_PRINT("[VarManager-WS] connect_to_url error: " + itos(err));
		return DISCONNECTED;
	}

	// Connect to process_frame for automatic frame polling
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (st && !st->is_connected(SNAME("process_frame"), Callable(this, SNAME("poll")))) {
		st->connect(SNAME("process_frame"), Callable(this, SNAME("poll")));
	}

	return ALL_OK;
}

VarManager::UAStatusCode VarManager::connect_close() {
	if (m_ws_peer.is_valid()) {
		m_ws_peer->close();
	}
	m_status = 99;
	emit_signal(SNAME("callback_varmanager_disconnect"));
	return ALL_OK;
}

int VarManager::getStatus() {
	return m_status;
}

void VarManager::_handle_ws_message(const String &p_text) {
	Variant v = JSON::parse_string(p_text);
	if (v.get_type() != Variant::DICTIONARY) {
		return;
	}
	Dictionary dict = v;
	String op = dict.get("op", "");

	if (op == "connected") {
		m_status = 0; // 0 = Connected (matching getStatus()==0 in tests)
		print_line("[VarManager-WS] Gateway Connected! Emitting callback_varmanager_connect.");
		emit_signal(SNAME("callback_varmanager_connect"));
		emit_signal(SNAME("callback_varmanager_cache_ready"), true);

		// Re-subscribe any pending paths
		if (m_subscribed_paths.size() > 0) {
			Array arr;
			for (int i = 0; i < m_subscribed_paths.size(); i++) {
				arr.push_back(m_subscribed_paths[i]);
			}
			Dictionary req;
			req["op"] = "sub";
			req["paths"] = arr;
			m_ws_peer->send_text(JSON::stringify(req));
		}
	} else if (op == "data_change") {
		String path = dict.get("path", "");
		Variant val = dict.get("val", Variant());
		m_tag_cache[path] = val;
		print_line("[VarManager-WS] Tag Changed: " + path + " -> " + String(val));
		emit_signal(SNAME("varChanged"), path, val);
	} else if (op == "sub_ack") {
		Dictionary initial = dict.get("initial", Dictionary());
		Array keys = initial.keys();
		for (int i = 0; i < keys.size(); i++) {
			String k = keys[i];
			Variant val = initial[k];
			m_tag_cache[k] = val;
			emit_signal(SNAME("varChanged"), k, val);
		}
		print_line("[VarManager-WS] Subscription Acked. Cached " + itos(keys.size()) + " initial tag values.");
	} else if (op == "read_resp") {
		Dictionary vals = dict.get("values", Dictionary());
		Array keys = vals.keys();
		for (int i = 0; i < keys.size(); i++) {
			String k = keys[i];
			m_tag_cache[k] = vals[k];
		}
	} else if (op == "write_ack") {
		String path = dict.get("path", "");
		Variant val = dict.get("val", Variant());
		m_tag_cache[path] = val;
	}
}

void VarManager::poll() {
	if (m_ws_peer.is_null()) {
		return;
	}
	m_ws_peer->poll();
	WebSocketPeer::State st = m_ws_peer->get_ready_state();

	if (st == WebSocketPeer::STATE_OPEN) {
		if (!m_handshake_sent) {
			m_handshake_sent = true;
			Dictionary req;
			req["op"] = "connect";
			req["client"] = "mindscada_web";
			m_ws_peer->send_text(JSON::stringify(req));
			print_line("[VarManager-WS] Sent connect handshake request to Gateway.");
		}
		while (m_ws_peer->get_available_packet_count() > 0) {
			const uint8_t *packet_data = nullptr;
			int packet_len = 0;
			Error err = m_ws_peer->get_packet(&packet_data, packet_len);
			if (err == OK && packet_len > 0) {
				String text = String::utf8((const char *)packet_data, packet_len);
				_handle_ws_message(text);
			}
		}
	} else if (st == WebSocketPeer::STATE_CLOSED) {
		if (m_status == 0) {
			m_status = 99;
			print_line("[VarManager-WS] Gateway connection closed.");
			emit_signal(SNAME("callback_varmanager_disconnect"));
		}
	}
}

Variant VarManager::readValue(const String &p_variablePath) {
	if (m_tag_cache.has(p_variablePath)) {
		return m_tag_cache[p_variablePath];
	}
	// Request read from gateway asynchronously
	if (m_ws_peer.is_valid() && m_ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
		Dictionary req;
		req["op"] = "read";
		Array arr;
		arr.push_back(p_variablePath);
		req["paths"] = arr;
		m_ws_peer->send_text(JSON::stringify(req));
	}
	return Variant();
}

Variant VarManager::readNextValue(const String &p_variablePath) {
	return readValue(p_variablePath);
}

Vector<Variant> VarManager::readValues(const Vector<String> &p_variablePaths) {
	Vector<Variant> res;
	for (int i = 0; i < p_variablePaths.size(); i++) {
		res.push_back(readValue(p_variablePaths[i]));
	}
	return res;
}

VarManager::UAStatusCode VarManager::readValueAsync(const String &p_variablePath) {
	readValue(p_variablePath);
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::readValuesAsync(const Vector<String> &p_variablePaths) {
	for (int i = 0; i < p_variablePaths.size(); i++) {
		readValue(p_variablePaths[i]);
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::writeValue(const String &p_variablePath, const Variant p_value) {
	m_tag_cache[p_variablePath] = p_value;
	if (m_ws_peer.is_valid() && m_ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
		Dictionary req;
		req["op"] = "write";
		req["path"] = p_variablePath;
		req["val"] = p_value;
		m_ws_peer->send_text(JSON::stringify(req));
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::writeValues(const Vector<String> &p_variablePaths, const Vector<Variant> p_values) {
	for (int i = 0; i < p_variablePaths.size() && i < p_values.size(); i++) {
		writeValue(p_variablePaths[i], p_values[i]);
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::writeValueAsync(const String &p_variablePath, const Variant p_value) {
	return writeValue(p_variablePath, p_value);
}

VarManager::UAStatusCode VarManager::writeValuesAsync(const Vector<String> &p_variablePaths, const Vector<Variant> p_values) {
	return writeValues(p_variablePaths, p_values);
}

VarManager::UAStatusCode VarManager::subscribe(const String &p_variablePath) {
	if (m_subscribed_paths.find(p_variablePath) == -1) {
		m_subscribed_paths.push_back(p_variablePath);
	}
	if (m_ws_peer.is_valid() && m_ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
		Dictionary req;
		req["op"] = "sub";
		Array arr;
		arr.push_back(p_variablePath);
		req["paths"] = arr;
		m_ws_peer->send_text(JSON::stringify(req));
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::unSubscribe(const String &p_variablePath) {
	int idx = m_subscribed_paths.find(p_variablePath);
	if (idx != -1) {
		m_subscribed_paths.remove_at(idx);
	}
	if (m_ws_peer.is_valid() && m_ws_peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
		Dictionary req;
		req["op"] = "unsub";
		Array arr;
		arr.push_back(p_variablePath);
		req["paths"] = arr;
		m_ws_peer->send_text(JSON::stringify(req));
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::subscribes(const Vector<String> &p_variablePaths, const String &p_aliasName) {
	for (int i = 0; i < p_variablePaths.size(); i++) {
		subscribe(p_variablePaths[i]);
	}
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::unSubscribes(const String &p_aliasName) {
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::subscribesById(const Vector<String> &p_variablePaths, const Variant &p_id) {
	return subscribes(p_variablePaths, "");
}

VarManager::UAStatusCode VarManager::unSubscribesById(const Variant &p_id) {
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::commitSubscribesById(const String &p_aliasName) {
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::commitUnSubscribesById(const String &p_aliasName) {
	return ALL_OK;
}

unsigned int VarManager::subscribeEvent(const Vector<String> &p_eventSelections, const String &p_nodePath, Variant p_cbObject, const String &p_cbFuncName) {
	return 0;
}

VarManager::UAStatusCode VarManager::unSubscribeEvent(const unsigned int &p_eventId) {
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::readHistoryDataAsync(const String &p_variablePath, const Variant &p_startTime, const Variant &p_endTime, bool p_returnBounds, const int p_numValuesPerNode) {
	return ALL_OK;
}

Variant VarManager::getNodeId(const String &p_nodePath) {
	return Variant();
}

Array VarManager::getChildrenOfPath(const String &p_nodePath) {
	return Array();
}

VarManager::UAStatusCode VarManager::invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments) {
	return ALL_OK;
}

VarManager::UAStatusCode VarManager::invokeMethodAsync(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments, Variant p_cbObject, const String &p_cbFuncName) {
	return ALL_OK;
}

void VarManager::setReadTimeoutMs(int ms) {}
void VarManager::setWriteTimeoutMs(int ms) {}
void VarManager::setInvokeTimeoutMs(int ms) {}
void VarManager::setRunPollIntervalMs(int ms) {}

bool VarManager::setVarNotifyMode(VarNotifyMode p_mode) { return true; }
bool VarManager::initClientCache(const String &p_cachePath) {
	emit_signal(SNAME("callback_varmanager_cache_ready"), true);
	return true;
}

bool VarManager::initClientCacheAsync(const String &p_cachePath) {
	emit_signal(SNAME("callback_varmanager_cache_ready"), true);
	return true;
}

bool VarManager::initClientCacheQueued(const String &p_cachePath) { return true; }
bool VarManager::initCacheRuntime(const String &p_cachePath) { return true; }

bool VarManager::setTransferProtocol(TransferProtocol p_mode) { return true; }
void VarManager::setMqttConf(const Dictionary &p_mqttConfDict) {}
void VarManager::setAutoRefreshVarKV(const bool &p_switch) {}
