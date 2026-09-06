/* varmanager.cpp */

#include "varmanager.h"
#include "core/variant/array.h"
#include "core/object/message_queue.h"
#include "core/object/callable_mp.h"
#include "core/os/os.h"
#include "core/io/config_file.h"

#include <algorithm>
#include <iostream>
#include <thread>
#include <future>

#include "uaclient.h"
#include "util.h"
#include "../mqttmanager/mqttTransferInterface.h"
#include "../kvmanager/kvTransferInterface.h"

#define SPDLOG_NO_EXCEPTIONS
#include "spdlog/async.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

static void load_levels_example() {
	// Examples:
	//
	// set global level to debug:
	// export SPDLOG_LEVEL=debug
	//
	// turn off all logging except for logger1:
	// export SPDLOG_LEVEL="*=off,logger1=debug"
	//

	// turn off all logging except for logger1 and logger2:
	// export SPDLOG_LEVEL="off,logger1=debug,logger2=info"
	
	// Set the log level to "info" and mylogger to "trace":
	// SPDLOG_LEVEL=info,mylogger=trace && ./example
	spdlog::cfg::load_env_levels();
	// or from command line:
	// ./example SPDLOG_LEVEL=info,mylogger=trace
	// #include "spdlog/cfg/argv.h" // for loading levels from argv
	// spdlog::cfg::load_argv_levels(args, argv);
}

VarManager *VarManager::singleton = NULL;
//VarManager *VarManager::alarmsingleton = NULL;

VarManager *VarManager::get_singleton() {
	return singleton;
}

//VarManager *VarManager::get_alarmsingleton() {
//	if (alarmsingleton == NULL)
//		alarmsingleton = new VarManager();
//	return alarmsingleton;
//}

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
	ClassDB::bind_method(D_METHOD("setReadTimeoutMs",   "ms"), &VarManager::setReadTimeoutMs);
	ClassDB::bind_method(D_METHOD("setWriteTimeoutMs",  "ms"), &VarManager::setWriteTimeoutMs);
	ClassDB::bind_method(D_METHOD("setInvokeTimeoutMs", "ms"), &VarManager::setInvokeTimeoutMs);
	ClassDB::bind_method(D_METHOD("setRunPollIntervalMs", "ms"), &VarManager::setRunPollIntervalMs);

	//ADD_PROPERTY(PropertyInfo(Variant::STRING, "subject"), "", "get_subject");
	//ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "names"), "", "get_names");
	//ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "strings"), "", "get_strings");

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

// obsolete
void VarManager::connect_signals() {
	//connect("varChanged", callable_mp(this, &VarManager::variableChanged_1));
	//connect("varsChanged", callable_mp(this, &VarManager::variablesChanged_1));
	//connect("eventTriggered", callable_mp(this, &VarManager::handleEvent));
	//connect("methodInvoked", callable_mp(this, &VarManager::handleInvokeMethodCallback));

	//connect("varChanged", Callable(this, "variableChanged"));
	//connect("varsChanged", Callable(this, "variablesChanged"));
	//connect("eventTriggered", Callable(this, "handleEvent"));
	//connect("methodInvoked", Callable(this, "handleInvokeMethodCallback"));
}

VarManager::VarManager() {
	client = new UAClient(std::chrono::milliseconds(run_poll_interval_ms_));
	singleton = this;
	m_varNotifyMode = VarNotifyMode::SIGNAL;
	m_transferProtocol = TransferProtocol::OPCUA;
	g_Mqtt = nullptr;
	m_autoRefreshVarKVSwitch = false;
}

VarManager::~VarManager() {
	//singleton = NULL;
	if (client)
		delete client;
}

void VarManager::setConnectAuthMode(const int p_caMode)
{
	if (client)
		client->setConnectAuthMode((uint16_t)p_caMode);
}
String VarManager::getMqRefreshKVVarPath() {
	m_mqRefreshKVPath = OS::get_singleton()->get_cache_path();
	return m_mqRefreshKVPath;
}
void VarManager::setUserPassword(const String &p_user, const String &p_password) {
	if (client) {
		std::string std_users = std::string(p_user.utf8().get_data());
		std::string std_password = std::string(p_password.utf8().get_data());
		client->setUserPassword(std_users, std_password);
	}	
}

VarManager::UAStatusCode VarManager::connect_start(const String &p_address, const String &p_objectPath, const bool p_activateUpcalls) {
	if (m_transferProtocol == TransferProtocol::OPCUA) {
		std::string std_address = std::string(p_address.utf8().get_data());
		std::string std_objectPath = std::string(p_objectPath.replace(".", "/").utf8().get_data());
		client->setVariableValueUpdateCallback((ICallback *)this);
		OPCUA::StatusCode sc = client->connect(std_address, std_objectPath, p_activateUpcalls);
		//std::cout << "uaclient connect_start statuscode=" << sc << std::endl;
		OS::get_singleton()->print("uaclient connect_start statuscode=%d\n", sc);
		if (sc == OPCUA::StatusCode::ALL_OK) {
			client->Start();
			//std::cout << "uaclient connected..." << std::endl;
			OS::get_singleton()->print("uaclient connected...\n");
			if (has_signal("callback_varmanager_connect"))
				emit_signal("callback_varmanager_connect");
		}
		return static_cast<VarManager::UAStatusCode>(sc);
	} else if (m_transferProtocol == TransferProtocol::MQTT) {
		Object *p_Mqtt = Engine::get_singleton()->get_singleton_object("g_Mqtt");
		if (p_Mqtt) {
			//g_Mqtt = dynamic_cast<MqttTransferInterface *>(p_Mqtt);
			g_Mqtt = (MqttTransferInterface *)(p_Mqtt);
			if (g_Mqtt) {
				Error err = g_Mqtt->init_bydict(m_mqttConfDict);
				if (err == Error::OK) {
					setAutoRefreshVarKV(true);
					bool bc = g_Mqtt->connect_start();
					bool bcr = g_Mqtt->connect_reqrsp_start();
					String remoteClientId = "remoteClientId_001";
					if ((String)m_mqttConfDict["@RemoteClientID"] != "")
						remoteClientId = (String) m_mqttConfDict["@RemoteClientID"];
					// todo: case1: get all variable current value from svc before subscribe to [DataChange/#] topic
					//       or
					//       case2: lazy get variable value when kv not contains the key
					// $file/{file_id}/init/{client_id}
					// $file/{file_id}/{offset}/{client_id}
					// $file/{file_id}/fin/{file_size}/{client_id}
					g_Mqtt->subscribe1("$file/+/+/"+remoteClientId, 1);
					g_Mqtt->subscribe1("$file/+/fin/+/" + remoteClientId, 1);
					g_Mqtt->req_rsp1("req/sync__var__", "{}", "rsp/sync__var__", remoteClientId, 1, 15 /*, bool p_auto_mode = true */);
					#ifdef  _WIN32
					Sleep(1000 * 3);
					#elif defined(__linux__)
					//sleep(1000 * 3);
					#endif
					g_Mqtt->req_rsp1("req/sync__varcrc__", "{}", "rsp/sync__varcrc__", remoteClientId, 1, 10 /*, bool p_auto_mode = true */);
					#ifdef  _WIN32
					Sleep(1000 * 2);
					#elif defined(__linux__)
					//sleep(1000 * 2);
					#endif

					if (bc && m_autoRefreshVarKVSwitch) {
						Object *p_KV = Engine::get_singleton()->get_singleton_object("g_KV");
						if (p_KV) {
							KVTransferInterface *g_KV = (KVTransferInterface *)(p_KV);
							if (g_KV) {
								m_mqRefreshKVPath = OS::get_singleton()->get_cache_path();
								// todo: sync __var__ from svc to gd local m_mqRefreshKVPath,and rename it to __varmq__
								g_KV->init(m_mqRefreshKVPath);
								g_KV->kvWithID1(1, "__varmq__", "" /*,"c:/kvtest/KV"*/);
								g_Mqtt->connect("MqttMsgReceived1", callable_mp(g_KV, &KVTransferInterface::onVarRefreshed));
							}
						}
						
						g_Mqtt->subscribe1("DataChange/#", 0);
					}
					
					if (bc && bcr) {
						return VarManager::UAStatusCode::ALL_OK;
					}
				}
			}
		}
	}
	return VarManager::UAStatusCode::UNDEFINED;
}

VarManager::UAStatusCode VarManager::connect_close() {
	// Always stop the background run thread before tearing down the client.
	// GenericClient::disconnect() returns ALL_OK (never DISCONNECTED), so the old
	// `if (sc == DISCONNECTED) client->Stop();` guard never ran — the run thread
	// was left alive and crashed at ~UAClient / delete client on shutdown.
	client->Stop();
	OPCUA::StatusCode sc = client->disconnect();
	if (sc == OPCUA::StatusCode::DISCONNECTED) {
		if (has_signal("callback_varmanager_disconnect"))
			emit_signal("callback_varmanager_disconnect");
	}
	return static_cast<VarManager::UAStatusCode>(sc);
}


Variant VarManager::getNodeId(const String &p_nodePath) {
	std::string std_nodePath = std::string(p_nodePath.replace(".", "/").utf8().get_data());
	return getNodeId_p(std_nodePath);
}

Variant VarManager::getNodeId_p(const std::string &std_nodePath) {
	if (client) {
		OPCUA::NodeId id = client->browseNodePath(std_nodePath);
		if (id.isNumericType())
			return Variant(id.getNumeric());
		else if (id.isStringType())
			return Variant(id.getSimpleName().c_str());
	}
	return Variant();
}

// Returns Array of Dictionary{"name": String, "class": int}
// class: 1=Object, 2=Variable
Array VarManager::getChildrenOfPath(const String &p_nodePath) {
	Array result;
	if (!client) return result;
	std::string std_path = std::string(p_nodePath.replace(".", "/").utf8().get_data());
	auto children = client->browseChildren(std_path);
	for (auto &kv : children) {
		Dictionary d;
		d["name"]  = String(kv.first.c_str());
		d["class"] = kv.second;
		result.push_back(d);
	}
	return result;
}

int VarManager::getStatus() {
	if (client) {
		return client->status();
	}
	return 99;
}

Variant VarManager::readValue(const String &p_variablePath) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	return readValue_p(std_variablePath);
	//OPCUA::Variant opcvv;
	//OPCUA::StatusCode retcode = client->readValue(std_variablePath, opcvv);
	////std::cout << "StatusCode:" << retcode << std::endl;
	////std::cout << std_variablePath << ": " << opcvv << std::endl;
	//OS::get_singleton()->print("readValue StatusCode: %d  %s=%s\n", retcode, std_variablePath, opcvv.toString().c_str());
	//// opcvv to value
	//Variant p_value;
	////std::cout << "readValue:" << std_variablePath << std::endl;
	//convertOPCVar2GDVar(opcvv, p_value);

	////if (value.get_type() == Variant::DICTIONARY) {
	////}
	////else if(value.get_type() == Variant::ARRAY) {
	////}
	////else {
	////}
	//return p_value;
}

Variant VarManager::readValue_p(const std::string &std_variablePath) {
    auto promise = std::make_shared<std::promise<std::vector<OPCUA::Variant>>>();
    auto future  = promise->get_future();
    postReadCmd({std_variablePath}, promise, /*is_sync=*/true);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(read_timeout_ms_);
    if (future.wait_until(deadline) != std::future_status::ready)
        return Variant();

    auto results = future.get();
    if (results.empty()) return Variant();
    Variant v;
    convertOPCVar2GDVar(results[0], v);
    return v;
}

Variant VarManager::readNextValue(const String &p_variablePath) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	return readNextValue_p(std_variablePath);
}

Variant VarManager::readNextValue_p(const std::string &std_variablePath) {
	OPCUA::Variant opcvv;
	OPCUA::StatusCode retcode = client->readNextValue(std_variablePath, opcvv);
	//std::cout << "StatusCode:" << retcode << std::endl;
	//std::cout << std_variablePath << ": " << opcvv << std::endl;
	OS::get_singleton()->print("readNextValue StatusCode: %d  %s=%s\n", retcode, std_variablePath.c_str(), opcvv.toString().c_str());
	if (retcode != OPCUA::StatusCode::ALL_OK)
		return Variant();
	// opcvv to value
	Variant p_value;
	//std::cout << "readValue:" << std_variablePath << std::endl;
	convertOPCVar2GDVar(opcvv, p_value);
	return p_value;
}

VarManager::UAStatusCode VarManager::readValueAsync(const String &p_variablePath) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	return readValueAsync_p(std_variablePath);
}

VarManager::UAStatusCode VarManager::readValueAsync_p(const std::string &std_variablePath) {
    postReadCmd({std_variablePath}, nullptr, /*is_sync=*/false);
    return VarManager::UAStatusCode::ALL_OK;
}

Vector<Variant> VarManager::readValues(const Vector<String> &p_variablePaths)
{
	std::vector<std::string> std_variablePaths;
	for (int i = 0; i < p_variablePaths.size(); ++i) {
		std::string std_variablePath = std::string(p_variablePaths[i].replace(".", "/").utf8().get_data());
		std_variablePaths.push_back(std_variablePath);
	}
	return readValues_p(std_variablePaths);
}

Vector<Variant> VarManager::readValues_p(const std::vector<std::string> &std_variablePaths)
{
    if (std_variablePaths.empty()) return Vector<Variant>();

    std::vector<std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>>> promises;
    promises.reserve(std_variablePaths.size());

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(read_timeout_ms_);

    {
        std::lock_guard<std::mutex> lk(cmd_mutex_);
        for (const auto &path : std_variablePaths) {
            auto p = std::make_shared<std::promise<std::vector<OPCUA::Variant>>>();
            promises.push_back(p);
            OpcCommand cmd;
            cmd.type    = OpcCmdType::ReadSync;
            cmd.paths   = {path};
            cmd.promise = p;
            cmd.expires = deadline;
            cmd_queue_.push(std::move(cmd));
        }
    }

    Vector<Variant> result;
    for (size_t i = 0; i < std_variablePaths.size(); i++) {
        auto fut = promises[i]->get_future();
        Variant v;
        if (fut.wait_until(deadline) == std::future_status::ready) {
            auto vals = fut.get();
            if (!vals.empty()) convertOPCVar2GDVar(vals[0], v);
        }
        result.push_back(v);
    }
    return result;
}

VarManager::UAStatusCode VarManager::readValuesAsync(const Vector<String> &p_variablePaths) {
	std::vector<std::string> std_variablePaths;
	for (int i = 0; i < p_variablePaths.size(); ++i) {
		std::string std_variablePath = std::string(p_variablePaths[i].replace(".", "/").utf8().get_data());
		std_variablePaths.push_back(std_variablePath);
	}
	return readValuesAsync_p(std_variablePaths);
}

VarManager::UAStatusCode VarManager::readValuesAsync_p(const std::vector<std::string> &std_variablePaths) {
    postReadCmd(std_variablePaths, nullptr, /*is_sync=*/false);
    return VarManager::UAStatusCode::ALL_OK;
}

VarManager::UAStatusCode VarManager::readHistoryDataAsync(const String &p_variablePath, const Variant &p_startTime, const Variant &p_endTime, bool p_returnBounds/* = false*/, const int p_numValuesPerNode/* = 10*/) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	OPCUA::Variant opc_startTime;
	convertGDVar2OPCVar(p_startTime, opc_startTime);
	OPCUA::Variant opc_endTime;
	convertGDVar2OPCVar(p_endTime, opc_endTime);
	OPCUA::StatusCode retcode = client->readHistoryData(std_variablePath, opc_startTime, opc_endTime, p_returnBounds, p_numValuesPerNode);
	return static_cast<VarManager::UAStatusCode>(retcode);
}

VarManager::UAStatusCode VarManager::writeValue(const String &p_variablePath, const Variant p_value) {
    std::string path = std::string(p_variablePath.replace(".", "/").utf8().get_data());
    OPCUA::Variant opcvv;
    convertGDVar2OPCVar(p_value, opcvv);

    auto promise = std::make_shared<std::promise<std::vector<OPCUA::Variant>>>();
    auto future  = promise->get_future();
    postWriteCmd({path}, {opcvv}, /*is_sync=*/true, promise);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(write_timeout_ms_);
    if (future.wait_until(deadline) != std::future_status::ready)
        return VarManager::UAStatusCode::ERROR_COMMUNICATION;

    auto results = future.get();
    if (results.empty()) return VarManager::UAStatusCode::ERROR_COMMUNICATION;
    return static_cast<VarManager::UAStatusCode>(
        (OPCUA::StatusCode)results[0].getValueAs<uint32_t>());
}

VarManager::UAStatusCode VarManager::writeValues(const Vector<String> &p_variablePaths, const Vector<Variant> p_values)
{
    int vpathSize = p_variablePaths.size();
    int vSize = p_values.size();
    if (vpathSize != vSize) {
        spdlog::get("uacpp_logger")->error("writeValues count mismatch: {0:d}!={1:d}", vpathSize, vSize);
        return VarManager::UAStatusCode::METHOD_INPUT_ARGUMENT_COUNT_MISMATCH;
    }
    std::vector<std::string> paths;
    std::vector<OPCUA::Variant> opcvvs;
    for (int i = 0; i < vpathSize; i++) {
        paths.push_back(std::string(p_variablePaths[i].replace(".", "/").utf8().get_data()));
        OPCUA::Variant v; convertGDVar2OPCVar(p_values[i], v); opcvvs.push_back(v);
    }
    auto promise = std::make_shared<std::promise<std::vector<OPCUA::Variant>>>();
    auto future  = promise->get_future();
    postWriteCmd(paths, opcvvs, /*is_sync=*/true, promise);

    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(write_timeout_ms_);
    if (future.wait_until(deadline) != std::future_status::ready)
        return VarManager::UAStatusCode::ERROR_COMMUNICATION;
    auto results = future.get();
    if (results.empty()) return VarManager::UAStatusCode::ERROR_COMMUNICATION;
    return static_cast<VarManager::UAStatusCode>(
        (OPCUA::StatusCode)results[0].getValueAs<uint32_t>());
}

VarManager::UAStatusCode VarManager::writeValueAsync(const String &p_variablePath, const Variant p_value) {
    std::string path = std::string(p_variablePath.replace(".", "/").utf8().get_data());
    OPCUA::Variant opcvv;
    convertGDVar2OPCVar(p_value, opcvv);
    postWriteCmd({path}, {opcvv}, /*is_sync=*/false, nullptr);
    return VarManager::UAStatusCode::ALL_OK;
}

VarManager::UAStatusCode VarManager::writeValuesAsync(const Vector<String> &p_variablePaths, const Vector<Variant> p_values)
{
    int vpathSize = p_variablePaths.size();
    int vSize = p_values.size();
    if (vpathSize != vSize) {
        spdlog::get("uacpp_logger")->error("writeValuesAsync count mismatch: {0:d}!={1:d}", vpathSize, vSize);
        return VarManager::UAStatusCode::METHOD_INPUT_ARGUMENT_COUNT_MISMATCH;
    }
    std::vector<std::string> paths;
    std::vector<OPCUA::Variant> opcvvs;
    for (int i = 0; i < vpathSize; i++) {
        paths.push_back(std::string(p_variablePaths[i].replace(".", "/").utf8().get_data()));
        OPCUA::Variant v; convertGDVar2OPCVar(p_values[i], v); opcvvs.push_back(v);
    }
    postWriteCmd(paths, opcvvs, /*is_sync=*/false, nullptr);
    return VarManager::UAStatusCode::ALL_OK;
}

void VarManager::convertOPCVar2GDVar(const OPCUA::Variant &src, Variant &dest) {
	if (src.isEmpty()) {
		//std::cout << "OPCUA::src isEmpty" <<  std::endl;
		spdlog::get("uacpp_logger")->warn("OPCUA::src isEmpty");
		return;
	}

	int typeIndex = src.getTypeIndex();
	//std::cout << "OPCUA::typeIndex=" << typeIndex << std::endl;
	//OS::get_singleton()->print("convertOPCVar2GDVar OPCUA::typeIndex=%d\n", typeIndex);
	//spdlog::get("uacpp_logger")->warn("OPCUA::typeIndex={0:d}", typeIndex);
	if (src.isScalar()) {
		//std::cout << "typeIndex: " << typeIndex << std::endl;
		if (typeIndex == UA_TYPES_BOOLEAN) {
			auto boolValue = src.getValueAs<bool>();
			//std::cout << "intValue: " << boolValue << std::endl;
			dest = Variant(boolValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=bool, src=({0:d}), dest= ({1}))", (int)boolValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_SBYTE) {
			// all numeric types that fit into 32 bit
			auto intValue = src.getValueAs<int16_t>();
			//std::cout << "intValue: " << intValue << std::endl;
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=int8_t, src=({0:d}), dest= ({1})", (int)intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_BYTE) {
			// all numeric types that fit into 32 bit
			auto intValue = src.getValueAs<uint16_t>();
			//std::cout << "intValue: " << intValue << std::endl;
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=uint8_t, src=({0:d}), dest= ({1})", intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_INT16) {
			// all numeric types that fit into 32 bit
			auto intValue = src.getValueAs<int16_t>();
			//std::cout << "intValue: " << intValue << std::endl;
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=int16_t, src=({0:d}), dest= ({1})", intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_UINT16) {
			// all numeric types that fit into 32 bit
			auto intValue = src.getValueAs<uint16_t>();
			//std::cout << "intValue: " << intValue << std::endl;
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=uint16_t, src=({0:d}), dest= ({1})", intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_INT32) {
			// all numeric types that fit into 32 bit
			auto intValue = src.getValueAs<int32_t>();
			//std::cout << "intValue: " << intValue << std::endl;
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=int32_t, src=({0:d}), dest= ({1})", intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_UINT32) {
			auto uintValue = src.getValueAs<uint32_t>();
			dest = Variant(uintValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=uint32_t, src=({0:d}), dest= ({1})", uintValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_INT64) {
			auto intValue = src.getValueAs<int64_t>();
			dest = Variant(intValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=int64_t, src=({0:d}), dest= ({1})", intValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_UINT64) {
			auto uintValue = src.getValueAs<uint64_t>();
			dest = Variant(uintValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=uint64_t, src=({0:d}), dest= ({1})", uintValue, String(dest).utf8().get_data());
		} else if (typeIndex <= UA_TYPES_FLOAT) {
			// floating types
			auto floatValue = src.getValueAs<float>();
			dest = Variant(floatValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=float, src=({0:f}), dest= ({1})", floatValue, String(dest).utf8().get_data());
		} else if (typeIndex <= UA_TYPES_DOUBLE) {
			// double types
			auto dblValue = src.getValueAs<double>();
			dest = Variant(dblValue);
			//spdlog::get("uacpp_logger")->info("convertOPCVar2GDVar(type=double, src=({0:f}), dest= ({1})", dblValue, String(dest).utf8().get_data());
		} else if (typeIndex == UA_TYPES_STRING || typeIndex == UA_TYPES_BYTESTRING) {
#ifdef  _WIN32
			// todo: 需要判断字符中是否含有宽字符，判断后智能处理 case 1, case 4
			UA_String *uaStringPtr = static_cast<UA_String *>(src.getInternalValuePtr()->data);
			if (uaStringPtr == 0 || uaStringPtr->length == 0) {
				dest = Variant("");
				return;
			}

			//const char *tmpchar = reinterpret_cast<const char *>(uaStringPtr->data);
			//int len = strlen(tmpchar);
			UAVarUtil util;
			//UAVarUtil::CODING code = util.GetCoding((unsigned char*)tmpchar, len);
			UAVarUtil::CODING code = util.GetCoding((unsigned char*)uaStringPtr->data, uaStringPtr->length);
			std::string s(reinterpret_cast<const char *>(uaStringPtr->data), uaStringPtr->length);
			// case 1: 直接
			if (code == UAVarUtil::CODING::GBK) {
				//dest = Variant(reinterpret_cast<const char *>(uaStringPtr->data));
				
				//std::cout << "uastring to GBK std::string=" << s << std::endl;
				dest = Variant(s.c_str());
			}

			//String gdstr = reinterpret_cast<const char *>(uaStringPtr->data);
			//std::string t_str = src.toString();
			////setup converter
			//typedef std::codecvt_utf8<wchar_t> convert_type;
			//std::wstring_convert<convert_type, wchar_t> converter;
			////use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
			//std::wstring ws = converter.from_bytes(t_str);
			//dest = Variant(ws.c_str());
			
			// case 2:
			//UA_String *uaStringPtr = static_cast<UA_String *>(src.getInternalValueCopy().data);

			// case 3:
			//std::string s = src.toString();
			//std::cout << "OPCUA::src.tostring()=" << s1 << std::endl;

			// case 4:
			//setup converter when 中文
			else if (code == UAVarUtil::CODING::UTF8) {
				//using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
				//std::wstring_convert<convert_type, typename std::wstring::value_type> converter;
				////use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
				////std::wstring ws = converter.from_bytes(reinterpret_cast<const char *>(uaStringPtr->data));
				//std::wstring ws = converter.from_bytes(reinterpret_cast<const char *>(s.c_str()));
				std::wstring ws = utf8_to_wide(s);

				//dest = Variant(wc_to_utf8(ws.c_str()));
				dest = Variant(String(ws.c_str()));
				//dest = Variant(wide_to_utf8(ws).c_str());


				//std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
				//std::wstring wString = conv.from_bytes(s.c_str()); // utf-8 => wstring
				//std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>
				//		convert(new std::codecvt<wchar_t, char, std::mbstate_t>("CHS"));
				//std::string str = convert.to_bytes(wString); // wstring => string
				//
				//dest = Variant(str.c_str());
			}
			
			// reinterpret cast is quite a sledge hammer here
			//std::string s(reinterpret_cast<const char *>(uaStringPtr->data), uaStringPtr->length);
			//std::cout << "uastring to std::string=" << s << std::endl;
			//dest = Variant(s.c_str());
#elif defined(__linux__)
			//UA_String *pOrig = static_cast<UA_String *>(src.getInternalValuePtr()->data);
			//for (unsigned int i = 0; i < pOrig->length; ++i)
			//{
			//	printf("0x%x ", pOrig->data[i]);
			//}
			//printf("\n%.*s\n\n", pOrig->length, pOrig->data);


			std::string s = src.toString();
			//std::wstring ws = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(s);
			
			//UA_String *uaStringPtr = static_cast<UA_String *>(src.getInternalValuePtr()->data);
			//String ss;
			//if (ss.parse_utf8(s.c_str(), s.length())) {

				std::string strLocale = setlocale(LC_ALL, "");
				const char* pSrc = s.c_str();
				unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
				wchar_t* szDest = new wchar_t[iDestSize];
				wmemset(szDest, 0, iDestSize);
				mbstowcs(szDest, pSrc, iDestSize);
				std::wstring ws = szDest;
				delete[]szDest;
				setlocale(LC_ALL, strLocale.c_str());

				dest = String(ws.c_str());
				//std::cout << "utf8 std::string=" << s << " String= " << ws.c_str() << s.length() << std::endl;
				//OS::get_singleton()->print("utf8 String=%s std::string=%s\n", ss.utf8().get_data(), src.toString().c_str());
			//}
			//else {
			//	dest = String(s.c_str());
			//	std::cout << "other std::string=" << s << std::endl;
			//	OS::get_singleton()->print("other std::string=%s\n", s.c_str());
			//}
			//dest = Variant(String::utf8(reinterpret_cast<const char *>(uaStringPtr->data), uaStringPtr->length));
			//dest = Variant(reinterpret_cast<const char *>(uaStringPtr->data));

#endif	
		} else if (typeIndex == UA_TYPES_QUALIFIEDNAME) {
			UA_QualifiedName *uaQname = static_cast<UA_QualifiedName *>(src.getInternalValuePtr()->data);
			std::string index = std::to_string(uaQname->namespaceIndex);
			std::string simple_name(reinterpret_cast<const char *>(uaQname->name.data), uaQname->name.length);
			std::string s = index + ":" + simple_name;
			dest = Variant(s.c_str());

		} else if (typeIndex == UA_TYPES_LOCALIZEDTEXT) {
#ifdef  _WIN32
			UA_LocalizedText *uaText = static_cast<UA_LocalizedText *>(src.getInternalValuePtr()->data);
			if (uaText == 0 || uaText->text.length == 0) {
				dest = Variant("");
				return;
			}
			UAVarUtil util;
			//UAVarUtil::CODING code = util.GetCoding((unsigned char*)tmpchar, len);
			UAVarUtil::CODING code = util.GetCoding((unsigned char*)uaText->text.data, uaText->text.length);
			std::string s(reinterpret_cast<const char *>(uaText->text.data), uaText->text.length);
			// case 1: 直接
			if (code == UAVarUtil::CODING::GBK) {
				//std::cout << "uastring to GBK std::string=" << s << std::endl;
				spdlog::get("uacpp_logger")->info("uastring to GBK std::string=({0}))", s);
				dest = Variant(s.c_str());
				
			}
			//setup converter when 中文
			else if (code == UAVarUtil::CODING::UTF8) {
				using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
				std::wstring_convert<convert_type, typename std::wstring::value_type> converter;
				try
				{
					//std::wstring ws = converter.from_bytes(reinterpret_cast<const char *>(s.c_str()));
					//std::wcout << "uastring to UTF8 std::string=" << ws << std::endl;
					//dest = Variant(wc_to_utf8(ws.c_str()));

					std::wstring ws = utf8_to_wide(s);
					dest = Variant(String(ws.c_str()));
				}
				catch (const std::range_error& /*e*/)
				{
					std::wstring ws1 = converter.from_bytes(s.substr(0, converter.converted()));
					std::cout << "\nUTF-8 failed after producing " << std::dec << ws1.size()
						<< " characters:\n" << std::showbase << std::hex;
					for (char16_t c : ws1)
						std::cout << static_cast<std::uint16_t>(c) << ' ';
					std::cout << '\n';
				}
			}

			// reinterpret cast is quite a sledge hammer here
			//std::string s = std::string(reinterpret_cast<const char *>(uaText->text.data), uaText->text.length);
			//dest = Variant(s.c_str());
#elif defined(__linux__)
			//UA_LocalizedText *pOrig = (UA_LocalizedText*)(src.getInternalValuePtr()->data);

			//for (unsigned int i = 0; i < pOrig->text.length; ++i)
			//{
			//	printf("0x%x ", pOrig->text.data[i]);
			//}
			//printf("\n%.*s\n\n", pOrig->text.length, pOrig->text.data);

			std::string s = src.toString();

			std::string strLocale = setlocale(LC_ALL, "");
			const char* pSrc = s.c_str();
			unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
			wchar_t* szDest = new wchar_t[iDestSize];
			wmemset(szDest, 0, iDestSize);
			mbstowcs(szDest, pSrc, iDestSize);
			std::wstring ws = szDest;
			delete[]szDest;
			setlocale(LC_ALL, strLocale.c_str());

			dest = String(ws.c_str());
			//UA_LocalizedText *uaText = static_cast<UA_LocalizedText *>(src.getInternalValuePtr()->data);
			//dest = Variant(reinterpret_cast<const char *>(uaText->text.data));
#endif	
		} else if (typeIndex == UA_TYPES_DATETIME) {
			UA_DateTime now = UA_DateTime_now();
			//std::cout << "now=" << now << std::endl;
			auto raw_date = src.getValueAs<UA_DateTime>();
			//std::cout << "dtUTCValue=" << raw_date << std::endl;
			UA_Int64 tOffset = UA_DateTime_localTimeUtcOffset();
			UA_DateTimeStruct dts = UA_DateTime_toStruct(raw_date + tOffset);
			/*printf("date is: %04u-%02u-%02u %02u:%02u:%02u.%03u",
					dts.year, dts.month, dts.day, dts.hour, dts.min, dts.sec, dts.milliSec);*/
			//UA_DateTime dt = UA_DateTime_fromUnixTime(raw_date);
			UA_DateTime dtlocal = UA_DateTime_fromStruct(dts);
			//printf("%Id-%lld-%I64d", dtlocal, dtlocal, dtlocal);
			UA_Int64 unix_time = UA_DateTime_toUnixTime(dtlocal);

			// UA_DateTime_now()加上偏移值，得到正确的系统时间
			//UA_DateTimeStruct dts = UA_DateTime_toStruct(dtUTCValue + tOffset);

			dest = Variant(unix_time);
		} else if (typeIndex == UA_TYPES_BYTESTRING) {
			//std::cout << "UA_TYPES_BYTESTRING" << std::endl;
			UA_ByteString *uaByteStringPtr = static_cast<UA_ByteString *>(src.getInternalValuePtr()->data);

			// case1: base64
			size_t size = UA_calcSizeBinary(uaByteStringPtr, &UA_TYPES[UA_TYPES_BYTESTRING]);
			//std::cout << "ua bytestring length=" << size << std::endl;
			OS::get_singleton()->print("ua bytestring length=%d\n", size);
			UA_String *uaString = new UA_String;
			//UA_StatusCode rv = UA_ByteString_toBase64(uaByteStringPtr, uaString);
			//std::cout << "UA_ByteString_toBase64 rv=" << rv << std::endl;
			//if (rv == UA_STATUSCODE_GOOD) {
			UA_StatusCode rv = UA_decodeBinary(uaByteStringPtr, uaString, &UA_TYPES[UA_TYPES_STRING], NULL);
			//std::cout << "UA_decodeBinary rv=" << rv << std::endl;
			OS::get_singleton()->print("UA_decodeBinary rv=%d\n", rv);
			//UA_String_decodeBinary(uaByteStringPtr, 0, &uaString);

			std::string s(reinterpret_cast<const char *>(uaString->data), uaString->length);
			dest = Variant(s.c_str());

			//}

			// case2: string
			//std::string s(reinterpret_cast<const char *>(uaByteStringPtr->data), uaByteStringPtr->length);
			////std::cout << "uastring to std::string=" << s << std::endl;
			//dest = Variant(s.c_str());

			//UA_ByteString myByteString = UA_BYTESTRING(reinterpret_cast<const char *>(uaStringPtr->data));
		} else if (typeIndex == UA_TYPES_NODEID) {
#ifdef  _WIN32
			UA_NodeId *uaNodeIdPtr = static_cast<UA_NodeId *>(src.getInternalValuePtr()->data);

			// case 1: 按照类型转换
			if (uaNodeIdPtr->identifierType == UA_NODEIDTYPE_STRING) {
				std::string s((const char *)uaNodeIdPtr->identifier.string.data, uaNodeIdPtr->identifier.string.length);
				//std::cout << "UA_TYPES_NODEID to std::string=" << s << "length=" << uaNodeIdPtr->identifier.string.length << std::endl;
				spdlog::get("uacpp_logger")->info("UA_TYPES_NODEID to std::string={0}, length={1:d}", s, uaNodeIdPtr->identifier.string.length);
				dest = String(s.c_str());
			} else if (uaNodeIdPtr->identifierType == UA_NODEIDTYPE_NUMERIC) {
				//std::cout << "UA_TYPES_NODEID to numeric=" << uaNodeIdPtr->identifier.numeric << std::endl;
				spdlog::get("uacpp_logger")->info("UA_TYPES_NODEID to numeric={0:d}", uaNodeIdPtr->identifier.numeric);
				dest = Variant(uaNodeIdPtr->identifier.numeric);
			}

			// case 2: 强制字符串转换
			//UA_String uaString;
			//UA_StatusCode rv = UA_NodeId_print(uaNodeIdPtr, &uaString);
			//std::string s(reinterpret_cast<const char *>(uaString.data), uaString.length);
			//dest = Variant(s.c_str());
#elif defined(__linux__)
			std::string s = src.toString();
			dest = String(s.c_str());
#endif
		}
	} else {
		// is array type
		//std::string result;
		//if( typeIndex == UA_TYPES_BOOLEAN) {
		//	auto boolValues = getArrayValuesAs<bool>();
		//	for(size_t i=0; i<boolValues.size(); ++i) {
		//		if(boolValues[i] == true) {
		//			result = result + std::string("true");
		//		} else {
		//			result = result + std::string("false");
		//		}
		//		if(i < boolValues.size()-1) {
		//			result = result + ", ";
		//		}
		//	}
		if (typeIndex == UA_TYPES_BOOLEAN) {
			auto boolValues = src.getArrayValuesAs<bool>();
			Array tmpArr;
			for (size_t i = 0; i < boolValues.size(); ++i) {
				tmpArr.append(Variant(boolValues[i]));
			}
			dest = tmpArr;
		}
		else if (typeIndex == UA_TYPES_SBYTE) {
			// all numeric types that fit into 32 bit
			auto intValues = src.getArrayValuesAs<int16_t>();
			Array tmpArr;
			for (size_t i = 0; i < intValues.size(); ++i) {
				tmpArr.append(Variant(intValues[i]));
			}
			dest = tmpArr;
		}
		else if (typeIndex == UA_TYPES_BYTE) {
			// all numeric types that fit into 32 bit
			auto intValues = src.getArrayValuesAs<uint16_t>();
			Array tmpArr;
			for (size_t i = 0; i < intValues.size(); ++i) {
				tmpArr.append(Variant(intValues[i]));
			}
			dest = tmpArr;
		}
		else if (typeIndex == UA_TYPES_INT16) {
			// all numeric types that fit into 32 bit
			auto intValues = src.getArrayValuesAs<int16_t>();
			Array tmpArr;
			for (size_t i = 0; i < intValues.size(); ++i) {
				tmpArr.append(Variant(intValues[i]));
			}
			dest = tmpArr;
		}
		else if (typeIndex == UA_TYPES_UINT16) {
			// all numeric types that fit into 32 bit
			auto intValues = src.getArrayValuesAs<uint16_t>();
			Array tmpArr;
			for (size_t i = 0; i < intValues.size(); ++i) {
				tmpArr.append(Variant(intValues[i]));
			}
			dest = tmpArr;
		}
		else if( typeIndex == UA_TYPES_INT32 ) {
			// all numeric types that fit into 32 bit
			auto intValues = src.getArrayValuesAs<int32_t>();
			Array tmpArr;
			for (size_t i = 0; i < intValues.size(); ++i) {
				tmpArr.append(Variant(intValues[i]));
			}
			dest = tmpArr;
		}
		else if( typeIndex == UA_TYPES_UINT32 ) {
			auto uintValues = src.getArrayValuesAs<uint32_t>();
			Array tmpArr;
			for(auto &uival: uintValues) {
				tmpArr.append(Variant(uival));
			}
			dest = tmpArr;
		}
		else if( typeIndex == UA_TYPES_INT64 ) {
			auto intValues = src.getArrayValuesAs<int64_t>();
			Array tmpArr;
			for(auto &ival: intValues) {
				tmpArr.append(Variant(ival));
			}
			dest = tmpArr;
		}
		else if( typeIndex == UA_TYPES_UINT64 ) {
			auto uintValues = src.getArrayValuesAs<uint64_t>();
			Array tmpArr;
			for(auto &uival: uintValues) {
				tmpArr.append(Variant(uival));
			}
			dest = tmpArr;
		}
		else if (typeIndex <= UA_TYPES_FLOAT) {
			auto floatValues = src.getArrayValuesAs<float>();
			Array tmpArr;
			for (auto &fval : floatValues) {
				tmpArr.append(Variant(fval));
			}
			dest = tmpArr;
		}
		else if( typeIndex <= UA_TYPES_DOUBLE ) {
			// floating types
			auto dblValues = src.getArrayValuesAs<double>();
			Array tmpArr;
			for(auto &dblval: dblValues) {
				tmpArr.append(Variant(dblval));
			}
			dest = tmpArr;
		}
		//else if(typeIndex == UA_TYPES_STRING) {
		//	auto strValues = getArrayValuesAs<std::string>();
		//	for(auto &str: strValues) {
		//		result = result + str;
		//		if(&str != &strValues.back()) {
		//			result = result + ", ";
		//		}
		//	}
		//}
		else if (typeIndex == UA_TYPES_STRING) {
			auto strValues = src.getArrayValuesAs<std::string>();
			Array tmpArr;
			for (auto &str : strValues) {
				const char *tmpchar = reinterpret_cast<const char *>(str.data());
				int len = strlen(tmpchar);
				if (len == 0) {
					tmpArr.append(Variant(""));
					continue;
				}
#ifdef  _WIN32
				UAVarUtil util;
				UAVarUtil::CODING code = util.GetCoding((unsigned char*)tmpchar, len);

				// case 1: 直接
				if (code == UAVarUtil::CODING::GBK) {
					tmpArr.append(Variant(tmpchar));
				}

				// case 4:
				//setup converter when 中文
				else {
					//using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
					//std::wstring_convert<convert_type, typename std::wstring::value_type> converter;
					////use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
					//std::wstring ws = converter.from_bytes(tmpchar);
					//tmpArr.append(Variant(wc_to_utf8(ws.c_str())));
					std::wstring ws = utf8_to_wide(std::string(tmpchar,len));
					tmpArr.append(Variant(String(ws.c_str())));
				}
#elif defined(__linux__)
				std::string s = std::string(tmpchar,len);
				std::string strLocale = setlocale(LC_ALL, "");
				const char* pSrc = s.c_str();
				unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
				wchar_t* szDest = new wchar_t[iDestSize];
				wmemset(szDest, 0, iDestSize);
				mbstowcs(szDest, pSrc, iDestSize);
				std::wstring ws = szDest;
				delete[]szDest;
				setlocale(LC_ALL, strLocale.c_str());
				tmpArr.append(Variant(String(ws.c_str())));
#endif
			}
			dest = tmpArr;
		}
		//return result;
	}

	//return std::string();
}

// intmode
//	= 0: int64_t
//	= 1: int32_t
void VarManager::convertGDVar2OPCVar(const Variant &src, OPCUA::Variant &dest, int intmode/* = 0*/) {
	int typeIndex = src.get_type();
	if (typeIndex == Variant::NIL) {
		return;
	}

#if defined(_WIN64) || defined(__amd64) || defined(__aarch64__)
#define __MASHINE_64BITS__		 // _WIN64 == 64
	intmode = 1;
#else
#define __MASHINE_32BITS__		 // _WIN64 == 32

#endif

#ifdef __linux__
	intmode = 1;
#endif

	//std::cout << "gd typeIndex: " << typeIndex << std::endl;
	//OS::get_singleton()->print("convertGDVar2OPCVar gdvar typeIndex=%d\n", typeIndex);
	if (typeIndex == Variant::BOOL) {
		dest.setValueFrom<bool>((bool)src);
	} else if (typeIndex == Variant::INT && intmode == 0) {
		dest.setValueFrom<int64_t>((int64_t)src);
	} else if (typeIndex == Variant::INT && intmode == 1) {
		dest.setValueFrom<int32_t>((int32_t)src);
	} else if (typeIndex == Variant::FLOAT) {
		dest.setValueFrom<double>((double)src);
	} else if (typeIndex == Variant::STRING) {
		dest.setValueFrom(std::string(((String)src).utf8().get_data()));
	}
}

VarManager::UAStatusCode VarManager::invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments) {
    if (m_transferProtocol == TransferProtocol::OPCUA) {
        std::vector<OPCUA::Variant> ins(p_inputArguments.size());
        for (int i = 0; i < p_inputArguments.size(); i++) {
            convertGDVar2OPCVar(p_inputArguments[i], ins[i]);
        }
        std::string method_path = std::string(p_nodePath.replace(".", "/").utf8().get_data());

        auto promise = std::make_shared<std::promise<std::vector<OPCUA::Variant>>>();
        auto future  = promise->get_future();

        OpcCommand cmd;
        cmd.type        = OpcCmdType::InvokeSync;
        cmd.method_path = method_path;
        cmd.inputs      = ins;
        cmd.promise     = promise;
        cmd.expires     = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(invoke_timeout_ms_);
        {
            std::lock_guard<std::mutex> lk(cmd_mutex_);
            cmd_queue_.push(std::move(cmd));
        }

        auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(invoke_timeout_ms_);
        if (future.wait_until(deadline) != std::future_status::ready)
            return VarManager::UAStatusCode::ERROR_COMMUNICATION;

        auto outs = future.get();
        p_outputArguments.resize(outs.size());
        for (int j = 0; j < (int)outs.size(); j++) {
            Variant v; convertOPCVar2GDVar(outs[j], v); p_outputArguments[j] = v;
        }
        return VarManager::UAStatusCode::ALL_OK;
    } else if (m_transferProtocol == TransferProtocol::MQTT) {
		OPCUA::StatusCode sc = OPCUA::StatusCode::ALL_OK;
		// todo
		if (g_Mqtt) {
			// example: Modules/g_Window/open
			if (p_nodePath.begins_with("Modules/")) {
				String methodPath = p_nodePath.replace_first("Modules/", "req/");
				String remoteClientId = "remoteClientId_001";
				if ((String)m_mqttConfDict["@RemoteClientID"] != "")
					remoteClientId = (String)m_mqttConfDict["@RemoteClientID"];
				Error err = g_Mqtt->invokeMethod(methodPath, p_inputArguments, p_outputArguments, remoteClientId);
				if (err != Error::OK)
					return VarManager::UAStatusCode::METHOD_BAD_CALL;
			}
		}
		return static_cast<VarManager::UAStatusCode>(sc);
	}

	return VarManager::UAStatusCode::UNDEFINED;
}

VarManager::UAStatusCode VarManager::invokeMethodAsync(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments/* = Array()*/, Variant p_cbObject/* = Variant()*/, const String &p_cbFuncName/* = ""*/) {
    std::vector<OPCUA::Variant> ins(p_inputArguments.size());
    for (int i = 0; i < p_inputArguments.size(); i++)
        convertGDVar2OPCVar(p_inputArguments[i], ins[i]);

    ObjectID cb_obj_id{};
    if (!p_cbObject.is_null() && p_cbObject.get_type() == Variant::OBJECT) {
        Object *obj = p_cbObject.get_validated_object();
        if (obj) cb_obj_id = obj->get_instance_id();
    }

    OpcCommand cmd;
    cmd.type         = OpcCmdType::InvokeAsync;
    cmd.method_path  = std::string(p_nodePath.replace(".", "/").utf8().get_data());
    cmd.inputs       = ins;
    cmd.cb_obj_id    = cb_obj_id;
    cmd.cb_func_name = p_cbFuncName;
    cmd.expires      = std::chrono::steady_clock::now() +
                       std::chrono::milliseconds(invoke_timeout_ms_);
    {
        std::lock_guard<std::mutex> lk(cmd_mutex_);
        cmd_queue_.push(std::move(cmd));
    }
    return VarManager::UAStatusCode::ALL_OK;
}

VarManager::UAStatusCode VarManager::subscribe(const String &p_variablePath) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	OPCUA::StatusCode sc = client->subscribe(std_variablePath);
	return static_cast<VarManager::UAStatusCode>(sc);
}
VarManager::UAStatusCode VarManager::unSubscribe(const String &p_variablePath) {
	std::string std_variablePath = std::string(p_variablePath.replace(".", "/").utf8().get_data());
	OPCUA::StatusCode sc = client->unSubscribe(std_variablePath);
	return static_cast<VarManager::UAStatusCode>(sc);
}

VarManager::UAStatusCode VarManager::subscribes(const Vector<String> &p_variablePaths, const String &p_aliasName) {
	std::vector<std::string> std_variablePaths;
	for (int i = 0; i < p_variablePaths.size(); ++i) {
		std::string std_variablePath = std::string(p_variablePaths[i].replace(".", "/").utf8().get_data());
		std_variablePaths.push_back(std_variablePath);
	}
	std::string std_aliasName = std::string(p_aliasName.utf8().get_data());
	OPCUA::StatusCode sc = client->subscribes(std_variablePaths, std_aliasName);
	return static_cast<VarManager::UAStatusCode>(sc);
}

VarManager::UAStatusCode VarManager::subscribesById(const Vector<String> &p_variablePaths, const Variant &p_id) {
	//std::vector<std::string> std_variablePaths;
	for (int i = 0; i < p_variablePaths.size(); ++i) {
		std::string std_variablePath = std::string(p_variablePaths[i].replace(".", "/").utf8().get_data());
		//std_variablePaths.push_back(std_variablePath);
		if (!p_id.is_null() && p_id.get_type() == Variant::OBJECT) {
			Object* obj = p_id.get_validated_object();
			if (obj) {
				ObjectID iid = obj->get_instance_id();
				register_vars(p_variablePaths[i].replace(".", "/"), iid, true);
			}
		}
	}
	//std::string std_aliasName = std_string_format("%d", p_id);//std::string(String((int)p_id).utf8().get_data());
	//OPCUA::StatusCode sc = client->subscribes(std_variablePaths, std_aliasName);
	//return static_cast<VarManager::UAStatusCode>(sc);
	return VarManager::UAStatusCode::ALL_OK;
}

VarManager::UAStatusCode VarManager::unSubscribes(const String &p_aliasName) {
	std::string std_aliasName = std::string(p_aliasName.utf8().get_data());
	OPCUA::StatusCode sc = client->unSubscribes(std_aliasName);
	return static_cast<VarManager::UAStatusCode>(sc);
}

VarManager::UAStatusCode VarManager::commitSubscribesById(const String &p_aliasName) {

	commitUnSubscribesById(p_aliasName);
	std::vector<std::string> std_variablePaths;
	Array keys = varpath_ids_dict.keys();
	if (keys.size()) {
		//Array varpaths;
		//std::cout << "keys.size()=" << keys.size() << std::endl;
		
		for (int i = 0; i < keys.size(); i++) {
			Variant p_varpath = keys[i];
			//std::cout << String(p_varpath).utf8().get_data() << std::endl;

			if (p_varpath.get_type() == Variant::Type::STRING) {
				String vp = String(p_varpath);
				if (varpath_subed_dict.has(vp))
				{
					if (int(varpath_subed_dict[vp]) == 1)
						continue;
				}

				std::string std_variablePath = std::string(vp.utf8().get_data());
				//std::cout << "p_var type=" << p_varpath.get_type() << " value=" << std_variablePath << std::endl;
				//OS::get_singleton()->print("p_var type = %d value=%s\n", p_varpath.get_type(), std_variablePath.c_str());
				std_variablePaths.push_back(std_variablePath);
			}
		}
	}

	OS::get_singleton()->print("commitSubscribesById keys.size()=%d\n", std_variablePaths.size());

	std::string std_aliasName = std::string(p_aliasName.utf8().get_data());
	OPCUA::StatusCode sc = client->subscribes(std_variablePaths, std_aliasName);
	if (sc == OPCUA::StatusCode::ALL_OK)
	{
		if (keys.size()) {
			for (int i = 0; i < keys.size(); i++) {
				Variant p_varpath = keys[i];
				//std::cout << String(p_varpath).utf8().get_data() << std::endl;

				if (p_varpath.get_type() == Variant::Type::STRING) {
					String vp = String(p_varpath);
					if (varpath_subed_dict.has(vp))
					{
						varpath_subed_dict[vp] = 1;
					}
				}
			}

			aliasName_varpath_ids_dict_dict[p_aliasName] = varpath_ids_dict.duplicate(true);
		}
	}
	return static_cast<VarManager::UAStatusCode>(sc);
}

//std::string &VarManager::std_string_format(const char *_Format, ...) {
//	std::string tmp;
//
//	va_list marker = nullptr;
//	va_start(marker, _Format);
//
//	size_t num_of_chars = _vscprintf(_Format, marker);
//
//	if (num_of_chars > tmp.capacity()) {
//		tmp.resize(num_of_chars + 1);
//	} 
//
//	vsprintf_s((char *)tmp.data(), tmp.capacity(), _Format, marker);
//
//	va_end(marker);
//
//	return tmp;
//}

VarManager::UAStatusCode VarManager::unSubscribesById(const Variant &p_id) {
	//std::string std_aliasName = std_string_format("%d", p_id); //std::string(String((int)p_id).utf8().get_data());
	//OPCUA::StatusCode sc = client->unSubscribes(std_aliasName);
	//if (sc == OPCUA::StatusCode::ALL_OK)
	if (!p_id.is_null() && p_id.get_type() == Variant::OBJECT) {
		Object* obj = p_id.get_validated_object();
		if (obj) {
			ObjectID iid = obj->get_instance_id();
			unregister_vars(iid, true);
		}
	}
	//return static_cast<VarManager::UAStatusCode>(sc);
	return VarManager::UAStatusCode::ALL_OK;
}

VarManager::UAStatusCode VarManager::commitUnSubscribesById(const String &p_aliasName) {

	if (aliasName_varpath_ids_dict_dict.has(p_aliasName))
	{
		Dictionary __varpath_ids_dict = aliasName_varpath_ids_dict_dict[p_aliasName];
		Array values = __varpath_ids_dict.values();
		if (values.size()) {
			for (int i = 0; i < values.size(); i++) {
				ObjectID p_id = (ObjectID)(uint64_t)values[i];
				unSubscribesById(p_id);
			}
		}
	}
	std::string std_aliasName = std::string(p_aliasName.utf8().get_data());
	OPCUA::StatusCode sc = client->unSubscribes(std_aliasName);
	if (sc == OPCUA::StatusCode::ALL_OK)
		aliasName_varpath_ids_dict_dict.erase(p_aliasName);
	return static_cast<VarManager::UAStatusCode>(sc);
}

unsigned int VarManager::subscribeEvent(const Vector<String> &p_eventSelections /* = String("Message,Severity").split(",")*/, const String &p_nodePath/* = ""*/, Variant p_cbObject/* = Variant()*/, const String &p_cbFuncName/* = ""*/) {
	Vector<String> actual_selections = p_eventSelections;
	if (actual_selections.is_empty()) {
		actual_selections.push_back("Message");
		actual_selections.push_back("Severity");
		// 或者保留原写法的：actual_selections = String("Message,Severity").split(",");
	}
	std::vector<std::string> std_eventSelections;
	for (int i = 0; i < actual_selections.size(); ++i) {
		std::string std_eventSelection = std::string(actual_selections[i].utf8().get_data());
		std_eventSelections.push_back(std_eventSelection);
		//std::cout << "subscribeEvent: " << std_eventSelection << std::endl;
		OS::get_singleton()->print("subscribeEvent: %s\n", std_eventSelection.c_str());
	}
	std::string std_eventNodePath = std::string(p_nodePath.utf8().get_data());
	int ret = client->subscribeEvent(std_eventSelections, std_eventNodePath);
	if (ret > 0)
	{
		if (!p_cbObject.is_null() && p_cbObject.get_type() == Variant::OBJECT)
		{
			Object* obj = p_cbObject.get_validated_object();
			if (obj) {
				ObjectID objID = obj->get_instance_id();
				uint32_t evtSubId = ret;
				Array vArray;
				vArray.push_back(objID);
				vArray.push_back(p_cbFuncName);
				subscribedEventID_ObjectID_funcName_dict[evtSubId] = vArray;
			}
		}
	}
	return ret;
}

VarManager::UAStatusCode VarManager::unSubscribeEvent(const unsigned int &p_eventId) {
	OPCUA::StatusCode sc = client->unSubscribeEvent(p_eventId);
	if(sc == OPCUA::StatusCode::ALL_OK)
		subscribedEventID_ObjectID_funcName_dict.erase((uint32_t)p_eventId);
	return static_cast<VarManager::UAStatusCode>(sc);
}

void VarManager::postReadCmd(
        const std::vector<std::string> &paths,
        const std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>> &promise,
        bool is_sync) {
    auto expires = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(read_timeout_ms_);
    std::lock_guard<std::mutex> lk(cmd_mutex_);
    for (const auto &path : paths) {
        OpcCommand cmd;
        cmd.type    = is_sync ? OpcCmdType::ReadSync : OpcCmdType::ReadAsync;
        cmd.paths   = {path};
        cmd.promise = is_sync ? promise : nullptr;
        cmd.expires = expires;
        cmd_queue_.push(std::move(cmd));
    }
}

void VarManager::postWriteCmd(
        const std::vector<std::string> &paths,
        const std::vector<OPCUA::Variant> &values,
        bool is_sync,
        const std::shared_ptr<std::promise<std::vector<OPCUA::Variant>>> &promise) {
    auto expires = std::chrono::steady_clock::now() +
                   std::chrono::milliseconds(write_timeout_ms_);
    OpcCommand cmd;
    cmd.type    = is_sync ? OpcCmdType::WriteSync : OpcCmdType::WriteAsync;
    cmd.paths   = paths;
    cmd.values  = values;
    cmd.promise = promise;
    cmd.expires = expires;
    std::lock_guard<std::mutex> lk(cmd_mutex_);
    cmd_queue_.push(std::move(cmd));
}

void VarManager::variableChanged_p(const String &varName, const OPCUA::Variant &value)
{
	Variant vv;
	convertOPCVar2GDVar(value, vv);

	return notifyRelatedControls(varName, vv);
}

void VarManager::drainCommandQueue() {
    if (!client) return;

    std::queue<OpcCommand> local;
    {
        std::lock_guard<std::mutex> lk(cmd_mutex_);
        local.swap(cmd_queue_);
    }
    if (local.empty()) return;

    auto now = std::chrono::steady_clock::now();

    std::vector<std::string> read_batch;
    // P5: fire-and-forget writes drained this cycle are coalesced into a single
    // batched writeValues() request (one round-trip for N writes).
    std::vector<std::string> write_async_paths;
    std::vector<OPCUA::Variant> write_async_values;

    while (!local.empty()) {
        OpcCommand cmd = std::move(local.front());
        local.pop();

        if (cmd.expires <= now) {
            if (cmd.promise)
                cmd.promise->set_value({});
            continue;
        }

        switch (cmd.type) {
        case OpcCmdType::ReadSync:
        case OpcCmdType::ReadAsync: {
            const std::string &path = cmd.paths[0];
            auto it = inflight_reads_.find(path);
            if (it != inflight_reads_.end()) {
                if (cmd.promise)
                    it->second.sync_waiters.push_back(cmd.promise);
                if (cmd.expires > it->second.expires)
                    it->second.expires = cmd.expires;
            } else {
                InflightEntry entry;
                entry.expires = cmd.expires;
                if (cmd.promise)
                    entry.sync_waiters.push_back(cmd.promise);
                inflight_reads_.emplace(path, std::move(entry));
                read_batch.push_back(path);
            }
            break;
        }

        case OpcCmdType::WriteAsync: {
            // P5: fire-and-forget writes carry no status back to GD, so coalesce
            // every WriteAsync drained this cycle into ONE batched writeValues()
            // request (issued after the loop) — one round-trip for N writes instead
            // of N. writeValues() has a result-code bug (returns BADUNEXPECTEDERROR
            // for N>1 even on success), but the writes DO land on the server and
            // async callers never read the status, so the buggy code is discarded.
            for (size_t i = 0; i < cmd.paths.size(); i++) {
                write_async_paths.push_back(cmd.paths[i]);
                write_async_values.push_back(cmd.values[i]);
            }
            break;
        }

        case OpcCmdType::WriteSync: {
            // Sync writes block a GD caller on the result code. Now that the wrapper
            // writeValues() aggregates per-path results correctly (GOOD iff every
            // path is GOOD, else the first bad status — wrapper dev aadc1fb), route
            // an N-path sync write through ONE batched writeValues() call (one
            // round-trip instead of N) and return its aggregate status. Single-path
            // (N==1) is the same one request either way.
            OPCUA::StatusCode sc = client->writeValues(cmd.paths, cmd.values);
            if (cmd.promise) {
                OPCUA::Variant rv;
                rv.setValueFrom<uint32_t>((uint32_t)sc);
                cmd.promise->set_value({rv});
            }
            break;
        }

        case OpcCmdType::InvokeSync: {
            std::vector<OPCUA::Variant> outs;
            client->invokeMethod(cmd.method_path, cmd.inputs, outs, false);
            if (cmd.promise)
                cmd.promise->set_value(outs);
            break;
        }

        case OpcCmdType::InvokeAsync: {
            std::vector<OPCUA::Variant> outs;
            OPCUA::StatusCode sc = client->invokeMethod(cmd.method_path, cmd.inputs, outs, true);
            if (sc == OPCUA::StatusCode::ALL_OK && outs.size() == 1 &&
                    !outs[0].isEmpty() && outs[0].isScalar() &&
                    outs[0].getTypeIndex() == UA_TYPES_UINT32) {
                uint32_t methodID = outs[0].getValueAs<uint32_t>();
                if (ObjectDB::get_instance(cmd.cb_obj_id)) {
                    Array va;
                    va.push_back(cmd.cb_obj_id);
                    va.push_back(cmd.cb_func_name);
                    invokedMethodID_ObjectID_funcName_dict[methodID] = va;
                }
            }
            break;
        }

        case OpcCmdType::WarmCache: {
            // T3: warm the UA cache ON the run thread. We are the only thread that
            // touches UA_Client* here, so no race with run_once() and no need to
            // Stop()/Start(). This blocks the run thread (subscriptions are not
            // serviced) for the browse duration, but never freezes the GD main
            // thread. Completion is signalled on the main thread via call_deferred.
            std::string sProtocol;
            bool ok = client->initClientUaCache(sProtocol, true, cmd.cache_path);
            // We are on the run thread. Deliver the signal via call_deferred so it
            // is emitted on the MAIN thread — this is what makes
            // `await callback_varmanager_cache_ready` safe: the awaiting GD
            // coroutine resumes on the main thread (NOT this run thread), so the
            // follow-up initCacheRuntime()/UI code does not run cross-thread.
            call_deferred("emit_signal", "callback_varmanager_cache_ready", ok);
            break;
        }
        }
    }

    // P5: flush all coalesced async writes in a single batched request BEFORE the
    // async reads, so a read queued behind a write sees the new value. The return
    // code is intentionally ignored (writeValues() misreports N>1, but the writes
    // land on the server and async callers don't consume a status).
    if (!write_async_paths.empty()) {
        client->writeValues(write_async_paths, write_async_values);
    }

    // P3: coalesce the deduped reads into ONE batched sync readValues() (one
    // round-trip for N reads, vs N separate readValue_async dispatches). Results
    // are delivered through variableChanged() — the exact path the async read
    // callback uses — so it resolves the inflight sync_waiters AND notifies bound
    // controls identically. As a side benefit the values are available IN this
    // drain cycle rather than the next one, shaving ~1 poll interval off read
    // latency. readValues() (unlike writeValues) handles N>1 correctly
    // (resultsSize==length) and locks clientMutex internally, so it is safe
    // against run_once(). On any batch failure, fall back to per-path async
    // dispatch so reads still resolve (via callback / expireInflight).
    if (!read_batch.empty()) {
        std::vector<OPCUA::Variant> rvals;
        OPCUA::StatusCode sc = client->readValues(read_batch, rvals);
        if (sc == OPCUA::StatusCode::ALL_OK && rvals.size() == read_batch.size()) {
            for (size_t i = 0; i < read_batch.size(); i++) {
                variableChanged(read_batch[i], rvals[i]);
            }
        } else {
            for (const auto &path : read_batch) {
                client->readValue_async(path);
            }
        }
    }
}

void VarManager::expireInflight() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = inflight_reads_.begin(); it != inflight_reads_.end(); ) {
        if (it->second.expires <= now) {
            for (auto &p : it->second.sync_waiters)
                p->set_value({});
            it = inflight_reads_.erase(it);
        } else {
            ++it;
        }
    }
}

void VarManager::setReadTimeoutMs(int ms)   { read_timeout_ms_   = ms; }
void VarManager::setWriteTimeoutMs(int ms)  { write_timeout_ms_  = ms; }
void VarManager::setInvokeTimeoutMs(int ms) { invoke_timeout_ms_ = ms; }

void VarManager::setRunPollIntervalMs(int ms) {
	// Clamp to [1, 1000]: < 1 ms would make run_once spin a hot busy loop (the
	// wrapper's wakeupTime math truncates sub-ms to 0 -> no throttle); > 1 s is
	// pointless for an interactive HMI.
	if (ms < 1)    ms = 1;
	if (ms > 1000) ms = 1000;
	run_poll_interval_ms_ = ms;
	// minSubscriptionInterval is private in GenericClient and only set at
	// construction, so apply by recreating the client. Only safe while
	// disconnected (the run thread must not be active); a call while connected is
	// ignored for the live session — call this before connect_start. Any other
	// pre-connect client config (e.g. setConnectAuthMode) must be set AFTER this.
	if (client && getStatus() != 0) {
		client->Stop();
		delete client;
		client = new UAClient(std::chrono::milliseconds(run_poll_interval_ms_));
	}
}

bool VarManager::setVarNotifyMode(VarManager::VarNotifyMode p_mode /*= SIGNAL*/)
{
	m_varNotifyMode = p_mode;
	return true;
}

bool VarManager::setTransferProtocol(TransferProtocol p_protocol /*= OPCUA*/) {
	m_transferProtocol = p_protocol;
	return true;
}

void VarManager::setMqttConf(const Dictionary &p_mqttConfDict) {
	m_mqttConfDict = p_mqttConfDict;
}

void VarManager::setAutoRefreshVarKV(const bool &p_switch/* = true*/)
{
	m_autoRefreshVarKVSwitch = p_switch;
}

bool VarManager::initClientCache(const String &p_cachePath/* = ""*/)
{
	// BLOCKING synchronous warm. The recursive browse runs on the CALLING thread,
	// so we must Stop() the background run thread first: UA_Client* is not
	// thread-safe and concurrent access with run_once() corrupts the secure
	// channel and drops the connection. This freezes the caller for the whole
	// browse (~15-20 s on the test server). For a non-blocking variant that warms
	// on the run thread, use initClientCacheAsync().
	//
	// For interface symmetry with initClientCacheAsync(), completion is ALSO
	// reported via callback_varmanager_cache_ready(success), emitted on the main
	// thread (call_deferred fires on the next idle frame, AFTER this call returns).
	// GD code therefore uses the SAME pattern for both variants:
	//     g_Variable.initClientCache(path)        # blocking  (or ...Async: non-blocking)
	//     await g_Variable.callback_varmanager_cache_ready
	//     g_Variable.initCacheRuntime(path)
	// See docs/varmanager-initcache-warming-findings.md
	if (!client)
		return false;
	std::string sProtocol;
	std::string std_cachePath = std::string(p_cachePath.utf8().get_data());
	client->Stop();
	bool bret = client->initClientUaCacheAsync(sProtocol, true, std_cachePath);
	client->Start();
	// Deferred so it fires on the next idle frame (after this call returns), making
	// `await callback_varmanager_cache_ready` work identically to the async path.
	call_deferred("emit_signal", "callback_varmanager_cache_ready", bret);
	return bret;
}

bool VarManager::initClientCacheAsync(const String &p_cachePath /* = ""*/) {
	// Truly asynchronous warm: routes the browse through the command queue so it
	// runs ON the run thread (the only thread that touches UA_Client*) and RETURNS
	// IMMEDIATELY (no main-thread freeze, no run_once race). Completion is reported
	// on the main thread via callback_varmanager_cache_ready(success). GD usage:
	//     g_Variable.initClientCacheAsync(path)   # returns ~0 ms
	//     await g_Variable.callback_varmanager_cache_ready
	//     g_Variable.initCacheRuntime(path)
	return initClientCacheQueued(p_cachePath);
}

bool VarManager::initClientCacheQueued(const String &p_cachePath /* = "" */)
{
	// Implementation behind initClientCacheAsync(): route warming through the
	// command queue so the recursive browse runs ON the run thread (the only
	// thread that touches UA_Client*). This removes the race by construction (no
	// Stop/Start needed) AND does not freeze the GD main thread — this call returns
	// immediately. Completion is reported via the
	// callback_varmanager_cache_ready(success) signal.
	if (!client)
		return false;
	OpcCommand cmd;
	cmd.type       = OpcCmdType::WarmCache;
	cmd.cache_path = std::string(p_cachePath.utf8().get_data());
	// Warming can take many seconds; do not let the drain-loop expiry cancel it.
	cmd.expires    = std::chrono::steady_clock::now() + std::chrono::hours(24);
	{
		std::lock_guard<std::mutex> lk(cmd_mutex_);
		cmd_queue_.push(std::move(cmd));
	}
	return true;
}

bool VarManager::initCacheRuntime(const String &p_cachePath/* = ""*/)
{
	if (client) {
		std::string sProtocol;
		std::string std_cachePath = std::string(p_cachePath.utf8().get_data());
		OPCUA::StatusCode sc = client->initCacheRuntime(std_cachePath);
		return sc == OPCUA::StatusCode::ALL_OK;
	}
	return false;
}

void VarManager::notifyRelatedControls(const String &varName, const Variant &value)
{
	String gdstr = varName;
	// notify related control
	uint32_t var_hash = gdstr.hash();
	if (varhash_ids_dict.has(var_hash)) {
		Vector<Variant> ids = varhash_ids_dict[var_hash];
		int len = ids.size();
		//std::cout << "=====>related var count ids=" << len << std::endl;
		for (int i = 0; i < len; i++) {
			ObjectID id = ObjectID(uint64_t(ids[i]));
			//MessageQueue::get_singleton()->push_call(id, "_on_related_varchange", gdstr, value);
			//instance_from_id(id)->call("_on_varidchange", id, gdstr, vv);
			Object *obj = ObjectDB::get_instance(id);
			//std::cout << "=====>ObjectDB::get_instance(" << id << ")=" << obj << std::endl;
			if (obj) {
				// case 1: signal
				if (m_varNotifyMode == VarNotifyMode::SIGNAL)
				{
					if (!obj->has_signal("_varidchange"))
					{
						if (obj->has_method("_on_varidchange")) {
							obj->add_user_signal(MethodInfo("_varidchange", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::STRING, "varname"), PropertyInfo(Variant::OBJECT, "value")));
							obj->connect("_varidchange", Callable(obj, "_on_varidchange"));
						}
					}
					if (has_signal("_varidchange"))
						obj->emit_signal("_varidchange", (int)(uint64_t)id, gdstr, value);
				}

				// case 2: call_deferred
				else if (m_varNotifyMode == VarNotifyMode::CALL_DEFERRED)
				{

					obj->call_deferred("_on_varidchange", (int)(uint64_t)id, gdstr, value);
				}

				// case 3: MessageQueue push_call
				else if (m_varNotifyMode == VarNotifyMode::MESSAGEQUEUE)
				{
					MessageQueue::get_singleton()->push_call(obj->get_instance_id(), "_on_varidchange", (int)(uint64_t)id, gdstr, value);
				}
			}
		}
	}
	if (has_signal("varChanged"))
		emit_signal("varChanged", gdstr, value);
}

void VarManager::variableChanged(const std::string &varName, const OPCUA::Variant &value) {
    auto it = inflight_reads_.find(varName);
    if (it != inflight_reads_.end()) {
        for (auto &p : it->second.sync_waiters)
            p->set_value({value});
        inflight_reads_.erase(it);
    }
    String gdstr = String(reinterpret_cast<const char *>(varName.data()));
    variableChanged_p(gdstr, value);
}

void VarManager::variableChanged_1(const String &varName, const Variant &value) {
	std::string std_variablePath = std::string(varName.utf8().get_data());
	OPCUA::Variant opcvv;
	convertGDVar2OPCVar(value, opcvv);
	return variableChanged(std_variablePath, opcvv);
}

void VarManager::variablesChanged(const std::vector<std::string> &varNames, const std::vector<OPCUA::Variant> &values) {

	size_t len1 = varNames.size();
	size_t len2 = values.size();

	if (len1 != len2)
	{
		//OS::get_singleton()->print("variablesChanged: varNames.count(%d) != values.count(%d)\n", len1, len2);
		spdlog::get("uacpp_logger")->warn("variablesChanged: varNames.count({0:d}) != values.count({1:d})", len1, len2);
		return;
	}

	Array ns;
	Array vs;

	for (int i = 0; i < len1; i++) {
		std::string varName = varNames[i];
		OPCUA::Variant value = values[i];
		Variant vv;
		String gdstr = String(reinterpret_cast<const char *>(varName.data()));
		convertOPCVar2GDVar(value, vv);
		variableChanged(varName, value);
		ns.push_back(gdstr);
		vs.push_back(vv);
	}
	if (has_signal("varChanged"))
		emit_signal("varsChanged", ns, vs);
}

void VarManager::variablesChanged_1(const Vector<String> &varNames, const Vector<Variant> & values)
{

}

void VarManager::handleEvent(const unsigned int &eventId, const std::map<std::string, OPCUA::Variant> &eventData) {
	//std::cout << "handleEvent eventId: " << eventId << std::endl;
	OS::get_singleton()->print("handleEvent eventId: %d\n", eventId);
	Dictionary eventDataDict;
	///*for (Map<String, Variant>::Element *E = eventDataMap.front(); E; E = E->next()) {
	//	E->key();
	//}*/
	std::map<std::string, OPCUA::Variant>::const_iterator it;
	for (it = eventData.begin(); it != eventData.end(); ++it) {
		Variant vv;
		std::string k = it->first;
		OPCUA::Variant v = it->second;
		//std::cout << "begin convert--------->" << std::endl;
		//std::cout << "k=" << k << " v=" << v << std::endl;
		convertOPCVar2GDVar(v, vv);
		//std::cout << "end convert---------<" << std::endl;
		eventDataDict[String(reinterpret_cast<const char *>(k.data()))] = vv;
		//std::cout << "handleEvent eventData: k=" << k << " v=" << v << " vv=" << std::string(String(vv).utf8().get_data()) << std::endl;
		//OS::get_singleton()->print("handleEvent eventData: k=%s v=%s vv=%s\n", k.c_str(), v.toString().c_str(), String(vv).utf8().get_data());
	}
	//Variant eventDataMapVV = Variant(eventDataDict);
	if (subscribedEventID_ObjectID_funcName_dict.has((uint32_t)eventId))
	{
		Array vArray = subscribedEventID_ObjectID_funcName_dict[(uint32_t)eventId];
		if (vArray.size() == 2)
		{
			ObjectID objID = vArray[0];
			String funcName = vArray[1];
			if (funcName == "")
				funcName = "_on_eventCallback";
			Object *node = ObjectDB::get_instance(objID);
			if (NULL == node) {
				// 对象已经销毁则移除对应的eventId
				subscribedEventID_ObjectID_funcName_dict.erase((uint32_t)eventId);
			}
			else
				MessageQueue::get_singleton()->push_call(node->get_instance_id(), funcName, eventId, eventDataDict);
		}
	}
	if (has_signal("eventTriggered"))
		emit_signal("eventTriggered", (int)eventId, eventDataDict);
}

void VarManager::handleStatusCallback(const int status)
{
	if (status == 0) {
		if (has_signal("callback_varmanager_connect"))
			emit_signal("callback_varmanager_connect");
	} else if (status == 99) {
		if (has_signal("callback_varmanager_disconnect"))
			emit_signal("callback_varmanager_disconnect");
	}
}

void VarManager::handleVariableReadHisCallback(const std::string& nodePath, const std::vector<UA_DataValue>& datas, const bool moreData)
{
	String varName = String(reinterpret_cast<const char *>(nodePath.data()));
	size_t len1 = datas.size();
	Array datas2gd;
	for (int i = 0; i < len1; i++) {
		UA_DataValue value = datas[i];
		Dictionary indic;

		if (value.hasServerTimestamp)
			indic["ServerTime"] = value.serverTimestamp;

		if (value.hasSourceTimestamp)
			indic["SourceTime"] = value.sourceTimestamp;

		if (value.hasStatus)
			indic["Status"] = value.status;

		if (value.value.type == &UA_TYPES[UA_TYPES_UINT32]) {
			UA_UInt32 hrValue = *(UA_UInt32 *)value.value.data;
			indic["Value"] = hrValue;
		}
		else if (value.value.type == &UA_TYPES[UA_TYPES_INT32]) {
			UA_Int32 hrValue = *(UA_Int32 *)value.value.data;
			indic["Value"] = hrValue;
		}
		else if (value.value.type == &UA_TYPES[UA_TYPES_DOUBLE]) {
			UA_Double hrValue = *(UA_Double *)value.value.data;
			indic["Value"] = hrValue;
		}

		//String in = JSON::print(indic);
		datas2gd.push_back(indic);
	}
	if (has_signal("varReadHisCallback"))
		emit_signal("varReadHisCallback", varName, datas2gd, moreData);
}

void VarManager::handleInvokeMethodCallback(const unsigned int &reqId, const std::vector<OPCUA::Variant> &outputs)
{
	size_t len = outputs.size();
	Array outputsArray;
	for (int i = 0; i < len; i++) {
		OPCUA::Variant value = outputs[i];
		Variant vv;
		convertOPCVar2GDVar(value, vv);
		outputsArray.push_back(vv);
	}
	if (invokedMethodID_ObjectID_funcName_dict.has((uint32_t)reqId))
	{
		Array vArray= invokedMethodID_ObjectID_funcName_dict[(uint32_t)reqId];
		if (vArray.size() == 2)
		{
			ObjectID objID = vArray[0];
			String funcName = vArray[1];
			if (funcName == "")
				funcName = "_on_methodCallback";
			Object *node = ObjectDB::get_instance(objID);
			if (NULL == node) {
				// 对象已经销毁则移除对应的eventId
				invokedMethodID_ObjectID_funcName_dict.erase((uint32_t)reqId);
			}
			else
				MessageQueue::get_singleton()->push_call(node->get_instance_id(), funcName, reqId, outputsArray);
		}
		
		invokedMethodID_ObjectID_funcName_dict.erase((uint32_t)reqId);
	}

	if(has_signal("methodInvoked"))
		emit_signal("methodInvoked", (int)reqId, outputsArray);
}


Error VarManager::init() {

	// Customize msg format for all loggers
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

	// periodically flush all *registered* loggers every 3 seconds:
	// warning: only use if all your loggers are thread safe ("_mt" loggers)
	spdlog::flush_every(std::chrono::seconds(3));

	spdlog::info("Welcome to ua cpp varmanager wrapper!");

	if (!spdlog::thread_pool())
		spdlog::init_thread_pool(8192, 1);

	if (!spdlog::get("uacpp_logger")) {
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		//console_sink->set_level(spdlog::level::warn);
		console_sink->set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

		// Create a file rotating logger with 5mb size max and 3 rotated files
		auto max_size = 1048576 * 5;
		auto max_files = 3;
		//auto file_sink = spdlog::rotating_logger_mt<spdlog::async_factory>("uacpp_logger", "logs/uacpp.log", max_size, max_files);
		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/uacpp_varmanager.log", max_size, max_files);
		file_sink->set_level(spdlog::level::warn);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

		std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
		auto logger = std::make_shared<spdlog::async_logger>("uacpp_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		//auto logger = std::make_shared<spdlog::logger>("uacpp_logger", sinks.begin(), sinks.end());
		spdlog::register_logger(logger);
	}

	// Log levels can be loaded from argv/env using "SPDLOG_LEVEL"
	load_levels_example();
	auto logger = spdlog::get("uacpp_logger");
	printf("uacpp_logger level=%d\n", logger->level());
	logger->info("uacpp_logger init() succeed");

	return Error::OK;
}

VarManager::OpcTestConfig VarManager::loadTestConfig() {
	OpcTestConfig cfg;
	// Look for config file next to the executable, then fall back to user://
	String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	String cfg_path = exe_dir.path_join("../modules/varmanager/test/varmanager_test.cfg");

	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(cfg_path) != OK) {
		cf->load("user://varmanager_test.cfg");
	}

	if (cf->has_section_key("server", "url"))
		cfg.server_url = cf->get_value("server", "url");
	if (cf->has_section_key("server", "object_path"))
		cfg.object_path = cf->get_value("server", "object_path");
	if (cf->has_section_key("test", "var_path"))
		cfg.test_var_path = cf->get_value("test", "var_path");
	if (cf->has_section_key("test", "batch_var_paths")) {
		String s = cf->get_value("test", "batch_var_paths");
		if (!s.is_empty())
			cfg.batch_var_paths = s.split(",", false);
	}
	if (cf->has_section_key("test", "concurrency"))
		cfg.concurrency = (int)cf->get_value("test", "concurrency");
	if (cf->has_section_key("test", "iterations"))
		cfg.iterations = (int)cf->get_value("test", "iterations");
	if (cf->has_section_key("test", "timeout_ms"))
		cfg.timeout_ms = (int)cf->get_value("test", "timeout_ms");
	return cfg;
}

void VarManager::finish() {
	connect_close();
}

void VarManager::register_vars(String p_var, ObjectID id, bool cache /*=false*/) {

	// varhash_ids_dict
	uint32_t var_hash = p_var.hash();
	if (varhash_ids_dict.has(var_hash)) {
		Vector<Variant> ids = varhash_ids_dict[var_hash];
		if (ids.find(id) < 0) {
			ids.push_back(id);
			varhash_ids_dict[var_hash] = ids;
		}
	} else {
		Vector<Variant> ids;
		ids.push_back(id);
		varhash_ids_dict[var_hash] = ids;
	}

	// varpath_ids_dict
	if (varpath_ids_dict.has(p_var)) {
		Vector<Variant> ids = varpath_ids_dict[p_var];
		if (ids.find(id) < 0) {
			ids.push_back(id);
			varpath_ids_dict[p_var] = ids;
			//Variant v = readValue(p_var);
			//notifyRelatedControls(p_var, v);
		}
	} else {
		Vector<Variant> ids;
		ids.push_back(id);
		varpath_ids_dict[p_var] = ids;
		if (!cache)
			VarManager::UAStatusCode sc = subscribe(p_var);
	}

	// varpath_subed_dict
	if (!varpath_subed_dict.has(p_var)) {
		varpath_subed_dict[p_var] = 0;
	}

	//std::cout << "register_vars: " << std::string(p_var.utf8().get_data()) << " ObjectID:" << id << std::endl;
	OS::get_singleton()->print("register_vars: %s ObjectID:%d\n", p_var.utf8().get_data(), id);
}

void VarManager::unregister_vars(ObjectID id, bool cache /*=false*/) {

	// varhash_ids_dict
	Array idkeys = varhash_ids_dict.keys();
	if (idkeys.size()) {
		for (int i = 0; i < idkeys.size(); i++) {
			Variant p_varid = idkeys[i];
			Vector<Variant> ids = varhash_ids_dict[p_varid];
			if (ids.find(id) >= 0) {
				// finded
				ids.erase(id);
				varhash_ids_dict[p_varid] = ids;
			}
		}
	}

	// varpath_ids_dict
	Array pathkeys = varpath_ids_dict.keys();
	if (pathkeys.size()) {
		for (int i = 0; i < pathkeys.size(); i++) {
			Variant p_varpath = pathkeys[i];
			Vector<Variant> ids = varpath_ids_dict[p_varpath];
			if (ids.find(id) >= 0) {
				// finded
				ids.erase(id);
				varpath_ids_dict[p_varpath] = ids;
				if (!cache) {
					if (ids.size())
						VarManager::UAStatusCode sc = unSubscribe(p_varpath);
				}
			}

			// varpath_subed_dict
			if (varpath_subed_dict.has(p_varpath))
				varpath_subed_dict.erase(p_varpath);
		}
	}

}
#ifdef  _WIN32
char *VarManager::wc_to_utf8(const wchar_t *wc) {
	int ulen = WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
	char *ubuf = new char[ulen + 1];
	WideCharToMultiByte(CP_UTF8, 0, wc, -1, ubuf, ulen, nullptr, nullptr);
	ubuf[ulen] = 0;
	return ubuf;
}

inline std::wstring VarManager::utf8_to_wide(const std::string &utf8Text) {
	if (utf8Text.empty()) {
		return {};
	}

	std::wstring wideText;
	const int wideLength = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), nullptr, 0);
	if (wideLength == 0) {
		spdlog::get("uacpp_logger")->error("utf8_to_wide get size error: " + std::to_string(::GetLastError()));
		return {};
	}

	// MultiByteToWideChar returns number of chars of the input buffer, regardless of null terminator
	wideText.resize(wideLength, 0);
	wchar_t *wideString = const_cast<wchar_t *>(wideText.data()); // mutable data() only exists in c++17
	const int length = ::MultiByteToWideChar(CP_UTF8, 0, utf8Text.data(), (int)utf8Text.size(), wideString, wideLength);
	if (length != wideLength) {
		spdlog::get("uacpp_logger")->error("utf8_to_wide convert string error: " + std::to_string(::GetLastError()));
		return {};
	}

	return wideText;
}

inline std::string VarManager::wide_to_utf8(const std::wstring &wideText) {
	if (wideText.empty()) {
		return {};
	}

	std::string narrowText;
	int narrowLength = ::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), nullptr, 0, nullptr, nullptr);
	if (narrowLength == 0) {
		spdlog::get("uacpp_logger")->error("wide_to_utf8 get size error: " + std::to_string(::GetLastError()));
		return {};
	}

	// WideCharToMultiByte returns number of chars of the input buffer, regardless of null terminator
	narrowText.resize(narrowLength, 0);
	char *narrowString = const_cast<char *>(narrowText.data()); // mutable data() only exists in c++17
	const int length =
			::WideCharToMultiByte(CP_UTF8, 0, wideText.data(), (int)wideText.size(), narrowString, narrowLength, nullptr, nullptr);
	if (length != narrowLength) {
		spdlog::get("uacpp_logger")->error("wide_to_utf8 convert string error: " + std::to_string(::GetLastError()));
		return {};
	}

	return narrowText;
}
#endif
