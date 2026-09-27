/* mqttmanager_web.cpp */

#include "mqttmanager.h"
#include "core/config/engine.h"
#include "core/os/os.h"
#include "modules/websocket/websocket_peer.h"
#include "scene/main/scene_tree.h"

MqttManager *MqttManager::singleton = nullptr;

MqttManager *MqttManager::get_singleton() {
	return singleton;
}

void MqttManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("MqttConnected"));
	ADD_SIGNAL(MethodInfo("MqttLostconnect"));
	ADD_SIGNAL(MethodInfo("MqttDisconnected"));
	ADD_SIGNAL(MethodInfo("MqttMsgReceived", PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::STRING, "msg"), PropertyInfo(Variant::INT, "qos")));
	ADD_SIGNAL(MethodInfo("MqttProcessRequest", PropertyInfo(Variant::STRING, "req_topic"), PropertyInfo(Variant::STRING, "req_msg"), PropertyInfo(Variant::STRING, "rsp_topic")));

	ClassDB::bind_method(D_METHOD("init", "prjPath"), &MqttManager::init, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("init_bydict", "mqttConfDict"), &MqttManager::init_bydict);
	ClassDB::bind_method(D_METHOD("connect_start", "host", "port", "clientId"), &MqttManager::connect_start, DEFVAL(""), DEFVAL(1883), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("connect_reqrsp_start", "host", "port", "clientId"), &MqttManager::connect_reqrsp_start, DEFVAL(""), DEFVAL(1883), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("subscribe", "topic", "qos"), &MqttManager::subscribe, DEFVAL(MqttManager::QOS::QOS0));
	ClassDB::bind_method(D_METHOD("subscribe1", "topic", "qos"), &MqttManager::subscribe1, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("autoReSubscribe"), &MqttManager::autoReSubscribe);
	ClassDB::bind_method(D_METHOD("unsubscribe", "topic"), &MqttManager::unsubscribe);
	ClassDB::bind_method(D_METHOD("publish", "topic", "msg", "qos"), &MqttManager::publish, DEFVAL(MqttManager::QOS::QOS0));
	ClassDB::bind_method(D_METHOD("req_rsp", "reqtopic", "reqmsg", "rsptopic", "remoteClientId", "qos", "secs_timeout", "auto_mode"), &MqttManager::req_rsp, DEFVAL(MqttManager::QOS::QOS0), DEFVAL(5), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("req_rsp1", "reqtopic", "reqmsg", "rsptopic", "remoteClientId", "qos", "secs_timeout", "auto_mode"), &MqttManager::req_rsp1, DEFVAL(2), DEFVAL(5), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("connect_close"), &MqttManager::connect_close);
	ClassDB::bind_method(D_METHOD("invokeMethod", "nodePath", "inputArguments", "outputArguments", "remoteClientId"), &MqttManager::invokeMethod);
	ClassDB::bind_method(D_METHOD("poll"), &MqttManager::poll);

	BIND_ENUM_CONSTANT(QOS0);
	BIND_ENUM_CONSTANT(QOS1);
	BIND_ENUM_CONSTANT(QOS2);
}

MqttManager::MqttManager() {
	singleton = this;
}

MqttManager::~MqttManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void MqttManager::connect_signals() {}

Error MqttManager::init(const String &p_prjPath) {
	return OK;
}

Error MqttManager::init_bydict(const Dictionary &p_mqttConfDict) {
	String host = p_mqttConfDict.get("@ServerIP", "127.0.0.1");
	int port = p_mqttConfDict.get("@ServerPort", 1883);
	String client_id = p_mqttConfDict.get("@ClientID", "mindscada_web");
	connect_start(host, port, client_id);
	return OK;
}

bool MqttManager::connect_start(const String &p_host, const int p_port, const String &p_clientId) {
	m_host = p_host.is_empty() ? "127.0.0.1" : p_host;
	m_port = p_port > 0 ? p_port : 1883;
	m_clientId = p_clientId.is_empty() ? "mindscada_web" : p_clientId;
	m_handshake_sent = false;
	m_connSts = 0;

	// Transparent tunnel routing: connect to WebSocket tunnel at port 8083
	String tunnel_url = "ws://" + m_host + ":8083/mqtt";
	print_line("[MQTT-Tunnel] Connecting transparent tunnel to " + tunnel_url + " (target TCP: " + m_host + ":" + itos(m_port) + ")");

	m_ws_peer = WebSocketPeer::create();
	if (m_ws_peer.is_null()) {
		ERR_PRINT("[MQTT-Tunnel] Failed to instantiate WebSocketPeer!");
		return false;
	}

	Vector<String> protocols;
	protocols.push_back("mqtt");
	m_ws_peer->set_supported_protocols(protocols);

	Error err = m_ws_peer->connect_to_url(tunnel_url);
	if (err != OK) {
		ERR_PRINT("[MQTT-Tunnel] connect_to_url error: " + itos(err));
		return false;
	}

	// Connect to process_frame for automatic frame polling
	SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (st && !st->is_connected(SNAME("process_frame"), Callable(this, SNAME("poll")))) {
		st->connect(SNAME("process_frame"), Callable(this, SNAME("poll")));
	}

	return true;
}

bool MqttManager::connect_reqrsp_start(const String &p_host, const int p_port, const String &p_clientId) {
	return connect_start(p_host, p_port, p_clientId);
}

void MqttManager::_send_mqtt_connect() {
	if (m_ws_peer.is_null() || m_ws_peer->get_ready_state() != WebSocketPeer::STATE_OPEN) {
		return;
	}

	CharString cid_utf8 = m_clientId.utf8();
	int cid_len = cid_utf8.length();

	// Variable header: Protocol "MQTT" (4 bytes), level 4, flags 0x02 (CleanSession), keepalive 60
	const uint8_t var_hdr[] = { 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x3C };
	int var_len = sizeof(var_hdr);
	int payload_len = 2 + cid_len;
	int rem_len = var_len + payload_len;

	Vector<uint8_t> pkt;
	pkt.push_back(0x10); // CONNECT packet type
	// Remaining length encoding (single byte for lengths <= 127)
	pkt.push_back((uint8_t)rem_len);
	for (int i = 0; i < var_len; i++) {
		pkt.push_back(var_hdr[i]);
	}
	// Client ID
	pkt.push_back((uint8_t)(cid_len >> 8));
	pkt.push_back((uint8_t)(cid_len & 0xFF));
	for (int i = 0; i < cid_len; i++) {
		pkt.push_back((uint8_t)cid_utf8[i]);
	}

	m_ws_peer->send(pkt.ptr(), pkt.size(), WebSocketPeer::WRITE_MODE_BINARY);
	print_line("[MQTT-Tunnel] Sent MQTT CONNECT packet (" + itos(pkt.size()) + " bytes, ClientId: " + m_clientId + ")");
}

void MqttManager::_handle_mqtt_packet(const uint8_t *data, int len) {
	if (len < 2) return;
	uint8_t pkt_type = data[0] >> 4;

	if (pkt_type == 2) { // CONNACK
		if (len >= 4 && data[3] == 0x00) {
			m_connSts = 1;
			print_line("[MQTT-Tunnel] Received CONNACK (Connection Accepted)! Emitting MqttConnected signal.");
			emit_signal(SNAME("MqttConnected"));
			autoReSubscribe();
		} else {
			ERR_PRINT("[MQTT-Tunnel] CONNACK refused, return code: " + itos(len >= 4 ? data[3] : -1));
		}
	} else if (pkt_type == 3) { // PUBLISH
		// Parse remaining length
		int offset = 1;
		while (offset < len) {
			uint8_t b = data[offset++];
			if ((b & 0x80) == 0) break;
		}

		if (offset + 2 <= len) {
			uint16_t topic_len = (data[offset] << 8) | data[offset + 1];
			offset += 2;
			if (offset + topic_len <= len) {
				String topic = String::utf8((const char *)&data[offset], topic_len);
				offset += topic_len;
				uint8_t qos = (data[0] >> 1) & 0x03;
				if (qos > 0) {
					offset += 2; // skip packet id
				}
				int msg_len = len - offset;
				String msg = String::utf8((const char *)&data[offset], msg_len);
				print_line("[MQTT-Tunnel] Received PUBLISH on '" + topic + "': " + msg);
				emit_signal(SNAME("MqttMsgReceived"), topic, msg, (int)qos);
			}
		}
	} else if (pkt_type == 9) { // SUBACK
		print_line("[MQTT-Tunnel] Received SUBACK from broker.");
	} else if (pkt_type == 13) { // PINGRESP
		// Keepalive response
	}
}

void MqttManager::poll() {
	if (m_ws_peer.is_null()) {
		return;
	}
	m_ws_peer->poll();
	WebSocketPeer::State st = m_ws_peer->get_ready_state();

	if (st == WebSocketPeer::STATE_OPEN) {
		if (!m_handshake_sent) {
			m_handshake_sent = true;
			_send_mqtt_connect();
		}
		while (m_ws_peer->get_available_packet_count() > 0) {
			const uint8_t *packet_data = nullptr;
			int packet_len = 0;
			Error err = m_ws_peer->get_packet(&packet_data, packet_len);
			if (err == OK && packet_len > 0) {
				_handle_mqtt_packet(packet_data, packet_len);
			}
		}
	} else if (st == WebSocketPeer::STATE_CLOSED) {
		if (m_connSts == 1) {
			m_connSts = 0;
			print_line("[MQTT-Tunnel] Tunnel connection closed. Emitting MqttDisconnected.");
			emit_signal(SNAME("MqttDisconnected"));
		}
	}
}

bool MqttManager::subscribe(const String &p_topic, MqttManager::QOS p_qos) {
	if (m_subscribed_topics.find(p_topic) == -1) {
		m_subscribed_topics.push_back(p_topic);
		m_subscribed_qos.push_back((int)p_qos);
	}

	if (m_ws_peer.is_null() || m_ws_peer->get_ready_state() != WebSocketPeer::STATE_OPEN || m_connSts != 1) {
		return true; // will autoReSubscribe on connect
	}

	CharString topic_utf8 = p_topic.utf8();
	int tlen = topic_utf8.length();
	uint16_t pid = m_packet_id++;
	int rem_len = 2 + 2 + tlen + 1;

	Vector<uint8_t> pkt;
	pkt.push_back(0x82); // SUBSCRIBE QoS 1 fixed header
	pkt.push_back((uint8_t)rem_len);
	pkt.push_back((uint8_t)(pid >> 8));
	pkt.push_back((uint8_t)(pid & 0xFF));
	pkt.push_back((uint8_t)(tlen >> 8));
	pkt.push_back((uint8_t)(tlen & 0xFF));
	for (int i = 0; i < tlen; i++) {
		pkt.push_back((uint8_t)topic_utf8[i]);
	}
	pkt.push_back((uint8_t)p_qos);

	m_ws_peer->send(pkt.ptr(), pkt.size(), WebSocketPeer::WRITE_MODE_BINARY);
	print_line("[MQTT-Tunnel] Sent SUBSCRIBE for topic '" + p_topic + "'");
	return true;
}

bool MqttManager::subscribe1(const String &p_topic, const int &p_qos) {
	return subscribe(p_topic, (MqttManager::QOS)p_qos);
}

bool MqttManager::autoReSubscribe() {
	for (int i = 0; i < m_subscribed_topics.size(); i++) {
		subscribe(m_subscribed_topics[i], (MqttManager::QOS)m_subscribed_qos[i]);
	}
	return true;
}

bool MqttManager::unsubscribe(const String &p_topic) {
	int idx = m_subscribed_topics.find(p_topic);
	if (idx != -1) {
		m_subscribed_topics.remove_at(idx);
		m_subscribed_qos.remove_at(idx);
	}
	return true;
}

bool MqttManager::publish(const String &p_topic, const String &p_msg, MqttManager::QOS p_qos) {
	if (m_ws_peer.is_null() || m_ws_peer->get_ready_state() != WebSocketPeer::STATE_OPEN || m_connSts != 1) {
		return false;
	}

	CharString topic_utf8 = p_topic.utf8();
	int tlen = topic_utf8.length();
	CharString msg_utf8 = p_msg.utf8();
	int mlen = msg_utf8.length();

	int rem_len = 2 + tlen + mlen;
	Vector<uint8_t> pkt;
	pkt.push_back(0x30 | (((uint8_t)p_qos & 0x03) << 1));
	pkt.push_back((uint8_t)rem_len);
	pkt.push_back((uint8_t)(tlen >> 8));
	pkt.push_back((uint8_t)(tlen & 0xFF));
	for (int i = 0; i < tlen; i++) {
		pkt.push_back((uint8_t)topic_utf8[i]);
	}
	for (int i = 0; i < mlen; i++) {
		pkt.push_back((uint8_t)msg_utf8[i]);
	}

	m_ws_peer->send(pkt.ptr(), pkt.size(), WebSocketPeer::WRITE_MODE_BINARY);
	print_line("[MQTT-Tunnel] Sent PUBLISH to '" + p_topic + "' (" + itos(mlen) + " bytes)");
	return true;
}

String MqttManager::req_rsp(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, MqttManager::QOS p_qos, int p_secs_timeout, bool p_auto_mode) {
	publish(p_reqtopic, p_reqmsg, p_qos);
	return "";
}

String MqttManager::req_rsp1(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, int p_qos, int p_secs_timeout, bool p_auto_mode) {
	return req_rsp(p_reqtopic, p_reqmsg, p_rsptopic, p_remoteClientId, (MqttManager::QOS)p_qos, p_secs_timeout, p_auto_mode);
}

bool MqttManager::connect_close() {
	if (m_ws_peer.is_valid()) {
		m_ws_peer->close();
	}
	m_connSts = 0;
	return true;
}

void MqttManager::finish() {
	connect_close();
}

void MqttManager::onMqttConnected() {}
void MqttManager::onMqttLostconnect() {}
void MqttManager::onMqttDisconnected() {}
void MqttManager::onMqttMsgReceived(const String &topic, const String &msg, const int qos) {}

Error MqttManager::invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments, const String &p_remoteClientId) {
	return OK;
}
