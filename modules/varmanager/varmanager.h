/* varmanager.h */

#ifndef VARMANGER_H
#define VARMANGER_H

#pragma once

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#ifdef _DEBUG
//#pragma comment(lib, "libcmtd.lib")
#pragma comment(lib, "msvcrtd.lib")
//#pragma comment(lib, "vcruntimed.lib")
//#pragma comment(lib, "ucrtd.lib")
#endif
#endif /* _WIN32 */

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

#include "ICallback.h"
#include <OpcUaVariant.hh>
#include <queue>
#include <mutex>
#include <future>
#include <chrono>
#include <unordered_map>
#include <memory>

// There is a parameter limit of 5 in C++ modules for things such as subclasses.
// This can be raised to 13 by including the header file core/method_bind_ext.gen.inc.
//#include "core/object/method_bind_ext.gen.inc"

class UAClient;
class MqttTransferInterface;
namespace OPCUA {
	class Variant;
}
class VarManager : public Object, ICallback {
	GDCLASS(VarManager, Object);
	
	//static void thread_func(void* p_date);

protected:
	static void _bind_methods();

public:
	VarManager();
	virtual ~VarManager();

	enum UAStatusCode {
		/// Undefined (i.e. constructor) value
		UNDEFINED,
		/// Operation completed successfully
		ALL_OK,
		/// Connected status
		CONNECTED,
		/// Disconnected status
		DISCONNECTED,
		/// Element name (i.e. variable/method name) not found
		WRONG_ID,
		/// no new data is available (yet)
		NO_NEW_DATA,
		/// ???
		METHOD_BAD_CALL,
		/// number of used input arguments missmatch
		METHOD_INPUT_ARGUMENT_COUNT_MISMATCH,
		//METHOD_INPUT_ARGUMENT_TYPE_MISMATCH,
		//METHOD_OUTPUT_ARGUMENT_COUNT_MISMATCH,
		//METHOD_OUTPUT_ARGUMENT_TYPE_MISMATCH,

		/// communication error
		ERROR_COMMUNICATION
	};

	enum ConnectAuthMode {
		/// 匿名
		ANONYMOUS = 1,
		/// 用户密码
		USER_PASSWORD = 2,
		/// 授权
		CERTIFICATE = 4,
		/// 授权+私钥
		CERTIFICATE_PRIVATEKEY = 8,

		/// length
		MAXLENGTH
	};

	struct OpcTestConfig {
		String server_url    = "opc.tcp://127.0.0.1:4840";
		String object_path   = "Objects";
		String test_var_path = "Objects/TestVar1";
		Vector<String> batch_var_paths;
		int concurrency  = 8;
		int iterations   = 100;
		int timeout_ms   = 3000;
	};
	static OpcTestConfig loadTestConfig();

	// obsolete
	void connect_signals();

	void setConnectAuthMode(const int p_caMode);
	String getMqRefreshKVVarPath();
	void setUserPassword(const String & p_user, const String & p_password);
	UAStatusCode connect_start(const String &p_address = "opc.tcp://127.0.0.1:4840", const String &p_objectPath = "Server", const bool p_activateUpcalls = true);
	UAStatusCode connect_close();
	int getStatus();
	Variant getNodeId(const String &p_nodePath);
	Array getChildrenOfPath(const String &p_nodePath);
	Variant readValue(const String &p_variablePath);
	Variant readNextValue(const String &p_variablePath);
	Vector<Variant> readValues(const Vector<String> &p_variablePaths);
	UAStatusCode readValueAsync(const String &p_variablePath);
	UAStatusCode readValuesAsync(const Vector<String> &p_variablePaths);
	UAStatusCode writeValue(const String &p_variablePath, const Variant p_value);
	UAStatusCode writeValues(const Vector<String> &p_variablePaths, const Vector<Variant> p_values);
	UAStatusCode writeValueAsync(const String &p_variablePath, const Variant p_value);
	UAStatusCode writeValuesAsync(const Vector<String> &p_variablePaths, const Vector<Variant> p_values);
	UAStatusCode invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments);
	UAStatusCode invokeMethodAsync(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments = Array(), Variant p_cbObject = Variant(), const String &p_cbFuncName = "");

	UAStatusCode subscribe(const String &p_variablePath);
	UAStatusCode unSubscribe(const String &p_variablePath);

	UAStatusCode subscribes(const Vector<String> &p_variablePaths, const String &p_aliasName);
	UAStatusCode unSubscribes(const String &p_aliasName);

	UAStatusCode subscribesById(const Vector<String> &p_variablePaths, const Variant &p_id);
	UAStatusCode unSubscribesById(const Variant &p_id);

	UAStatusCode commitSubscribesById(const String &p_aliasName);
	UAStatusCode commitUnSubscribesById(const String &p_aliasName);

	unsigned int subscribeEvent(const Vector<String> &p_eventSelections = Vector<String>(), const String &p_nodePath = "", Variant p_cbObject = Variant(), const String &p_cbFuncName = "");
	UAStatusCode unSubscribeEvent(const unsigned int &p_eventId);

	UAStatusCode readHistoryDataAsync(const String &p_variablePath, const Variant &p_startTime, const Variant &p_endTime, bool p_returnBounds = false, const int p_numValuesPerNode = 10);


	void setReadTimeoutMs(int ms);
	void setWriteTimeoutMs(int ms);
	void setInvokeTimeoutMs(int ms);
	// Run-thread poll cadence (= GenericClient minSubscriptionInterval). The run
	// loop sleeps to this interval each cycle, so it is the floor on read/write
	// latency (a command waits up to ~1 interval to be drained; async reads ~2).
	// Default 10 ms (down from the wrapper's 100 ms). MUST be called before
	// connect_start — it recreates the client and only applies while disconnected.
	void setRunPollIntervalMs(int ms);

	void variableChanged_1(const String &varName, const Variant &value);
	virtual void /*ICallback::*/variableChanged(const std::string &varName, const OPCUA::Variant &value) override;
	void variablesChanged_1(const Vector<String> &varNames, const Vector<Variant> & values);
	virtual void /*ICallback::*/variablesChanged(const std::vector<std::string> &varNames, const std::vector<OPCUA::Variant> & values) override;

	virtual void /*ICallback::*/handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) override;

	virtual void /*ICallback::*/handleVariableReadHisCallback(const std::string& nodePath, const std::vector<UA_DataValue>& datas, const bool moreData) override;

	virtual void /*ICallback::*/handleStatusCallback(const int status) override;

	virtual void /*ICallback::*/handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs) override;

	static VarManager *get_singleton();
	//static VarManager *get_alarmsingleton();
	Error init();
	void finish();

	// 注册变量变化 c++内部使用 not 4 gdscript
	void register_vars(String p_var, ObjectID id, bool cache =false);
	void unregister_vars(ObjectID id, bool cache = false);

	enum VarNotifyMode {
		/// Undefined (i.e. constructor) value
		NONE,
		SIGNAL,
		CALL_DEFERRED,
		MESSAGEQUEUE
	};
	bool setVarNotifyMode(VarNotifyMode p_mode = VarManager::VarNotifyMode::SIGNAL);
	//bool setVarNotifyMode(VarManager::VarNotifyMode mode = /*SIGNAL*/)
	// BLOCKING warm (Stop/warm/Start on the calling thread); also emits
	// callback_varmanager_cache_ready(success) (deferred) for symmetry with Async.
	bool initClientCache(const String &p_cachePath = "");
	// Non-blocking warm: posts WarmCache to the run thread, returns immediately,
	// emits callback_varmanager_cache_ready(success) when done.
	bool initClientCacheAsync(const String &p_cachePath = "");
	// Implementation behind initClientCacheAsync (queued/run-thread warm).
	bool initClientCacheQueued(const String &p_cachePath = "");

	bool initCacheRuntime(const String &p_cachePath = "");

	enum TransferProtocol {
		OPCUA,
		MQTT
	};
	bool setTransferProtocol(TransferProtocol p_mode = VarManager::TransferProtocol::OPCUA);
	void setMqttConf(const Dictionary &p_mqttConfDict);
	void setAutoRefreshVarKV(const bool &p_switch = true);

private:

	// ── Async command queue infrastructure ──────────────────────────────────

	enum class OpcCmdType { ReadSync, ReadAsync, WriteSync, WriteAsync, InvokeSync, InvokeAsync, WarmCache };

	struct OpcCommand {
		OpcCmdType type;
		std::vector<std::string>        paths;
		std::vector<OPCUA::Variant>     values;
		std::string                     method_path;
		std::vector<OPCUA::Variant>     inputs;
		ObjectID                        cb_obj_id{};
		String                          cb_func_name;
		std::string                     cache_path;   // T3: WarmCache target dir
		std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>> promise;
		std::chrono::steady_clock::time_point expires;
	};

	struct InflightEntry {
		std::vector<std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>>> sync_waiters;
		std::chrono::steady_clock::time_point expires;
	};

	// Command queue: main thread writes, background thread drains
	std::queue<OpcCommand> cmd_queue_;
	std::mutex             cmd_mutex_;

	// In-flight read dedup map (background thread only — no mutex needed)
	std::unordered_map<std::string, InflightEntry> inflight_reads_;

	// Timeout config (milliseconds)
	int read_timeout_ms_   = 3000;
	int write_timeout_ms_  = 3000;
	int invoke_timeout_ms_ = 5000;
	// Run-thread poll cadence in ms (GenericClient minSubscriptionInterval).
	// Applied when the UAClient is constructed; default 10 ms.
	int run_poll_interval_ms_ = 10;

	static VarManager *singleton;
	//static VarManager *alarmsingleton;
	MqttTransferInterface *g_Mqtt;

	//  uint32_t
	// <id, Vector<ObjectID>>
	Dictionary varhash_ids_dict;

	//  string
	// <varpath, Vector<ObjectID>>
	Dictionary varpath_ids_dict;

	//  string   int=0-registed 1-subed
	// <varpath, status>
	Dictionary varpath_subed_dict;

	//  string
	// <aliasName, Dictionary<varpath, Vector<ObjectID>>>
	Dictionary aliasName_varpath_ids_dict_dict;

	//  uint32_t  Array [0]-ObjectID [1]-String-callback_func_name
	// <id, Array>
	Dictionary invokedMethodID_ObjectID_funcName_dict;

	//  uint32_t  Array [0]-ObjectID [1]-String-callback_func_name
	// <id, Array>
	Dictionary subscribedEventID_ObjectID_funcName_dict;

	VarNotifyMode m_varNotifyMode;

	TransferProtocol m_transferProtocol;

	Dictionary m_mqttConfDict;

	bool m_autoRefreshVarKVSwitch;

	String m_mqRefreshKVPath;

	UAClient *client;
	void convertOPCVar2GDVar(const OPCUA::Variant &src, Variant &dest);
	// intmode
	//	= 0: int64_t
	//	= 1: int32_t
	void convertGDVar2OPCVar(const Variant &src, OPCUA::Variant &dest, int intmode = 0);
	Variant getNodeId_p(const std::string &std_nodePath);
	Variant readValue_p(const std::string &std_variablePath);
	Variant readNextValue_p(const std::string &std_variablePath);
	UAStatusCode readValueAsync_p(const std::string &std_variablePath);
	Vector<Variant> readValues_p(const std::vector<std::string> &std_variablePaths);
	UAStatusCode readValuesAsync_p(const std::vector<std::string> &std_variablePaths);
	void variableChanged_p(const String &varName, const OPCUA::Variant &value);
	void notifyRelatedControls(const String &varName, const Variant &value);

	//std::string &std_string_format(const char *_Format, ...);
	void postReadCmd(const std::vector<std::string> &paths,
	                 const std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>> &promise,
	                 bool is_sync);
	void postWriteCmd(const std::vector<std::string> &paths,
	                  const std::vector<OPCUA::Variant> &values, bool is_sync,
	                  const std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>> &promise);
	virtual void drainCommandQueue() override;
	virtual void expireInflight() override;
#ifdef  _WIN32
	char *wc_to_utf8(const wchar_t *wc);
	inline std::wstring utf8_to_wide(const std::string &utf8Text);
	std::string wide_to_utf8(const std::wstring &wideText);
#endif
};

VARIANT_ENUM_CAST(VarManager::UAStatusCode);
VARIANT_ENUM_CAST(VarManager::VarNotifyMode);
VARIANT_ENUM_CAST(VarManager::ConnectAuthMode);
VARIANT_ENUM_CAST(VarManager::TransferProtocol);

#endif // VARMANGER_H
