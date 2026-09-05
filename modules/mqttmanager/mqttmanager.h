/* mqttmanager.h */

#ifndef MQTTMANGER_H
#define MQTTMANGER_H

#pragma once

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "crypt32.lib")
#include <shlwapi.h>
#ifdef _DEBUG
//#pragma comment(lib, "libcmtd.lib")
#pragma comment(lib, "msvcrtd.lib")
//#pragma comment(lib, "vcruntimed.lib")
//#pragma comment(lib, "ucrtd.lib")
//#else
//#pragma comment(lib, "msvcrt.lib")
#endif
#elif defined(__linux__)
#include <sys/stat.h>
#include <sys/types.h>
#define INFINITE 0xFFFFFFFF // Infinite timeout
#endif

#include <atomic>
#include <future>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "core/variant/variant.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
#include "mqttclient.h"
#include "ctimer.h"
#include "mio.hpp"
#include "mqttTransferInterface.h"

// There is a parameter limit of 5 in C++ modules for things such as subclasses.
// This can be raised to 13 by including the header file core/method_bind_ext.gen.inc.
//#include "core/object/method_bind_ext.gen.inc"
using namespace std;

class MqttManager : /*public Object, */public MqttTransferInterface {
	GDCLASS(MqttManager, MqttTransferInterface);
	
	//static void thread_func(void* p_date);

protected:
	static void _bind_methods();

public:
	MqttManager();
	virtual ~MqttManager();

	enum QOS {
		QOS0 = 0,
		QOS1 = 1,
		QOS2 = 2,
	};

	void connect_signals();

	virtual Error init(const String &p_prjPath = "") override;

	/// <summary>
	/// {
    //    "@Enable": "1",
    //    "@ClientID": "svc_lxy4_001",
    //    "@ServerIP": "192.167.200.1",
    //    "@ServerPort": "1883",
    //    "@User": "",
    //    "@Psw": "",
    //    "@TrustName": "",
    //    "@KeyName": "",
    //    "@KeepAlive": "10",
    //    "@AutoReconnect": "1",
    //    "@AutoReconnectInterval": "15"
    //  }
	/// </summary>
	virtual Error init_bydict(const Dictionary &p_mqttConfDict) override;

	virtual bool connect_start(const String &p_host = "", const int p_port = 1883, const String &p_clientId = "") override;
	virtual bool connect_reqrsp_start(const String &p_host = "", const int p_port = 1883, const String &p_clientId = "") override;

	bool subscribe(const String &p_topic, MqttManager::QOS p_qos = MqttManager::QOS::QOS0);
	virtual bool subscribe1(const String &p_topic, const int &p_qos = 0) override;

	bool autoReSubscribe();

	bool unsubscribe(const String &p_topic);

	bool publish(const String &p_topic, const String &p_msg, MqttManager::QOS p_qos = MqttManager::QOS::QOS0);

	String req_rsp(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, MqttManager::QOS p_qos = MqttManager::QOS::QOS2, int p_secs_timeout = 5, bool p_auto_mode = true);
	String req_rsp1(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, int p_qos = 2, int p_secs_timeout = 5, bool p_auto_mode = true) override;

	bool connect_close();

	static MqttManager *get_singleton();
	void finish();

	void onMqttConnected();
	void onMqttLostconnect();
	void onMqttDisconnected();
	void onMqttMsgReceived(const String &topic, const String &msg, const int qos);

	String getUtf8String(std::string in);
	bool try_reconnect(MqttClient *cli);

	MqttClient *getMqttClient() { return m_mqttClient; };

	void setConnSts(int sts) { m_connSts = sts; };

	virtual Error invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments, const String &p_remoteClientId) override;

private:

	static MqttManager *singleton;
	MqttClient *m_mqttClient;
	MqttClient *m_mqttReqRspClient;
	CTimer m_heartbeatTimer;
	// 链接状态 -1-初始状态 0-disconnected 1-connected
	int m_connSts;

	std::thread startConsumeThread;

	std::vector<std::string> std_subscribedTopics;
	std::vector<int> std_subscribedQos;

	//wstring getAppDataRoaming(const wstring &company, const wstring &appName);

	void onHeartbeatTimeout();

	void initSpdlog();
	//void startConsumeMsg(MqttManager* pman);

	String m_last_mqRefreshKVPath;
	mio::mmap_sink m_rw_mmap;
	String getFileMD5(const String &filePath);
	void _pre_msg_received(String topic, mqtt::const_message_ptr msg, const int qos);
	int handle_mio_error(const std::error_code &error);

	std::wstring stringToWstring(const std::string& t_str);
	std::wstring stringToWstring(const char* utf8Bytes);
	int getIndexInVector(vector<std::string> vec, std::string v);

	std::string m_prjPath;
	Dictionary m_mqttConfDict;

	// --- concurrent req_rsp support ---
	// Thread-safe one-time init guard for m_mqttReqRspClient
	std::once_flag m_reqRspOnceFlag;
	// Monotonically increasing counter for unique CORRELATION_DATA per call
	std::atomic<uint64_t> m_corrIdCounter{ 0 };
	// Guards m_inFlightRequests and m_reqRspSubscribedTopics
	std::mutex m_inFlightMutex;
	// Maps corrId → promise; background thread fulfills promises by corrId
	std::unordered_map<std::string, std::promise<std::string>> m_inFlightRequests;
	// Tracks which reply topics are already subscribed (subscribed once, kept permanently)
	std::set<std::string> m_reqRspSubscribedTopics;
	// Background dispatch thread lifecycle flag
	std::atomic<bool> m_reqRspRunning{ false };
	// Background thread: consumes messages and dispatches to promises
	std::thread m_reqRspConsumeThread;

	void _reqrsp_consume_loop();
};

VARIANT_ENUM_CAST(MqttManager::QOS);

#endif // MQTTMANGER_H
