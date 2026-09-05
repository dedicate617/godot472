#include "mqttmanager.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "core/io/json.h"
#include "core/io/file_access.h"
#include "core/io/dir_access.h"
#include "core/crypto/hashing_context.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"
//#include "modules/regex/regex.h"
#include "util.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <codecvt>
#include <time.h>

#include "mqtt/async_client.h"
#include "mqtt/client.h"
#include "mqtt/create_options.h"
#include "mqtt/iclient_persistence.h"

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#define SPDLOG_NO_EXCEPTIONS
#include "spdlog/async.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/fmt/bundled/ranges.h"
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
 

////std::wstring MqttManager::getAppDataRoaming(const wstring &company, const wstring &appName) {
////#ifdef __linux__ || __unix__
////	
////#elif _WIN32
////	
////	wchar_t roaming[MAX_PATH] = { 0 };
////	auto size = GetEnvironmentVariable(L"appdata", roaming, MAX_PATH);
////	if (size >= MAX_PATH || size == 0) {
////		cout << "fail to get %appdata%: " << GetLastError() << endl;
////		return L"";
////	}
////	else {
////		wstring result(roaming, size);
////		result += L"\\" + company;
////		result += L"\\" + appName;
////		return result;
////	}
////#endif
////	return L"";
////}


MqttManager *MqttManager::singleton = NULL;

MqttManager *MqttManager::get_singleton() {
	return singleton;
}

void MqttManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("MqttConnected"));
	ADD_SIGNAL(MethodInfo("MqttLostconnect"));
	ADD_SIGNAL(MethodInfo("MqttDisconnected"));
	ADD_SIGNAL(MethodInfo("MqttMsgReceived", PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::STRING, "msg"), PropertyInfo(Variant::INT, "qos")));
	ADD_SIGNAL(MethodInfo("MqttMsgReceived1", PropertyInfo(Variant::STRING, "topic"), PropertyInfo(Variant::STRING, "msg"), PropertyInfo(Variant::INT, "qos")));
	ADD_SIGNAL(MethodInfo("MqttProcessRequest", PropertyInfo(Variant::STRING, "req_topic"), PropertyInfo(Variant::STRING, "req_msg"), PropertyInfo(Variant::STRING, "rsp_topic")));

	ClassDB::bind_method(D_METHOD("init", "prjPath"), &MqttManager::init, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("init_bydict", "mqttConfDict"), &MqttManager::init_bydict);
	ClassDB::bind_method(D_METHOD("connect_start", "host", "port", "clientId"), &MqttManager::connect_start, DEFVAL(""), DEFVAL(1883), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("subscribe", "topic", "qos"), &MqttManager::subscribe, DEFVAL(MqttManager::QOS::QOS0));
	ClassDB::bind_method(D_METHOD("unsubscribe", "topic"), &MqttManager::unsubscribe);
	ClassDB::bind_method(D_METHOD("publish", "topic", "msg", "qos"), &MqttManager::publish, DEFVAL(MqttManager::QOS::QOS0));
	ClassDB::bind_method(D_METHOD("req_rsp", "reqtopic", "reqmsg", "rsptopic", "remoteClientId", "qos", "secs_timeout", "auto_mode"), &MqttManager::req_rsp, DEFVAL(MqttManager::QOS::QOS0), DEFVAL(5), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("connect_close"), &MqttManager::connect_close);

	BIND_ENUM_CONSTANT(QOS0);
	BIND_ENUM_CONSTANT(QOS1);
	BIND_ENUM_CONSTANT(QOS2);
}

void MqttManager::connect_signals() {
	Object::connect("MqttConnected", callable_mp(this, &MqttManager::onMqttConnected));
	Object::connect("MqttLostconnect", callable_mp(this, &MqttManager::onMqttLostconnect));
	Object::connect("MqttDisconnected", callable_mp(this, &MqttManager::onMqttDisconnected));
	Object::connect("MqttMsgReceived", callable_mp(this, &MqttManager::onMqttMsgReceived));
}

MqttManager::MqttManager() {
	m_mqttClient = nullptr;
	m_mqttReqRspClient = nullptr;
	m_prjPath = "";
	m_connSts = -1;
	singleton = this;
}

MqttManager::~MqttManager() {
	//singleton = NULL;
}

void MqttManager::onMqttConnected() {
	if (m_connSts == 0) {
		// todo: auto resub the topics has been subscribed when reconnected
		//m_mqttClient->subscribe(TOPICS, VEC_QOS);
		autoReSubscribe();
		//std_subscribedTopics
		spdlog::get("mqtt_logger")->info("mqtt Reconnected and resub succeed!");

	} else if (m_connSts == -1)
		;
	m_connSts = 1;
	spdlog::get("mqtt_logger")->info("MqttManager connected");
}

void MqttManager::onMqttLostconnect() {
	m_connSts = 0;
	spdlog::get("mqtt_logger")->warn("MqttManager lost connect");
}

void MqttManager::onMqttDisconnected() {
	m_connSts = 0;
	spdlog::get("mqtt_logger")->warn("MqttManager Disconnected");
}

void MqttManager::onMqttMsgReceived(const String &topic, const String &msg, const int qos) {
	spdlog::get("mqtt_logger")->info("MqttManager message received: topic: {}, msg: {}, qos: {}", string(topic.utf8().get_data()), string(msg.utf8().get_data()), (int)qos);
	if (has_signal("MqttMsgReceived1"))
		emit_signal("MqttMsgReceived1", topic, msg, qos);
}

void MqttManager::_pre_msg_received(String topic, mqtt::const_message_ptr msg, const int qos) {
	spdlog::get("mqtt_logger")->info("MqttManager _pre_msg_received: topic: {}, msg: {}, qos: {}", string(topic.utf8().get_data()), string(msg->get_payload_str()), (int)qos);
	if (topic.begins_with("$file/")) {
		Vector<String> arr = topic.split("/", false);
		if (arr.size() == 4 && arr[2]=="init") {
			// msg init
			// $file/31c97becadc058e791c5d6a0d817ecb8/init/[remoteclientid]
			//{
			//    "expire_at": 524288,
			//    "name": "C:\\prj\\gd4\\server\\KV_bak\\__var__",
			//    "segments_ttl": "3",
			//    "size": 524288
			//}
			//
			// Windows: C:\Users\Administrator\AppData\Local
			// macOS: ~/Library/Caches/Godot/
			// Linux : ~/.cache/
			String m_mqRefreshKVPath = OS::get_singleton()->get_cache_path();
			spdlog::get("mqtt_logger")->info("cache path: {}", string(m_mqRefreshKVPath.utf8().get_data()));
			String msgstr = String(msg->get_payload_str().c_str());
			Dictionary msg_dict = JSON::parse_string(msgstr);
			String received_filename = msg_dict["name"];
			if (received_filename.ends_with("__var__"))
				received_filename = "__varmq__";
			else if (received_filename.ends_with("__var__.crc"))
				received_filename = "__varmq__.crc";
			else
			{
#if defined(_WIN32)
				received_filename = received_filename.replace("/", "\\");
				PackedStringArray arr = received_filename.split("\\");
				if (arr.size() > 0)
					received_filename = arr[arr.size() - 1];
#else
				received_filename = received_filename.replace("\\", "/");
				PackedStringArray arr = received_filename.split("/");
				if (arr.size() > 0)
					received_filename = arr[arr.size()-1];
#endif
			}
			
#if defined(_WIN32)
			m_mqRefreshKVPath = m_mqRefreshKVPath.replace("/", "\\") + "\\" + received_filename;
#else
			m_mqRefreshKVPath = m_mqRefreshKVPath.replace("\\", "/") + "/" + received_filename;
#endif
			spdlog::get("mqtt_logger")->info("receive file path: {}", string(m_mqRefreshKVPath.utf8().get_data()));
			// 1.存在则删除
			if (FileAccess::exists(m_mqRefreshKVPath))
				DirAccess::remove_absolute(m_mqRefreshKVPath);
			// 2.创建size大小文件
			std::ofstream file(std::string(m_mqRefreshKVPath.utf8().get_data()));
			size_t size = (size_t)(int)msg_dict["size"];
			std::string s(size, '0');
			file << s;
			file.close();

			// Read-write memory map the whole file by using `map_entire_file` where the
			// length of the mapping is otherwise expected, with the factory method.
			std::error_code error;
			m_rw_mmap = mio::make_mmap_sink(
					std::string(m_mqRefreshKVPath.utf8().get_data()), 0, mio::map_entire_file, error);
			if (error) {
				handle_mio_error(error);
				return;
			}

			m_last_mqRefreshKVPath = m_mqRefreshKVPath;

		} else if (arr.size() == 4 && arr[2] != "init") {
			// msg body
			// $file/4280e956ea78b25a2222ccdc001f7c4d/516096/[remoteclientid]
			int offset = arr[2].to_int();
			//const char *payload = (const char *)msg->get_payload_ref().data();
			//size_t payload_length = msg->get_payload_ref().length();
			string payload_str = msg->get_payload_str();
			const char *payload = payload_str.c_str();
			size_t payload_length = payload_str.length();

			char *mem_offset = m_rw_mmap.begin();
			mem_offset += offset;
			std::memcpy(mem_offset, payload, payload_length);

			// Don't forget to flush changes to disk before unmapping. However, if
			// `rw_mmap` were to go out of scope at this point, the destructor would also
			// automatically invoke `sync` before `unmap`.
			std::error_code error;
			m_rw_mmap.sync(error);
			if (error) {
				handle_mio_error(error);
				return;
			}

		} else if (arr.size() == 5 && arr[2]=="fin") {
			// msg fin
			// $file/4280e956ea78b25a2222ccdc001f7c4d/fin/524288/[remoteclientid]

			// Don't forget to flush changes to disk before unmapping. However, if
			// `rw_mmap` were to go out of scope at this point, the destructor would also
			// automatically invoke `sync` before `unmap`.
			std::error_code error;
			m_rw_mmap.sync(error);
			if (error) {
				handle_mio_error(error);
				return;
			}

			// We can then remove the mapping, after which rw_mmap will be in a default
			// constructed state, i.e. this and the above call to `sync` have the same
			// effect as if the destructor had been invoked.
			m_rw_mmap.unmap();

			// todo: 最终存盘成功后检测md5是否一致表示传输成功
			String filemd5 = getFileMD5(m_last_mqRefreshKVPath);
			bool transfer_result = (filemd5 == arr[1]);
			spdlog::get("mqtt_logger")->info("MqttManager _pre_msg_received: file: {}, transfer result: {}", string(m_last_mqRefreshKVPath.utf8().get_data()), transfer_result);
		}
	}
}

String MqttManager::getFileMD5(const String &filePath) {
	// Check that file exists.
	if (!FileAccess::exists(filePath))
		return "";
	return FileAccess::get_md5(filePath);
	//// Start an SHA - 256 context.
	//HashingContext h_ctx;
	//Error err = h_ctx.start(HashingContext::HASH_MD5);
	//// Open the file to hash.
	//Ref<FileAccess> file = FileAccess::open(filePath, FileAccess::READ);
	//// Update the context after reading each chunk.
	//while (file.ptr()->get_position() < file.ptr()->get_length()) {
	//	uint64_t remaining = file.ptr()->get_length() - file.ptr()->get_position();
	//	h_ctx.update(file.ptr()->get_buffer(MIN(remaining, 1024 * 8)));
	//}
	//// Get the computed hash.
	//const uint8_t* res = h_ctx.finish().ptr();
	//return String::hex_encode_buffer(res, sizeof(res));
	//// Print the result as hex string and array.
	//printt(res.hex_encode(), Array(res))
	//const auto &errmsg = error.message();
	//std::printf("error mapping file: %s, exiting...\n", errmsg.c_str());
	//spdlog::get("mqtt_logger")->error("MqttManager handle_mio_error: error mapping file: {}, exiting...", string(errmsg.c_str()));

}


int MqttManager::handle_mio_error(const std::error_code &error) {
	const auto &errmsg = error.message();
	std::printf("error mapping file: %s, exiting...\n", errmsg.c_str());
	spdlog::get("mqtt_logger")->error("MqttManager handle_mio_error: error mapping file: {}, exiting...", string(errmsg.c_str()));
	return error.value();
}

void MqttManager::initSpdlog() {

	// Customize msg format for all loggers
	spdlog::set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

	// periodically flush all *registered* loggers every 3 seconds:
	// warning: only use if all your loggers are thread safe ("_mt" loggers)
	spdlog::flush_every(std::chrono::seconds(3));

	spdlog::info("Welcome to mqtt cpp wrapper!");

	if (!spdlog::thread_pool())
		spdlog::init_thread_pool(8192, 1);

	if (!spdlog::get("mqtt_logger")) {
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		//console_sink->set_level(spdlog::level::warn);
		console_sink->set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

		// Create a file rotating logger with 5mb size max and 3 rotated files
		auto max_size = 1048576 * 5;
		auto max_files = 3;
		//auto file_sink = spdlog::rotating_logger_mt<spdlog::async_factory>("uacpp_logger", "logs/uacpp.log", max_size, max_files);
		auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>("logs/mqtt.log", max_size, max_files);
		file_sink->set_level(spdlog::level::warn);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S %z] [%^%L%$] [thread %t] %v");

		std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
		auto logger = std::make_shared<spdlog::async_logger>("mqtt_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		//auto logger = std::make_shared<spdlog::logger>("uacpp_logger", sinks.begin(), sinks.end());

		spdlog::register_logger(logger);
	}

	// Log levels can be loaded from argv/env using "SPDLOG_LEVEL"
	load_levels_example();
	auto logger = spdlog::get("mqtt_logger");
	printf("mqtt_logger level=%d\n", logger->level());
	logger->info("mqtt_logger init() succeed!");
}


Error MqttManager::init(const String &p_prjPath /* = ""*/) {

	initSpdlog();

	m_prjPath = std::string(p_prjPath.utf8().get_data());
	Error err;
	Ref<FileAccess> f_lf = FileAccess::open(p_prjPath, FileAccess::READ, &err);
	
	if (!f_lf.is_null()) {
		String json_string = f_lf->get_as_utf8_string();
		Dictionary dict = JSON::parse_string(json_string);
		m_mqttConfDict = (Dictionary)(dict["@Mqtt"]);
		return Error::OK;
	}

	return err;
}

Error MqttManager::init_bydict(const Dictionary &p_mqttConfDict) {

	initSpdlog();

	m_mqttConfDict = p_mqttConfDict;

	return Error::OK;
}

void /*MqttManager::*/ startConsumeMsg(MqttManager *pman) {
	if (!pman)
		return;

	MqttClient *cli = pman->getMqttClient();
	if (!cli)
		return;
	if (!cli->isConnectedToHost())
		return;

	spdlog::get("mqtt_logger")->info("Waiting for RPC requests...");
	while (true) {

		// case 1: just monitor connect status and do reconnect
		//if (!cli->is_connected()) {
		//	pman->setConnSts(0);
		//	spdlog::get("mqtt_logger")->warn("Lost connection. Attempting reconnect");
		//		if (pman->try_reconnect(cli)) {
		//			//cli->subscribe(TOPICS, VEC_QOS);
		//			spdlog::get("mqtt_logger")->warn("Reconnected");
		//			continue;
		//		} else {
		//			spdlog::get("mqtt_logger")->warn("Reconnect failed, try to reconnect again...");
		//			continue;
		//		}
		//} else {
		//	//                cli->subscribe(TOPICS, VEC_QOS);
		//	//                qDebug() << "Reconnected";
		//	this_thread::sleep_for(seconds(2));
		//	continue;
		//}


		// case 2: consume msg and check connect status and reconnect
		//auto msg = m_mqttClient->consume_message();
		auto msg = cli->try_consume_message_for(seconds(3));

		if (!msg) {
			if (!cli->is_connected()) {
				pman->setConnSts(0);
				spdlog::get("mqtt_logger")->warn("Lost connection. Attempting reconnect");
				if (pman->try_reconnect(cli)) {
					spdlog::get("mqtt_logger")->warn("Reconnected");
					// do resub topics when reconnect succeed!
					// cli->subscribe(TOPICS, VEC_QOS);
					pman->autoReSubscribe();
					spdlog::get("mqtt_logger")->warn("resubed topics");
					continue;
				} else {
					spdlog::get("mqtt_logger")->warn("Reconnect failed, try to reconnect again...");
					continue;
				}
			} else {
				//                cli->subscribe(TOPICS, VEC_QOS);
				//                qDebug() << "Reconnected";
				continue;
			}
		}

		spdlog::get("mqtt_logger")->info("Received a msg");

		const mqtt::properties &props = msg->get_properties();
		std::string req_topic = msg->get_topic();
		std::string req_msg = msg->to_string();
		spdlog::get("mqtt_logger")->info("{}: {}", req_topic, req_msg);

		// 通知外部用户回调事件处理订阅接收到的消息
		if (pman->has_signal("MqttProcessRequest"))
			pman->emit_signal("MqttProcessRequest", pman->getUtf8String(req_topic), pman->getUtf8String(req_msg), "");


		//if (props.contains(RESPONSE_TOPIC) && props.contains(CORRELATION_DATA)) {
		//	mqtt::binary corr_id = mqtt::get<string>(props, CORRELATION_DATA);
		//	std::string corr_id_str = corr_id;
		//	std::string rsp_topic = mqtt::get<string>(props, RESPONSE_TOPIC);
		//	spdlog::get("mqtt_logger")->info("Client wants a response to [{}] on '{}'", corr_id_str, rsp_topic);

		//	// 如果需要在CloudClient端提前处理则在此处判断req_topic并处理rsp
		//	//            QString result = "error";
		//	//            if(req_topic == "req/xxx/yyy")
		//	//            {
		//	//                // response
		//	//                emit publish2Mqtt(reply_to, result);
		//	//            }
		//	//            else
		//	//            {
		//	//                qDebug() << "Unknown request topic: " << req_topic;
		//	//            }

		//	// 否则交由 gd script callback 通知 MqttProcessRequest 统一处理req/rsp模式的请求回馈
		//	if (pman->has_signal("MqttProcessRequest"))
		//		pman->emit_signal("MqttProcessRequest", pman->getUtf8String(req_topic), pman->getUtf8String(req_msg), pman->getUtf8String(rsp_topic));

		//	//            auto reply_msg = mqtt::message::create(reply_to, to_string(x), 1, false);
		//	//            m_mqttClient->publish(reply_msg);
		//}
	}
}

bool MqttManager::connect_start(const String &p_host /* = ""*/, const int p_port /* = 1883*/, const String &p_clientId /* = ""*/) {
	String ServerIP = (String)m_mqttConfDict["@ServerIP"];
	std::string host = std::string(ServerIP.utf8().get_data());
	if (p_host != "")
		host = std::string(p_host.utf8().get_data());
	uint16_t port = (uint16_t)((int)m_mqttConfDict["@ServerPort"]);
	if (p_port != 1883)
		port = (uint16_t)p_port;
	std::string clientId = ((String)m_mqttConfDict["@ClientID"]).utf8().get_data();
	if (p_clientId != "")
		clientId = std::string(p_clientId.utf8().get_data());

	// mqtt broker
	mqtt::create_options createOpts = mqtt::create_options_builder()
							  .mqtt_version(MQTTVERSION_5)
							  .send_while_disconnected(true, true)
							  .max_buffered_messages(MAX_BUFFERED_MESSAGES)
							  .delete_oldest_messages(true)
							  .restore_messages(true)
							  .persist_qos0(true)
							  .finalize();
#ifdef _WIN32
	std::string serverURI = str_format("mqtt://%s:%d", host, port);
#elif defined(__linux__)
	std::string serverURI = util::Format("mqtt://{0}:{1}", host, port);
#endif

	spdlog::get("mqtt_logger")->info("connect to {}", serverURI);

#if defined(_WIN32)
	m_mqttClient = new MqttClient(createOpts, host, port, clientId);
#else
	m_mqttClient = new MqttClient(createOpts, host, port, clientId);
	//encoded_file_persistence persist("Gdmorph");
	//m_mqttClient = new MqttClient(createOpts, host, port, clientId, &persist);
#endif
	// Set callbacks for when connected and connection lost.

	m_mqttClient->set_connected_handler([this](const std::string &) {
		spdlog::get("mqtt_logger")->info("*** Connected ({0:d}) ***", timestamp());
		if (has_signal("MqttConnected"))
			emit_signal("MqttConnected");
	});

	m_mqttClient->set_connection_lost_handler([this](const std::string &) {
		spdlog::get("mqtt_logger")->info("*** Connected Lost ({0:d}) ***", timestamp());
		if (has_signal("MqttLostconnect"))
			emit_signal("MqttLostconnect");
	});

	m_mqttClient->set_disconnected_handler([this](const mqtt::properties &, mqtt::ReasonCode reason) {
		spdlog::get("mqtt_logger")->info("*** Disconnected. Reason: {0:d} ***", reason);
		if (has_signal("MqttDisconnected"))
			emit_signal("MqttDisconnected");
	});

	m_mqttClient->set_message_callback([this](mqtt::const_message_ptr msg) {
		spdlog::get("mqtt_logger")->debug("receive: {} ***", msg->get_payload_str());
		String topic = String(msg->get_topic().c_str());
		if (topic.begins_with("$file/")) {
			//this->call_deferred("_pre_msg_received", topic, msg, msg->get_qos());
			_pre_msg_received(topic, msg, msg->get_qos());
		}

		else if (has_signal("MqttMsgReceived"))
			emit_signal("MqttMsgReceived", topic, String(msg->get_payload_str().c_str()), msg->get_qos());
	});

	m_mqttClient->set_update_connection_handler([](mqtt::connect_data &connData) {
		std::string newUserName{ "newuser" };
		if (connData.get_user_name() == newUserName)
			return false;

		spdlog::get("mqtt_logger")->debug("Previous user: '{}'", connData.get_user_name());
		connData.set_user_name(newUserName);
		spdlog::get("mqtt_logger")->debug("New user name: '{}'", connData.get_user_name());
		return true;
	});

	//(int)Dictionary(m_mqttConfDict["apples"])["blue"]
	if ((String)m_mqttConfDict["@User"] != "") {
#define CONN_MQTT_BROKER_WITH_USR_PSW
		;
	} else {
#undef CONN_MQTT_BROKER_WITH_USR_PSW
		;
	}
#if defined(_WIN32)
	size_t pos = 0;
	while ((pos = m_prjPath.find("/", pos)) != std::string::npos) {
		m_prjPath.replace(pos, std::string("/").length(), "\\");
		pos += std::string("\\").length(); // Move past the replaced substring
	}
	std::string confPath = m_prjPath + "\\" + "conf" + "\\";
#else
	size_t pos = 0;
	while ((pos = m_prjPath.find("\\", pos)) != std::string::npos) {
		m_prjPath.replace(pos, std::string("\\").length(), "/");
		pos += std::string("/").length(); // Move past the replaced substring
	}
	std::string confPath = m_prjPath + "/" + "conf" + "/";
#endif
	std::string trustFilePath;
	std::string keyFilePath;
	if (m_mqttConfDict["@KeyName"] != "" && m_mqttConfDict["@TrustName"] != "") {
		// Note that we don't actually need to open the trust or key stores.
		// We just need a quick, portable way to check that they exist.
		{
			trustFilePath = confPath + std::string(((String)m_mqttConfDict["@TrustName"]).utf8().get_data());
			std::ifstream tstore(trustFilePath);
			if (!tstore) {
				spdlog::get("mqtt_logger")->warn("The trust store file does not exist: {}", trustFilePath);
				spdlog::get("mqtt_logger")->warn("  Get a copy from \"[prjPath]/conf/mqtt_root_ca.crt\"");
				return false;
			}

			keyFilePath = confPath + std::string(((String)m_mqttConfDict["@KeyName"]).utf8().get_data());
			std::ifstream kstore(keyFilePath);
			if (!kstore) {
				spdlog::get("mqtt_logger")->warn("The key store file does not exist: {}", keyFilePath);
				spdlog::get("mqtt_logger")->warn("  Get a copy from \"[prjPath]/conf/mqtt_client.pem\"");
				return false;
			}
		}
#define CONN_MQTT_BROKER_WITH_SSL
	} else {
#undef CONN_MQTT_BROKER_WITH_SSL
		;
	}

#ifdef CONN_MQTT_BROKER_WITH_SSL
	auto sslopts = mqtt::ssl_options_builder()
					//        .ssl_version(MQTT_SSL_VERSION_DEFAULT)
					//        .enabled_cipher_suites(true)
					//        .verify(true)
					.trust_store(trustFilePath.toStdString())
					.key_store(keyFilePath.toStdString())
					.error_handler([](const std::string &msg) {
						spdlog::get("mqtt_logger")->info("SSL Error: {}",msg);
					})
					//        .private_key(const string& key)
					//        .private_keypassword(const string& passwd)
					//        .ca_path(const string& path)

					//        .psk_handler()([](const std::string& msg,
					//                       char *identity, size_t max_identity_len,
					//                       unsigned char *psk, size_t max_psk_len){
					//            spdlog::get("mqtt_logger")->info("psk Error: {}, identity: {}, max_identity_len: {}, psk: {}, max_psk_len: {}"
					//                      , msg
					//						, std::string(identity)
					//						, (int)max_identity_len
					//						, std::string(psk)
					//						, (int)max_psk_len
					//				);
					//        })
					//        .alpn_protos(const std::vector<string>& protos)
					.finalize();
#endif

	auto willMsg = mqtt::message("scada/prjname/scadawill", "goodbye scada world", (int)QOS1, true);
	auto connOpts = mqtt::connect_options_builder()
							.mqtt_version(MQTTVERSION_5)
							.clean_start(true) //(MQTT v5 only)
							//            .clean_session(true) //(MQTT v3.x only)
							.connect_timeout(seconds(30))
							.keep_alive_interval(seconds(20))
							.automatic_reconnect(true)
							.automatic_reconnect(seconds(2), seconds(30))
							.will(std::move(willMsg))
							.properties({ { mqtt::property::SESSION_EXPIRY_INTERVAL, INFINITE /*604800*/ } })
#ifdef CONN_MQTT_BROKER_WITH_USR_PSW
							.user_name(clientPara->m_mqttUser.toLocal8Bit().constData());
	.password(clientPara->m_mqttPsw.toLocal8Bit().constData());
#endif
#ifdef CONN_MQTT_BROKER_WITH_SSL
	.ssl(std::move(sslopts))
#endif
			//            .max_inflight(100)
			//            .http_headers()
			//            .http_proxy()
			.finalize();

	connect_signals();

	//mqttConnectCallback ccb(*m_mqttClient, connOpts);
	//m_mqttClient->set_callback(ccb);

	try {
		// Start consumer before connecting to make sure to not miss messages
		m_mqttClient->start_consuming();

		spdlog::get("mqtt_logger")->info("Connecting to the MQTT server...");
		// case 1: connect use callback manually
		//m_mqttClient->connect(connOpts, nullptr, ccb);

		// case 2: connect use paho backend itself
		mqtt::token_ptr conntok = m_mqttClient->connect(connOpts);

		spdlog::get("mqtt_logger")->info("Waiting for the connection...");
		//            conntok->wait();

		// Getting the connect response will block waiting for the
		// connection to complete.
		mqtt::connect_response rsp = conntok->get_connect_response();
		spdlog::get("mqtt_logger")->info("  ...OK");

		// Make sure we were granted a v5 connection.
		if (rsp.get_mqtt_version() < MQTTVERSION_5) {
			spdlog::get("mqtt_logger")->error("Did not get an MQTT v5 connection.");
			return false;
		}

		// If there is no session present, then we need to subscribe, but if
		// there is a session, then the server remembers us and our
		// subscriptions.
		//            if (!rsp.is_session_present())
		{
			//spdlog::get("mqtt_logger")->info("Session not present on broker. Subscribing.");
			//m_mqttClient->subscribe(TOPICS, VEC_QOS)->wait();

			// 启动线程
			startConsumeThread = std::thread(startConsumeMsg, this);
		}

		spdlog::get("mqtt_logger")->info("paho mqtt connect to broker {} OK", serverURI);
	} catch (const mqtt::exception &excp) {
		spdlog::get("mqtt_logger")->error("\nERROR: Unable to connect to MQTT server: '{}': {}", serverURI, excp.to_string());
		return false;
	}

	m_heartbeatTimer.StartTimer(3000, std::bind(&MqttManager::onHeartbeatTimeout, this));

	return true;
}

bool MqttManager::connect_reqrsp_start(const String &p_host /* = ""*/, const int p_port /* = 1883*/, const String &p_clientId /* = ""*/) {
	String ServerIP = (String)m_mqttConfDict["@ServerIP"];
	std::string host = std::string(ServerIP.utf8().get_data());
	if (p_host != "")
		host = std::string(p_host.utf8().get_data());
	uint16_t port = (uint16_t)((int)m_mqttConfDict["@ServerPort"]);
	if (p_port != 1883)
		port = (uint16_t)p_port;
	std::string clientId = ((String)m_mqttConfDict["@ClientID"]).utf8().get_data();
	if (p_clientId != "") {
		clientId = std::string(p_clientId.utf8().get_data());
	}
	clientId +=  "_req_rsp";

	// mqtt broker
	mqtt::create_options createOpts = mqtt::create_options_builder()
											  .mqtt_version(MQTTVERSION_5)
											  //.send_while_disconnected(true, true)
											  //.max_buffered_messages(MAX_BUFFERED_MESSAGES)
											  //.delete_oldest_messages(true)
											  //.restore_messages(true)
											  //.persist_qos0(true)
											  .finalize();
#ifdef _WIN32
	std::string serverURI = str_format("mqtt://%s:%d", host, port);
#elif defined(__linux__)
	std::string serverURI = util::Format("mqtt://{0}:{1}", host, port);
#endif

#if defined(_WIN32)
	m_mqttReqRspClient = new MqttClient(createOpts, host, port, clientId);
#else
	m_mqttReqRspClient = new MqttClient(createOpts, host, port, clientId);
	//encoded_file_persistence persist("Gdmorph");
	//m_mqttReqRspClient = new MqttClient(createOpts, host, port, clientId, &persist);
#endif
	// Set callbacks for when connected and connection lost.

	m_mqttReqRspClient->set_connected_handler([this](const std::string &) {
		spdlog::get("mqtt_logger")->info("*** Connected ({0:d}) ***", timestamp());
		if (has_signal("MqttConnected"))
			emit_signal("MqttConnected");
	});

	m_mqttReqRspClient->set_connection_lost_handler([this](const std::string &) {
		spdlog::get("mqtt_logger")->info("*** Connected Lost ({0:d}) ***", timestamp());
		if (has_signal("MqttLostconnect"))
			emit_signal("MqttLostconnect");
	});

	m_mqttReqRspClient->set_disconnected_handler([this](const mqtt::properties &, mqtt::ReasonCode reason) {
		spdlog::get("mqtt_logger")->info("*** Disconnected. Reason: {0:d} ***", reason);
		if (has_signal("MqttDisconnected"))
			emit_signal("MqttDisconnected");
	});

	//m_mqttReqRspClient->set_message_callback([this](mqtt::const_message_ptr msg) {
	//	spdlog::get("mqtt_logger")->debug("receive: {} ***", msg->get_payload_str());
	//	if (has_signal("MqttMsgReceived"))
	//		emit_signal("MqttMsgReceived", String(msg->get_topic().c_str()), String(msg->get_payload_str().c_str()), msg->get_qos());
	//});

	m_mqttReqRspClient->set_update_connection_handler([](mqtt::connect_data &connData) {
		std::string newUserName{ "newuser" };
		if (connData.get_user_name() == newUserName)
			return false;

		spdlog::get("mqtt_logger")->debug("Previous user: '{}'", connData.get_user_name());
		connData.set_user_name(newUserName);
		spdlog::get("mqtt_logger")->debug("New user name: '{}'", connData.get_user_name());
		return true;
	});

	//(int)Dictionary(m_mqttConfDict["apples"])["blue"]
	if ((String)m_mqttConfDict["@User"] != "") {
#define CONN_MQTT_BROKER_WITH_USR_PSW
		;
	} else {
#undef CONN_MQTT_BROKER_WITH_USR_PSW
		;
	}
#if defined(_WIN32)
	size_t pos = 0;
	while ((pos = m_prjPath.find("/", pos)) != std::string::npos) {
		m_prjPath.replace(pos, std::string("/").length(), "\\");
		pos += std::string("\\").length(); // Move past the replaced substring
	}
	std::string confPath = m_prjPath + "\\" + "conf" + "\\";
#else
	size_t pos = 0;
	while ((pos = m_prjPath.find("\\", pos)) != std::string::npos) {
		m_prjPath.replace(pos, std::string("\\").length(), "/");
		pos += std::string("/").length(); // Move past the replaced substring
	}
	std::string confPath = m_prjPath + "/" + "conf" + "/";
#endif
	std::string trustFilePath;
	std::string keyFilePath;
	if (m_mqttConfDict["@KeyName"] != "" && m_mqttConfDict["@TrustName"] != "") {
		// Note that we don't actually need to open the trust or key stores.
		// We just need a quick, portable way to check that they exist.
		{
			trustFilePath = confPath + std::string(((String)m_mqttConfDict["@TrustName"]).utf8().get_data());
			std::ifstream tstore(trustFilePath);
			if (!tstore) {
				spdlog::get("mqtt_logger")->warn("The trust store file does not exist: {}", trustFilePath);
				spdlog::get("mqtt_logger")->warn("  Get a copy from \"[prjPath]/conf/mqtt_root_ca.crt\"");
				return false;
			}

			keyFilePath = confPath + std::string(((String)m_mqttConfDict["@KeyName"]).utf8().get_data());
			std::ifstream kstore(keyFilePath);
			if (!kstore) {
				spdlog::get("mqtt_logger")->warn("The key store file does not exist: {}", keyFilePath);
				spdlog::get("mqtt_logger")->warn("  Get a copy from \"[prjPath]/conf/mqtt_client.pem\"");
				return false;
			}
		}
#define CONN_MQTT_BROKER_WITH_SSL
	} else {
#undef CONN_MQTT_BROKER_WITH_SSL
		;
	}

#ifdef CONN_MQTT_BROKER_WITH_SSL
	auto sslopts = mqtt::ssl_options_builder()
						   //        .ssl_version(MQTT_SSL_VERSION_DEFAULT)
						   //        .enabled_cipher_suites(true)
						   //        .verify(true)
						   .trust_store(trustFilePath.toStdString())
						   .key_store(keyFilePath.toStdString())
						   .error_handler([](const std::string &msg) {
							   spdlog::get("mqtt_logger")->info("SSL Error: {}", msg);
						   })
						   //        .private_key(const string& key)
						   //        .private_keypassword(const string& passwd)
						   //        .ca_path(const string& path)

						   //        .psk_handler()([](const std::string& msg,
						   //                       char *identity, size_t max_identity_len,
						   //                       unsigned char *psk, size_t max_psk_len){
						   //            spdlog::get("mqtt_logger")->info("psk Error: {}, identity: {}, max_identity_len: {}, psk: {}, max_psk_len: {}"
						   //                      , msg
						   //						, std::string(identity)
						   //						, (int)max_identity_len
						   //						, std::string(psk)
						   //						, (int)max_psk_len
						   //				);
						   //        })
						   //        .alpn_protos(const std::vector<string>& protos)
						   .finalize();
#endif

	auto willMsg = mqtt::message("scada/prjname/scadawill", "goodbye scada req rsp world", (int)QOS1, true);
	auto connOpts = mqtt::connect_options_builder()
							.mqtt_version(MQTTVERSION_5)
							.clean_start(true) //(MQTT v5 only)
							//            .clean_session(true) //(MQTT v3.x only)
							.connect_timeout(seconds(30))
							.keep_alive_interval(seconds(20))
							.automatic_reconnect(true)
							.automatic_reconnect(seconds(2), seconds(30))
							.will(std::move(willMsg))
							.properties({ { mqtt::property::SESSION_EXPIRY_INTERVAL, INFINITE /*604800*/ } })
#ifdef CONN_MQTT_BROKER_WITH_USR_PSW
							.user_name(clientPara->m_mqttUser.toLocal8Bit().constData());
	.password(clientPara->m_mqttPsw.toLocal8Bit().constData());
#endif
#ifdef CONN_MQTT_BROKER_WITH_SSL
	.ssl(std::move(sslopts))
#endif
			//            .max_inflight(100)
			//            .http_headers()
			//            .http_proxy()
			.finalize();

	//connect_signals();

	//mqttConnectCallback ccb(*m_mqttClient, connOpts);
	//m_mqttClient->set_callback(ccb);

	try {
		// Start consumer before connecting to make sure to not miss messages
		m_mqttReqRspClient->start_consuming();

		spdlog::get("mqtt_logger")->info("Connecting to the MQTT server...");
		// case 1: connect use callback manually
		//m_mqttClient->connect(connOpts, nullptr, ccb);

		// case 2: connect use paho backend itself
		mqtt::token_ptr conntok = m_mqttReqRspClient->connect(connOpts);

		spdlog::get("mqtt_logger")->info("Waiting for the connection...");
		//            conntok->wait();

		// Getting the connect response will block waiting for the
		// connection to complete.
		mqtt::connect_response rsp = conntok->get_connect_response();
		spdlog::get("mqtt_logger")->info("  ...OK");

		// Make sure we were granted a v5 connection.
		if (rsp.get_mqtt_version() < MQTTVERSION_5) {
			spdlog::get("mqtt_logger")->error("Did not get an MQTT v5 connection.");
			return false;
		}

		// If there is no session present, then we need to subscribe, but if
		// there is a session, then the server remembers us and our
		// subscriptions.
		//            if (!rsp.is_session_present())
		{
			//spdlog::get("mqtt_logger")->info("Session not present on broker. Subscribing.");
			//m_mqttClient->subscribe(TOPICS, VEC_QOS)->wait();

			// 启动线程
			//std::thread startConsumeThread(startConsumeMsg, this);
		}

		spdlog::get("mqtt_logger")->info("paho mqtt connect to broker {} OK", serverURI);
	} catch (const mqtt::exception &excp) {
		spdlog::get("mqtt_logger")->error("\nERROR: Unable to connect to MQTT server: '{}': {}", serverURI, excp.to_string());
		return false;
	}

	//m_heartbeatTimer.StartTimer(3000, std::bind(&MqttManager::onHeartbeatTimeout, this));

	// Start background thread that reads all incoming messages and dispatches by CORRELATION_DATA.
	// Each reply topic is subscribed on first use in req_rsp() and kept permanently.
	m_reqRspRunning = true;
	m_reqRspConsumeThread = std::thread(&MqttManager::_reqrsp_consume_loop, this);
	spdlog::get("mqtt_logger")->info("connect_reqrsp_start: background dispatch thread started");

	return true;
}

void MqttManager::_reqrsp_consume_loop() {
	spdlog::get("mqtt_logger")->info("_reqrsp_consume_loop: started");
	using namespace std::chrono_literals;
	while (m_reqRspRunning) {
		// Short poll interval keeps dispatch latency low; exits promptly when stopped
		auto msg = m_mqttReqRspClient->try_consume_message_for(100ms);
		if (!msg)
			continue;

		// Extract corrId from the last path component of the response topic.
		// For p_auto_mode=false, RESPONSE_TOPIC = rsptopic + "/" + corrId, so the server
		// publishes to that unique topic and the topic suffix IS the corrId.
		std::string corrId;
		const std::string &msgTopic = msg->get_topic();
		size_t slashPos = msgTopic.rfind('/');
		if (slashPos != std::string::npos) {
			corrId = msgTopic.substr(slashPos + 1);
		}

		if (corrId.empty()) {
			spdlog::get("mqtt_logger")->warn("_reqrsp_consume_loop: cannot extract corrId from topic={}", msgTopic);
			continue;
		}

		std::lock_guard<std::mutex> lock(m_inFlightMutex);
		auto it = m_inFlightRequests.find(corrId);
		if (it != m_inFlightRequests.end()) {
			it->second.set_value(msg->get_payload_str());
			m_inFlightRequests.erase(it);
		} else {
			// Caller already timed out and cleaned up — discard the late response
			spdlog::get("mqtt_logger")->warn("_reqrsp_consume_loop: no in-flight entry for corrId={} (caller timed out?)", corrId);
		}
	}
	spdlog::get("mqtt_logger")->info("_reqrsp_consume_loop: stopped");
}

bool MqttManager::subscribe(const String &p_topic, MqttManager::QOS p_qos /* = MqttManager::QOS::QOS0*/) {
	if (!m_mqttClient)
		return false;

	if (!m_mqttClient->isConnectedToHost())
		return false;

	std::string topic(p_topic.utf8().get_data());
	spdlog::get("mqtt_logger")->info("start Subscribing topic: {}, qos: {}", topic, (int)p_qos);
#ifdef _WIN32
	bool bret = m_mqttClient->subscribe(topic, (int)p_qos)->wait_for(seconds(3));
#elif defined(__linux__)
	bool bret = m_mqttClient->subscribe(topic, (int)p_qos)->wait_for(seconds(3));
#endif
	if (!bret) {
		spdlog::get("mqtt_logger")->warn("Error: Subscrib to topic [{}] , qos: {}: elapse {} seconds timeout fail!", topic, (int)p_qos, 3);
		return false;
	}
	spdlog::get("mqtt_logger")->info("end Subscribing topic: {}, qos: {}", topic, (int)p_qos);

	if (std::find(std_subscribedTopics.begin(), std_subscribedTopics.end(), topic) == std_subscribedTopics.end()) {
		// Element not in vector std_subscribedTopics.
		std_subscribedTopics.push_back(topic);
		std_subscribedQos.push_back((int)p_qos);
	}

	return true;
}

bool MqttManager::subscribe1(const String &p_topic, const int &p_qos /*= 0*/) {
	MqttManager::QOS qos = (MqttManager::QOS)p_qos;
	return subscribe(p_topic, qos);
}

bool MqttManager::autoReSubscribe() {
	if (!m_mqttClient)
		return false;

	if (!m_mqttClient->isConnectedToHost())
		return false;

	if (std_subscribedTopics.size() > 0 && (std_subscribedTopics.size() == std_subscribedQos.size())) {
		spdlog::get("mqtt_logger")->info("autoReSubscribe topic: {}, qos: {}", fmt::join(std_subscribedTopics, ", "), fmt::join(std_subscribedQos, ", "));
		const_string_collection_ptr subtopics = const_string_collection_ptr(mqtt::string_collection::create(std_subscribedTopics));
		bool bret = m_mqttClient->subscribe(subtopics, std_subscribedQos)->wait_for(seconds(5));
		if (!bret) {
			spdlog::get("mqtt_logger")->warn("Error: autoReSubscribe to topic [{}], qos: {} : elapse {} seconds timeout fail!", fmt::join(std_subscribedTopics, ", "), fmt::join(std_subscribedQos, ", "), 5);
			return false;
		}
	}

	return true;
}

bool MqttManager::unsubscribe(const String &p_topic) {
	if (!m_mqttClient)
		return false;

	if (!m_mqttClient->isConnectedToHost())
		return false;

	std::string topic(p_topic.utf8().get_data());
	spdlog::get("mqtt_logger")->info("unSubscribing topic: {}", topic);
	bool bret = m_mqttClient->unsubscribe(topic)->wait_for(seconds(3));
	if (!bret) {
		spdlog::get("mqtt_logger")->warn("Error: unSubscribing to topic [{}] : elapse {} seconds timeout fail!", topic, 3);
		return false;
	}

	std_subscribedTopics.erase(std::remove(std_subscribedTopics.begin(), std_subscribedTopics.end(), topic), std_subscribedTopics.end());
	int idx = getIndexInVector(std_subscribedTopics, topic);
	if ( idx >=0 )
		std_subscribedQos.erase(std_subscribedQos.begin() + idx);

	return true;
}

bool MqttManager::publish(const String &p_topic, const String &p_msg, MqttManager::QOS p_qos /* = MqttManager::QOS::QOS0*/) {
	if (!m_mqttClient)
		return false;

	if (!m_mqttClient->isConnectedToHost())
		return false;

	// 若外部命令未给ID号，则以__IoT节点配置的描述中为准
	std::string topic(p_topic.utf8().get_data());
	std::string msg(p_msg.utf8().get_data());

	//    mqtt::properties props {
	//        { mqtt::property::RESPONSE_TOPIC, repTopic },
	//        { mqtt::property::CORRELATION_DATA, "1" }
	//    };
	auto pubmsg = mqtt::message_ptr_builder()
							.topic(topic)
							.payload(msg)
							.qos((int)p_qos)
							.retained(false)
							//.properties(props)
							.finalize();

	mqtt::delivery_token_ptr pubtok = m_mqttClient->publish(pubmsg);
	int msgid = pubtok->get_message_id();
	spdlog::get("mqtt_logger")->info("  ...with token: {}  ...for message with {} bytes", pubtok->get_message_id(), pubtok->get_message()->get_payload().size());
	pubtok->wait_for(seconds(5));

	std::thread::id threadID = std::this_thread::get_id();
	spdlog::get("mqtt_logger")->debug("send to mqtt topic={}, msg={}, msgid={}, current threadid={}", topic, msg, msgid, *(unsigned int *)&threadID);

	return true;
}

String MqttManager::req_rsp(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, MqttManager::QOS p_qos /* = MqttManager::QOS::QOS1*/, int p_secs_timeout /*  = 5*/, bool p_auto_mode /* = true*/) {

	String ret =
		"{\"msg\" : \"error!\", \"result\" : \"0\" }";

	// Thread-safe lazy init: first caller (any thread) creates and connects the req/rsp client.
	// Subsequent callers skip this entirely with no locking overhead.
	std::call_once(m_reqRspOnceFlag, [this]() {
		connect_reqrsp_start();
	});

	if (!m_mqttReqRspClient)
		return ret;

	if (!m_mqttReqRspClient->isConnectedToHost())
		return ret;

	// 若外部命令未给ID号，则以__IoT节点配置的描述中为准
	String t_reqtopic = p_reqtopic;
	if (t_reqtopic.begins_with("req/") && !t_reqtopic.ends_with("/" + p_remoteClientId))
		t_reqtopic += "/" + p_remoteClientId;
	std::string reqtopic(t_reqtopic.utf8().get_data());
	std::string reqmsg(p_reqmsg.utf8().get_data());
	String t_rsptopic = p_rsptopic;
	if (t_rsptopic.begins_with("rsp/") && !t_rsptopic.ends_with("/" + p_remoteClientId))
		t_rsptopic += "/" + p_remoteClientId;
	std::string rsptopic(t_rsptopic.utf8().get_data());

	// Unique correlation ID generated first so we can embed it in rsptopic_prop below.
	uint64_t corrIdNum = m_corrIdCounter.fetch_add(1, std::memory_order_relaxed);
	std::string corrId = std::to_string(corrIdNum);

	// Determine RESPONSE_TOPIC for this call.
	// p_auto_mode=false (normal): append corrId to rsptopic — each call gets a unique response
	//   topic so routing does not depend on server echoing CORRELATION_DATA.
	// p_auto_mode=true: keep "rsp/auto" unchanged (original behavior preserved).
	std::string rsptopic_prop;
	if (p_auto_mode) {
		rsptopic_prop = "rsp/auto";
	} else {
		rsptopic_prop = rsptopic + "/" + corrId;
	}

	// Subscribe to rsptopic/# once per unique base topic (wildcard covers all corrId variants).
	// Pre-insert under lock prevents two threads from subscribing the same wildcard twice.
	std::string wildcard = rsptopic + "/#";
	bool needSubscribe = false;
	{
		std::lock_guard<std::mutex> lock(m_inFlightMutex);
		needSubscribe = (m_reqRspSubscribedTopics.count(wildcard) == 0);
		if (needSubscribe)
			m_reqRspSubscribedTopics.insert(wildcard); // reserve slot; erased on failure below
	}
	if (needSubscribe) {
		spdlog::get("mqtt_logger")->info("start Subscribing topic: {}, qos: {}", wildcard, (int)p_qos);
		mqtt::token_ptr tok = m_mqttReqRspClient->subscribe(wildcard, (int)p_qos);
		bool bret = tok->wait_for(seconds(p_secs_timeout));
		if (!bret) {
			spdlog::get("mqtt_logger")->warn("Error: subscribe to topic [{}] with qos {}: elapse {} seconds timeout fail!", wildcard, (int)p_qos, p_secs_timeout);
			std::lock_guard<std::mutex> lock(m_inFlightMutex);
			m_reqRspSubscribedTopics.erase(wildcard); // rollback so next call retries
			return ret;
		}
		spdlog::get("mqtt_logger")->info("end Subscribing topic: {}, qos: {}", wildcard, (int)p_qos);

		/*	reson code description
		{ MQTTREASONCODE_SUCCESS, "SUCCESS" },
		{ MQTTREASONCODE_NORMAL_DISCONNECTION, "Normal disconnection" },
		{ MQTTREASONCODE_GRANTED_QOS_0, "Granted QoS 0" },
		{ MQTTREASONCODE_GRANTED_QOS_1, "Granted QoS 1" },
		{ MQTTREASONCODE_GRANTED_QOS_2, "Granted QoS 2" },
		{ MQTTREASONCODE_DISCONNECT_WITH_WILL_MESSAGE, "Disconnect with Will Message" },
		{ MQTTREASONCODE_NO_MATCHING_SUBSCRIBERS, "No matching subscribers" },
		{ MQTTREASONCODE_NO_SUBSCRIPTION_FOUND, "No subscription found" },
		{ MQTTREASONCODE_CONTINUE_AUTHENTICATION, "Continue authentication" },
		{ MQTTREASONCODE_RE_AUTHENTICATE, "Re-authenticate" },
		{ MQTTREASONCODE_UNSPECIFIED_ERROR, "Unspecified error" },
		{ MQTTREASONCODE_MALFORMED_PACKET, "Malformed Packet" },
		{ MQTTREASONCODE_PROTOCOL_ERROR, "Protocol error" },
		{ MQTTREASONCODE_IMPLEMENTATION_SPECIFIC_ERROR, "Implementation specific error" },
		{ MQTTREASONCODE_UNSUPPORTED_PROTOCOL_VERSION, "Unsupported Protocol Version" },
		{ MQTTREASONCODE_CLIENT_IDENTIFIER_NOT_VALID, "Client Identifier not valid" },
		{ MQTTREASONCODE_BAD_USER_NAME_OR_PASSWORD, "Bad User Name or Password" },
		{ MQTTREASONCODE_NOT_AUTHORIZED, "Not authorized" },
		{ MQTTREASONCODE_SERVER_UNAVAILABLE, "Server unavailable" },
		{ MQTTREASONCODE_SERVER_BUSY, "Server busy" },
		{ MQTTREASONCODE_BANNED, "Banned" },
		{ MQTTREASONCODE_SERVER_SHUTTING_DOWN, "Server shutting down" },
		{ MQTTREASONCODE_BAD_AUTHENTICATION_METHOD, "Bad authentication method" },
		{ MQTTREASONCODE_KEEP_ALIVE_TIMEOUT, "Keep Alive timeout" },
		{ MQTTREASONCODE_SESSION_TAKEN_OVER, "Session taken over" },
		{ MQTTREASONCODE_TOPIC_FILTER_INVALID, "Topic filter invalid" },
		{ MQTTREASONCODE_TOPIC_NAME_INVALID, "Topic name invalid" },
		{ MQTTREASONCODE_PACKET_IDENTIFIER_IN_USE, "Packet Identifier in use" },
		{ MQTTREASONCODE_PACKET_IDENTIFIER_NOT_FOUND, "Packet Identifier not found" },
		{ MQTTREASONCODE_RECEIVE_MAXIMUM_EXCEEDED, "Receive Maximum exceeded" },
		{ MQTTREASONCODE_TOPIC_ALIAS_INVALID, "Topic Alias invalid" },
		{ MQTTREASONCODE_PACKET_TOO_LARGE, "Packet too large" },
		{ MQTTREASONCODE_MESSAGE_RATE_TOO_HIGH, "Message rate too high" },
		{ MQTTREASONCODE_QUOTA_EXCEEDED, "Quota exceeded" },
		{ MQTTREASONCODE_ADMINISTRATIVE_ACTION, "Administrative action" },
		{ MQTTREASONCODE_PAYLOAD_FORMAT_INVALID, "Payload format invalid" },
		{ MQTTREASONCODE_RETAIN_NOT_SUPPORTED, "Retain not supported" },
		{ MQTTREASONCODE_QOS_NOT_SUPPORTED, "QoS not supported" },
		{ MQTTREASONCODE_USE_ANOTHER_SERVER, "Use another server" },
		{ MQTTREASONCODE_SERVER_MOVED, "Server moved" },
		{ MQTTREASONCODE_SHARED_SUBSCRIPTIONS_NOT_SUPPORTED, "Shared subscriptions not supported" },
		{ MQTTREASONCODE_CONNECTION_RATE_EXCEEDED, "Connection rate exceeded" },
		{ MQTTREASONCODE_MAXIMUM_CONNECT_TIME, "Maximum connect time" },
		{ MQTTREASONCODE_SUBSCRIPTION_IDENTIFIERS_NOT_SUPPORTED, "Subscription Identifiers not supported" },
		{ MQTTREASONCODE_WILDCARD_SUBSCRIPTIONS_NOT_SUPPORTED, "Wildcard Subscriptions not supported" }
		*/
		if (int(tok->get_reason_code()) > (int)MqttManager::QOS::QOS2) {
			// reason codes 0/1/2 = granted QoS0/1/2 (all acceptable); > 2 = error
			spdlog::get("mqtt_logger")->warn("Error: Server subscribe failed, reason code: [{}]", tok->get_reason_code());
			std::lock_guard<std::mutex> lock(m_inFlightMutex);
			m_reqRspSubscribedTopics.erase(wildcard); // rollback so next call retries
			return ret;
		}
	}

	// Register promise BEFORE publishing to eliminate the race where the response arrives
	// (and gets dispatched by _reqrsp_consume_loop) before we are ready to receive it.
	std::future<std::string> future;
	{
		std::lock_guard<std::mutex> lock(m_inFlightMutex);
		auto result = m_inFlightRequests.emplace(corrId, std::promise<std::string>{});
		future = result.first->second.get_future();
	}

	mqtt::properties props {
		{ mqtt::property::RESPONSE_TOPIC, rsptopic_prop },
	    { mqtt::property::CORRELATION_DATA, corrId }
	};
	int pub_qos = p_qos;
#if defined(__linux__)
	pub_qos = 1;
#endif
	auto pubmsg = mqtt::message_ptr_builder()
						  .topic(reqtopic)
						  .payload(reqmsg)
						  .qos(pub_qos)
						  .retained(false)
						  .properties(props)
						  .finalize();

	spdlog::get("mqtt_logger")->info("start publish topic: {}, qos: {}, msg: {}", reqtopic, (int)pub_qos, pubmsg->to_string());
	mqtt::delivery_token_ptr pubtok = m_mqttReqRspClient->publish(pubmsg);
	int msgid = pubtok->get_message_id();
	spdlog::get("mqtt_logger")->info("  ...with token: {}  ...for message with {} bytes", pubtok->get_message_id(), pubtok->get_message()->get_payload().size());
	pubtok->wait_for(seconds(p_secs_timeout));
	spdlog::get("mqtt_logger")->info("end publish topic: {}, qos: {}, msg: {}", reqtopic, (int)pub_qos, pubmsg->to_string());

	// Wait for _reqrsp_consume_loop to fulfill our promise.
	// The full p_secs_timeout budget is available for server-side processing.
	auto status = future.wait_for(seconds(p_secs_timeout));
	if (status == std::future_status::timeout) {
		spdlog::get("mqtt_logger")->warn("req_rsp timeout ({}s) waiting for corrId={}, reqtopic={}", p_secs_timeout, corrId, reqtopic);
		// Remove stale entry; if the response arrives later it will be discarded by the dispatch loop
		std::lock_guard<std::mutex> lock(m_inFlightMutex);
		m_inFlightRequests.erase(corrId);
		return ret;
	}

	std::string rspmsg_tmp = future.get();
	String rspmsg_tmp1 = getUtf8String(rspmsg_tmp);

	// 如果需要验证回馈信息，在此处验证corr_id数据
	//const mqtt::properties &props = rspmsg->get_properties();
	//if (props.contains(CORRELATION_DATA)) {
	//	mqtt::binary corr_id = mqtt::get<string>(props, CORRELATION_DATA);

	//	spdlog::get("mqtt_logger")->info("Client get a CORRELATION_DATA=[{}]", corr_id);
	//}

	std::thread::id threadID = std::this_thread::get_id();
	spdlog::get("mqtt_logger")->debug("send request to mqtt reqtopic={}, reqmsg={}, qos={}, msgid={}, wanted rsptopic={}, and get rspmsg={}, current threadid={}", reqtopic, reqmsg, (int)p_qos, msgid, rsptopic, rspmsg_tmp, *(unsigned int *)&threadID);
	return rspmsg_tmp1;
}

String MqttManager::req_rsp1(const String &p_reqtopic, const String &p_reqmsg, const String &p_rsptopic, const String &p_remoteClientId, int p_qos /* = 2*/, int p_secs_timeout /* = 5*/, bool p_auto_mode /* = true*/) {
	MqttManager::QOS qos = (MqttManager::QOS)p_qos;
	return req_rsp(p_reqtopic, p_reqmsg, p_rsptopic, p_remoteClientId, qos, p_secs_timeout, p_auto_mode);
}

bool MqttManager::connect_close() {
	// Stop background req_rsp dispatch thread before closing the connection
	if (m_reqRspRunning) {
		m_reqRspRunning = false;
		if (m_reqRspConsumeThread.joinable())
			m_reqRspConsumeThread.join();
		spdlog::get("mqtt_logger")->info("connect_close: req_rsp dispatch thread stopped");
	}

	// Cancel all pending in-flight calls so waiting threads unblock immediately
	{
		std::lock_guard<std::mutex> lock(m_inFlightMutex);
		for (auto &kv : m_inFlightRequests) {
			try {
				kv.second.set_exception(std::make_exception_ptr(
					std::runtime_error("MqttManager closed while request in flight")));
			} catch (...) {}
		}
		m_inFlightRequests.clear();
	}

	if (!m_mqttClient)
		return false;

	if (!m_mqttClient->isConnectedToHost())
		return true;

	return m_mqttClient->disconnect()->wait_for(seconds(3));
}

Error MqttManager::invokeMethod(const String &p_nodePath, const Array &p_inputArguments, Array p_outputArguments, const String &p_remoteClientId) {

	String req_topic = p_nodePath;
	bool bret = true;
	// 项目
	//    projectExpertImp
	//    if(req_topic == "req/g_Project/xxx")
	//    {

	//    }

	//////////////////////////////////////////
	// 变量专家 g_Variable
	//////////////////////////////////////////
	//    projectExpertImp->m_variableExpert
	Dictionary req_msg_dict;
	String remoteClientId = p_remoteClientId;
	if (req_topic == "req/g_Variable/readVariable") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// type:value
			// eg:
			// {
			//		"result": "2:754"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Vector<String> ret_vec = result.split(":", true, 2);
			int v_typr = ret_vec[0].to_int();
			String v_str = ret_vec[1];
			Variant v_value;
			// 根据类型 v_typr 计算 v_value
			// QVariant Type 
			/*enum Type {
				Invalid = QMetaType::UnknownType,
				Bool = QMetaType::Bool,
				Int = QMetaType::Int,
				UInt = QMetaType::UInt,
				LongLong = QMetaType::LongLong,
				ULongLong = QMetaType::ULongLong,
				Double = QMetaType::Double,
				Char = QMetaType::QChar,
				Map = QMetaType::QVariantMap,
				List = QMetaType::QVariantList,
				String = QMetaType::QString,
				StringList = QMetaType::QStringList,
				ByteArray = QMetaType::QByteArray,
				BitArray = QMetaType::QBitArray,
				Date = QMetaType::QDate,
				Time = QMetaType::QTime,
				DateTime = QMetaType::QDateTime,*/
			switch (v_typr) {
				case 0: // Invalid
					break;
				case 1: // Bool
					v_value = v_str.to_int() ? true : false;
					break;
				case 2: // Int
				case 3: // UInt
				case 4: // LongLong
				case 5: // ULongLong
				case 7: // Char
					v_value = v_str.to_int();
					break;
				case 6: // Double
					v_value = v_str.to_float();
					break;
				case 10: // String
					v_value = v_str;
					break;
				case 16: // DateTime
					v_value = v_str.to_int();
					break;
				default:
					break;
			}
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Variable/writeVariable") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["value"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	 else if (req_topic == "req/g_Variable/writeVariableWithoutSig") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["value"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Variable/writeVariableRelative")
	{
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["value"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Variable/writeVariableSwitch01") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Variable/writeVariables") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["names"] = Array(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["values"] = Array(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Variable/writeVariablesWithoutSig") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["names"] = Array(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["values"] = Array(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}

	//// 变量对象
	////    IMDRTDBManager* rtdbMgr = projectExpertImp->m_variableExpert.RTDBManager();
	////    IMDVariableManager* varMgr = rtdbMgr->variableManager();
	////    int count = varMgr->getVariableCount();
	////    for (int i = 0; i < count; i++)
	////    {
	////        IMDRTDBVariable* var = varMgr->getVariableAt(i);
	//////        if(var->isEnableScript())
	////        {
	////            QString sName = var->name().replace('.', '_');
	////            CMDVariableWrapper* wrapper = new CMDVariableWrapper(var, sName);
	////            m_wrappersMgr.addWrapper(wrapper);
	////        }
	////    }

	////    // 数据归档
	////    CMDDataArchiveExpertWrapper* daeWrapper = new CMDDataArchiveExpertWrapper(&projectExpertImp->m_historyExpert, "g_DataSource");
	////    m_wrappersMgr.addWrapper(daeWrapper);

	////    // 数据查询
	////    CMDDataQueryExpertWrapper* dqeWrapper = new CMDDataQueryExpertWrapper(projectExpertImp->m_historyExpert.qTaskMgr(), "g_Database");
	////    m_wrappersMgr.addWrapper(dqeWrapper);

	//////////////////////////////////////////
	// 窗体 g_Window
	//////////////////////////////////////////
	//    projectExpertImp->m_proxyMgr.m_hmiProxy
	else if (req_topic == "req/g_Window/open") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["pwid"] = (signed int)(p_inputArguments[1]);
			if (p_inputArguments.size() > 2)
				req_msg_dict["x"] = (signed int)(p_inputArguments[2]);
			if (p_inputArguments.size() > 3)
				req_msg_dict["y"] = (signed int)(p_inputArguments[3]);
			if (p_inputArguments.size() > 4)
				req_msg_dict["w"] = (signed int)(p_inputArguments[4]);
			if (p_inputArguments.size() > 5)
				req_msg_dict["h"] = (signed int)(p_inputArguments[5]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/move") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["left"] = (signed int)(p_inputArguments[1]);
			if (p_inputArguments.size() > 2)
				req_msg_dict["top"] = (signed int)(p_inputArguments[2]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/callJsFunc") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["funcName"] = String(p_inputArguments[1]);
			if (p_inputArguments.size() > 2) {
				Dictionary params_dict = JSON::parse_string(String(p_inputArguments[2]));
				req_msg_dict["params"] = params_dict;
			}
				
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/hide") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Window/hideByPid") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		req_msg_dict["pid"] = 0;
		req_msg_dict["phwnd"] = 0; // 0 - 全局根据pid找 mainwindow 并 hide, >0 - 全局根据pid找到主进程,并在进程内查找句柄为phwnd的窗口并hide
		if (p_inputArguments.size() > 0) {
			req_msg_dict["pid"] = (signed int)(p_inputArguments[0]);
			if (p_inputArguments.size() > 1) {
				req_msg_dict["phwnd"] = (signed int)(p_inputArguments[1]);
			}
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/showByPid") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		req_msg_dict["pid"] = 0;
		req_msg_dict["phwnd"] = 0; // 0 - 全局根据pid找 mainwindow 并 hide, >0 - 全局根据pid找到主进程,并在进程内查找句柄为phwnd的窗口并hide
		if (p_inputArguments.size() > 0) {
			req_msg_dict["pid"] = (signed int)(p_inputArguments[0]);
			if (p_inputArguments.size() > 1) {
				req_msg_dict["phwnd"] = (signed int)(p_inputArguments[1]);
			}
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/restore") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/hideRuntime") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Window/showRuntime") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Window/hideRecipeRuntime") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Window/showRecipeRuntime") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Window/getWid") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 1377760
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Window/close") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/closeByPid") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		req_msg_dict["pid"] = 0;
		req_msg_dict["phwnd"] = 0; // 0 - 全局根据pid找 mainwindow 并 hide, >0 - 全局根据pid找到主进程,并在进程内查找句柄为phwnd的窗口并hide
		if (p_inputArguments.size() > 0) {
			req_msg_dict["pid"] = (signed int)(p_inputArguments[0]);
			if (p_inputArguments.size() > 1) {
				req_msg_dict["phwnd"] = (signed int)(p_inputArguments[1]);
			}
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Window/enterFullScreen") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Window/exitFullScreen") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}

	//////////////////////////////////////////
	// 安全 g_Authorize
	//////////////////////////////////////////
	//    projectExpertImp->m_securityExpert
	else if (req_topic == "req/g_Authorize/accountSignature") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["actionInfo"] = Dictionary(JSON::parse_string(String(p_inputArguments[0])));
			if (p_inputArguments.size() > 1) {
				req_msg_dict["sigConf"] = Dictionary(JSON::parse_string(String(p_inputArguments[1])));
			}
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 0
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/accountsCount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": 10
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Authorize/addAccountToGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["account"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["group"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/deleteAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/deleteGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/disableAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/disableGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/enableAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/enableGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/commit") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Authorize/getAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	}
	else if (req_topic == "req/g_Authorize/getAccountGroups") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": [...]
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Array result = Array(rsp_msg_dict["result"]);
			Variant v_value = JSON::stringify(result);
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Authorize/getAccountOption") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	}
	else if (req_topic == "req/g_Authorize/getAccounts") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": [...]
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		Array result = Array(rsp_msg_dict["result"]);
		Variant v_value = JSON::stringify(result);
		p_outputArguments.push_back(v_value);
	}
	else if (req_topic == "req/g_Authorize/getAccountsInGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["group"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": [...]
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Array result = Array(rsp_msg_dict["result"]);
			Variant v_value = JSON::stringify(result);
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/getCurrentAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "003"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = String(rsp_msg_dict["result"]);
		p_outputArguments.push_back(result);
	} else if (req_topic == "req/g_Authorize/getDisabledAccounts") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": [...]
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		Array result = Array(rsp_msg_dict["result"]);
		Variant v_value = JSON::stringify(result);
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/getDisabledGroups") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": [...]
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		Array result = Array(rsp_msg_dict["result"]);
		Variant v_value = JSON::stringify(result);
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/getGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	} else if (req_topic == "req/g_Authorize/getGroups") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": [...]
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		Array result = Array(rsp_msg_dict["result"]);
		Variant v_value = JSON::stringify(result);
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/getLockedAccounts") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": [...]
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		Array result = Array(rsp_msg_dict["result"]);
		Variant v_value = JSON::stringify(result);
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/groupsCount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": 10
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/isAccountEnabled") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/isAccountInGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["account"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["group"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/isAccountLocked") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/isAccountLogin") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/lockAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/login") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["password"] = String(p_inputArguments[1]);
			req_msg_dict["checkGroup"] = 0;
			if (p_inputArguments.size() > 2)
				req_msg_dict["checkGroup"] = (signed int)(p_inputArguments[2]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 1
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/logout") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": 1
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/modifyPassword") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["oldPassword"] = String(p_inputArguments[1]);
			if (p_inputArguments.size() > 2)
				req_msg_dict["newPassword"] = String(p_inputArguments[2]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 1
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/removeAccountFromGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["account"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["group"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/resetAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/resetAllAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = String(rsp_msg_dict["result"]);
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Authorize/setAccountOption") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["option"] = Dictionary(JSON::parse_string(String(p_inputArguments[0])));
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 0
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = (String)rsp_msg_dict["result"];
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/unlockAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/upsertAccount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			if (p_inputArguments.size() > 2)
				req_msg_dict["password"] = String(p_inputArguments[2]);
			if (p_inputArguments.size() > 3)
				req_msg_dict["enable"] = bool(p_inputArguments[3]);
			if (p_inputArguments.size() > 4)
				req_msg_dict["lock"] = bool(p_inputArguments[4]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Authorize/upsertGroup") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["name"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			if (p_inputArguments.size() > 2)
				req_msg_dict["password"] = String(p_inputArguments[2]);
			if (p_inputArguments.size() > 3)
				req_msg_dict["enable"] = bool(p_inputArguments[3]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}

	//////////////////////////////////////////
	// 报警 g_Alarm
	//////////////////////////////////////////
	//    projectExpertImp->m_alarmExpert
	else if (req_topic == "req/g_Alarm/acknowledge") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["alarmId"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Alarm/acknowledgeAll") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = String(rsp_msg_dict["result"]);
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Alarm/addComment") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["alarmId"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Alarm/confirm") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["alarmId"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Alarm/confirmAll") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": "1"
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = String(rsp_msg_dict["result"]);
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Alarm/getAlarmById") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["id"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	} else if (req_topic == "req/g_Alarm/getAlarmByIndex") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["index"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	} else if (req_topic == "req/g_Alarm/getAlarmConfById") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["id"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	} else if (req_topic == "req/g_Alarm/getAlarmCount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		String req_msg = JSON::stringify(req_msg_dict);
		String rsp_msg = "";
		rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
		// rsp_msg 结果模板
		// eg:
		// {
		//		"result": 10
		// }
		Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
		String result = (String)rsp_msg_dict["result"];
		Variant v_value = result.to_int();
		p_outputArguments.push_back(v_value);
	} else if (req_topic == "req/g_Alarm/shelve") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["alarmId"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Alarm/suppress") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["alarmId"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["comment"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}

	//////////////////////////////////////////
	// 配方 g_Recipe
	//////////////////////////////////////////
	//    projectExpertImp->m_recipeExpert
	else if (req_topic == "req/g_Recipe/copy") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["srcRecipeName"] = String(p_inputArguments[0]);
			if (p_inputArguments.size() > 1)
				req_msg_dict["destRecipeName"] = String(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/create") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/download") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			req_msg_dict["showProcessDialog"] = 1;
			if (p_inputArguments.size() > 1)
				req_msg_dict["showProcessDialog"] = (signed int)(p_inputArguments[1]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/enumRecipe") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["onlyRecipe"] = 0;
			req_msg_dict["onlyRecipe"] = (signed int)(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": [...]
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Array result = Array(rsp_msg_dict["result"]);
			Variant v_value = JSON::stringify(result);
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/exists") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/getContent") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// case 1: fail
			// {
			//		"result": "0"
			// }

			// case 2: succeed
			// {
			//		"result": {...}
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			Variant result_ret = rsp_msg_dict["result"];
			String result = String(result_ret);
			if (result.is_numeric()) {
				Variant v_value = result.to_int();
				p_outputArguments.push_back(v_value);
			} else {
				p_outputArguments.push_back(result_ret.to_json_string());
			}
		}
	} else if (req_topic == "req/g_Recipe/getRecipeCount") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["onlyRecipe"] = 0;
			req_msg_dict["onlyRecipe"] = (signed int)(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": 12
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}
	else if (req_topic == "req/g_Recipe/getRecipeName") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["index"] = 0;
			req_msg_dict["index"] = (signed int)(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "usr/xxx.rcp"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result;
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/remove") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/save") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			req_msg_dict["content"] = Dictionary(JSON::parse_string(String(p_inputArguments[0])));
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/toFileName") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "c:/prj/Recipe/usr/xxx.rcp"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result;
			p_outputArguments.push_back(v_value);
		}
	} else if (req_topic == "req/g_Recipe/upload") {
		String rsp_topic = req_topic.replace_first("req/", "rsp/");
		if (p_inputArguments.size() > 0) {
			req_msg_dict["recipeName"] = String(p_inputArguments[0]);
			String req_msg = JSON::stringify(req_msg_dict);
			String rsp_msg = "";
			rsp_msg = req_rsp(req_topic, req_msg, rsp_topic, remoteClientId, MqttManager::QOS1);
			// rsp_msg 结果模板
			// eg:
			// {
			//		"result": "1"
			// }
			Dictionary rsp_msg_dict = JSON::parse_string(rsp_msg);
			String result = String(rsp_msg_dict["result"]);
			Variant v_value = result.to_int();
			p_outputArguments.push_back(v_value);
		}
	}

	//////////////////////////////////////////
	// others Unknown request
	//////////////////////////////////////////
	else {
		spdlog::get("mqtt_logger")->error("Unknown request topic: {}", std::string(req_topic.utf8().get_data()));
		p_outputArguments.push_back(0);
		p_outputArguments.push_back("Unknown request topic: " + req_topic);
		bret = false;
	}

	if (bret && p_outputArguments.size() > 0) {
		bret = true;
	}

	return bret ? Error::OK : Error::FAILED;
}


void MqttManager::onHeartbeatTimeout() {
	// 周期 发送心跳包
	// 获取当前时间点
	auto now = std::chrono::system_clock::now();
	// 转换为time_t格式
	std::time_t now_c = std::chrono::system_clock::to_time_t(now - std::chrono::hours(24));
	// 转换为tm结构
	// case 1: localtime
	//std::tm *now_tm = std::localtime(&now_c);
	
	// case 2: localtime_s
	struct std::tm timeinfo;
#ifdef __clang__
/*code specific to clang compiler*/
#elif __GNUC__
/*code for GNU C compiler */
	localtime_r(&now_c, &timeinfo);
#elif _MSC_VER
/*usually has the version number in _MSC_VER*/
/*code specific to MSVC compiler*/
	localtime_s(&timeinfo, &now_c);
#elif __BORLANDC__
/*code specific to borland compilers*/
#elif __MINGW32__
/*code specific to mingw compilers*/
#endif
	std::tm *now_tm = &timeinfo;

	std::stringstream ss;
	ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S");
	std::string str_time = ss.str();

	// 输出格式化时间
	String time_str_now = getUtf8String(str_time);
	Dictionary jdict;
	Dictionary head;
	head["V"] = "1.0";
	jdict["H"] = head;
	jdict["GW"] = getUtf8String(m_mqttClient->clientId());

	Array vsArr;
	Dictionary vs;
	vs["ID"] = "ONLINE";
	vs["Q"] = 192;
	vs["T"] = time_str_now;
	vs["V"] = "1";
	vsArr.append(vs);
	jdict["VS"] = vsArr;

	//String topic = getUtf8String(str_format(GW2SERVER_HEARTBEAT_TOPIC, m_mqttClient->clientId()));
	String topic = getUtf8String("Status/"+m_mqttClient->clientId());
	String msg = JSON::stringify(jdict);
	publish(topic, msg);
}

// Simple function to manually reconnect a client.
bool MqttManager::try_reconnect(MqttClient *cli) {
	constexpr int N_ATTEMPT = 3;

	for (int i = 0; i < N_ATTEMPT && !cli->is_connected(); ++i) {
		try {
			cli->reconnect();
			spdlog::get("mqtt_logger")->info("mqtt client reconnect succeed!");
			return true;
		} catch (const mqtt::exception &) {
			this_thread::sleep_for(seconds(1));
			spdlog::get("mqtt_logger")->error("mqtt client {} trying to reconnect to {} ... ", cli->clientId(), cli->hostname());
		}
	}
	return false;
}

void MqttManager::finish() {
	if (!m_mqttClient)
		return;

	if (!m_mqttClient->isConnectedToHost())
		return;

	bool bret = m_mqttClient->unsubscribe("#")->wait_for(seconds(3));

	m_mqttClient->stop_consuming();

	// Disconnect
	cout << "\nDisconnecting..." << flush;
	bret = m_mqttClient->disconnect()->wait_for(seconds(3));
}

std::wstring MqttManager::stringToWstring(const std::string &t_str) {
#ifdef  _WIN32
//#ifdef _CODECVT_ /// deprecated in c++ 17
	//_STL_DISABLED_DEPRECATED_WARNINGS
	//setup converter
	typedef std::codecvt_utf8<wchar_t> convert_type;
	std::wstring_convert<convert_type, wchar_t> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	return converter.from_bytes(t_str);
	//_STL_RESTORE__DEPRECATED__WARNINGS
//#else // #ifdef CODECVT /// deprecated in c++ 17
//	 if (t_str.empty())
//    {
//        return L"";
//    }
//
//    const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, t_str.data(), (int)t_str.size(), nullptr, 0);
//    if (size_needed <= 0)
//    {
//        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
//    }
//
//    std::wstring result(size_needed, 0);
//    MultiByteToWideChar(CP_UTF8, 0, t_str.data(), (int)t_str.size(), result.data(), size_needed);
//    return result;
//#endif // #ifdef _CODECVT_ /// deprecated in c++ 17

#elif defined(__linux__)

	std::string s = t_str;

	std::string strLocale = setlocale(LC_ALL, "");
	const char* pSrc = s.c_str();
	unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
	wchar_t* szDest = new wchar_t[iDestSize];
	wmemset(szDest, 0, iDestSize);
	mbstowcs(szDest, pSrc, iDestSize);
	std::wstring ws = szDest;
	delete[]szDest;
	setlocale(LC_ALL, strLocale.c_str());

	return ws;

#endif
}

std::wstring MqttManager::stringToWstring(const char *utf8Bytes) {
#ifdef  _WIN32
//#ifdef _CODECVT_ /// deprecated in c++ 17
	//_STL_DISABLED_DEPRECATED_WARNINGS
	//setup converter
	using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
	std::wstring_convert<convert_type, typename std::wstring::value_type> converter;

	//use converter (.to_bytes: wstr->str, .from_bytes: str->wstr)
	return converter.from_bytes(utf8Bytes);
	//_STL_RESTORE__DEPRECATED__WARNINGS
//#else // #ifdef CODECVT /// deprecated in c++ 17
//	const auto size_needed = MultiByteToWideChar(CP_UTF8, 0, t_str.data(), (int)t_str.size(), nullptr, 0);
//    if (size_needed <= 0)
//    {
//        throw std::runtime_error("MultiByteToWideChar() failed: " + std::to_string(size_needed));
//    }
//
//    std::wstring result(size_needed, 0);
//    MultiByteToWideChar(CP_UTF8, 0, t_str.data(), (int)t_str.size(), result.data(), size_needed);
//    return result;
//#endif // #ifdef _CODECVT_ /// deprecated in c++ 17

#elif defined(__linux__)

	std::string s(utf8Bytes);

	std::string strLocale = setlocale(LC_ALL, "");
	const char* pSrc = s.c_str();
	unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
	wchar_t* szDest = new wchar_t[iDestSize];
	wmemset(szDest, 0, iDestSize);
	mbstowcs(szDest, pSrc, iDestSize);
	std::wstring ws = szDest;
	delete[]szDest;
	setlocale(LC_ALL, strLocale.c_str());

	return ws;

#endif
}

String MqttManager::getUtf8String(std::string in) {
#ifdef  _WIN32
	// todo: 需要判断字符中是否含有宽字符，判断后智能处理 case 1, case 4
	UAVarUtil util;
	UAVarUtil::CODING code = util.GetCoding((unsigned char*)in.c_str(), in.length());
	// case 1: 直接
	if (code == UAVarUtil::CODING::GBK) {
		return String(in.c_str());
	}

	// case 4:
	//setup converter when 中文
	else if (code == UAVarUtil::CODING::UTF8) {
		using convert_type = std::codecvt_utf8<typename std::wstring::value_type>;
		std::wstring_convert<convert_type, typename std::wstring::value_type> converter;
		std::wstring ws = converter.from_bytes(reinterpret_cast<const char *>(in.c_str()));
		return String(ws.c_str());
	}

#elif defined(__linux__)

	std::string s = in;

	std::string strLocale = setlocale(LC_ALL, "");
	const char* pSrc = s.c_str();
	unsigned int iDestSize = mbstowcs(NULL, pSrc, 0) + 1;
	wchar_t* szDest = new wchar_t[iDestSize];
	wmemset(szDest, 0, iDestSize);
	mbstowcs(szDest, pSrc, iDestSize);
	std::wstring ws = szDest;
	delete[]szDest;
	setlocale(LC_ALL, strLocale.c_str());

	return String(ws.c_str());

#endif

	return "";
}

int MqttManager::getIndexInVector(vector<std::string> vec, std::string v) {
	auto it = std::find(vec.begin(), vec.end(), v);
	if (it != vec.end()) {
		int idx = std::distance(vec.begin(), it);
		return idx;
	}

	return -1;
}
