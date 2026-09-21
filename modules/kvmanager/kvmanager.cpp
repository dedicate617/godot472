/* kvmanager.cpp */

#include "kvmanager.h"
#include "../varmanager/util.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/object/message_queue.h"
#include "core/os/os.h"
//#include "core/reference.h"
#include "core/io/json.h"

#include <iostream>
#include <string>
#include <codecvt>
#ifdef _WIN32
	#include "MMKV/MMKV.h"
#elif defined(__linux__)
	#include "MMKV.h"
#endif
#if defined(WEB_ENABLED) || defined(JAVASCRIPT_ENABLED)

KVManager *KVManager::singleton = nullptr;

KVManager *KVManager::get_singleton() {
	return singleton;
}

KVManager::KVManager() {
	singleton = this;
	load_from_disk();
}

KVManager::~KVManager() {
	flush_to_disk();
	singleton = nullptr;
}

void KVManager::init(const String &p_rootDir) {
	// No-op for web: data is persisted via FileAccess/IDBFS
}

void KVManager::finish() {
	flush_to_disk();
}

void KVManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setValue", "key", "value", "id"), &KVManager::setValue, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getString", "key", "id"), &KVManager::getString, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getBool", "key", "id"), &KVManager::getBool, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getInt32", "key", "id"), &KVManager::getInt32, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getReal", "key", "id"), &KVManager::getReal, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("containsKey", "key", "id"), &KVManager::containsKey, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("removeValueForKey", "key", "id"), &KVManager::removeValueForKey, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("clearAll", "id"), &KVManager::clearAll, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("allKeys", "id"), &KVManager::allKeys, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("count", "id"), &KVManager::count, DEFVAL("DEFAULT"));
}

void KVManager::set(const String &key, const Variant &val) {
	memory_cache[key] = val;
	is_dirty = true;
}

Variant KVManager::get(const String &key) {
	if (memory_cache.has(key)) {
		return memory_cache[key];
	}
	return Variant();
}

bool KVManager::setValue(const String &p_key, const Variant p_value, const String &p_id) {
	set(p_key, p_value);
	return true;
}

bool KVManager::getBool(const String &p_key, const String &p_id) {
	return (bool)get(p_key);
}

int KVManager::getInt32(const String &p_key, const String &p_id) {
	return (int)get(p_key);
}

int KVManager::getUInt32(const String &p_key, const String &p_id) {
	return (int)(uint32_t)(int)get(p_key);
}

int KVManager::getInt64(const String &p_key, const String &p_id) {
	return (int64_t)(int)get(p_key);
}

int KVManager::getUInt64(const String &p_key, const String &p_id) {
	return (int64_t)(uint64_t)(int)get(p_key);
}

double KVManager::getReal(const String &p_key, const String &p_id) {
	return (double)get(p_key);
}

String KVManager::getString(const String &p_key, const String &p_id) {
	return (String)get(p_key);
}

Vector<String> KVManager::getArray(const String &p_key, const String &p_id) {
	Variant v = get(p_key);
	if (v.get_type() == Variant::ARRAY) {
		Array arr = v;
		Vector<String> result;
		for (int i = 0; i < arr.size(); i++) {
			result.push_back(arr[i]);
		}
		return result;
	}
	return Vector<String>();
}

Dictionary KVManager::getDict(const String &p_key, const String &p_id) {
	Variant v = get(p_key);
	if (v.get_type() == Variant::DICTIONARY) {
		return v;
	}
	return Dictionary();
}

bool KVManager::containsKey(const String &p_key, const String &p_id) {
	return memory_cache.has(p_key);
}

int KVManager::count(const String &p_id) {
	return memory_cache.size();
}

Vector<String> KVManager::allKeys(const String &p_id) {
	Vector<String> keys;
	for (const KeyValue<String, Variant> &E : memory_cache) {
		keys.push_back(E.key);
	}
	return keys;
}

void KVManager::removeValueForKey(const String &p_key, const String &p_id) {
	memory_cache.erase(p_key);
	is_dirty = true;
}

void KVManager::clearAll(const String &p_id) {
	memory_cache.clear();
	is_dirty = true;
}

void KVManager::flush_to_disk() {
	if (!is_dirty) return;
	Ref<FileAccess> f = FileAccess::open(save_path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_32(memory_cache.size());
		for (const KeyValue<String, Variant> &E : memory_cache) {
			f->store_pascal_string(E.key);
			f->store_var(E.value);
		}
		f->close(); // Triggers Emscripten IDBFS sync
		is_dirty = false;
		last_flush_time = OS::get_singleton()->get_ticks_msec();
	}
}

void KVManager::load_from_disk() {
	Ref<FileAccess> f = FileAccess::open(save_path, FileAccess::READ);
	if (f.is_valid()) {
		uint32_t size = f->get_32();
		for (uint32_t i = 0; i < size; i++) {
			String k = f->get_pascal_string();
			Variant v = f->get_var();
			memory_cache[k] = v;
		}
		f->close();
	}
}

void KVManager::process(float delta) {
	if (is_dirty && OS::get_singleton()->get_ticks_msec() - last_flush_time > 500) {
		flush_to_disk();
	}
}

#else


#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

//#include "spdlog/async.h"
//#include "spdlog/cfg/env.h"
//#include "spdlog/sinks/rotating_file_sink.h"
//#include "spdlog/sinks/stdout_color_sinks.h"
//#include "spdlog/spdlog.h"
 
static void LogHandler(MMKVLogLevel level, const char *file, int line, const char *function, const std::string &message) {

	auto desc = [level] {
		switch (level) {
		case MMKVLogDebug:
			return "D";
		case MMKVLogInfo:
			return "I";
		case MMKVLogWarning:
			return "W";
		case MMKVLogError:
			return "E";
		default:
			return "N";
		}
	}();
	printf("redirecting-[%s] <%s:%d::%s> %s\n", desc, file, line, function, message.c_str());
}

static MMKVRecoverStrategic ErrorHandler(const std::string &mmapID, MMKVErrorType errorType)
{
	printf("redirecting-[ErrorHandler] <%s:%d> \n", mmapID.c_str(), errorType);
	return MMKVRecoverStrategic::OnErrorDiscard;
}



//std::wstring KVManager::getAppDataRoaming(const wstring &company, const wstring &appName) {
//#ifdef __linux__ || __unix__
//	
//#elif _WIN32
//	
//	wchar_t roaming[MAX_PATH] = { 0 };
//	auto size = GetEnvironmentVariable(L"appdata", roaming, MAX_PATH);
//	if (size >= MAX_PATH || size == 0) {
//		cout << "fail to get %appdata%: " << GetLastError() << endl;
//		return L"";
//	}
//	else {
//		wstring result(roaming, size);
//		result += L"\\" + company;
//		result += L"\\" + appName;
//		return result;
//	}
//#endif
//	return L"";
//}

static void ContentChangeHandler(const std::string &mmapID)
{
	String s = String(mmapID.c_str());
	KVManager::get_singleton()->emit_signal("ContentChange", s);
}

KVManager *KVManager::singleton = NULL;

KVManager *KVManager::get_singleton() {
	return singleton;
}

void KVManager::_bind_methods() {
	ADD_SIGNAL(MethodInfo("ContentChange", PropertyInfo(Variant::STRING, "id")));

	ClassDB::bind_method(D_METHOD("init", "rootDir"), &KVManager::init, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("setLogLevel", "lv"), &KVManager::setLogLevel);
	ClassDB::bind_method(D_METHOD("kvWithID", "mode", "id", "cryptKey", "rootPath"), &KVManager::kvWithID, DEFVAL(KVManager::KVMode::KV_MULTI_PROCESS), DEFVAL("DEFAULT"), DEFVAL(""), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("setValue", "key", "value", "id"), &KVManager::setValue, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getBool", "key", "id"), &KVManager::getBool, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getInt32", "key", "id"), &KVManager::getInt32, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getUInt32", "key", "id"), &KVManager::getUInt32, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getInt64", "key", "id"), &KVManager::getInt64, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getUInt64", "key", "id"), &KVManager::getUInt64, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getReal", "key", "id"), &KVManager::getReal, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getString", "key", "id"), &KVManager::getString, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getArray", "key", "id"), &KVManager::getArray, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("getDict", "key", "id"), &KVManager::getDict, DEFVAL("DEFAULT"));

	ClassDB::bind_method(D_METHOD("getValueSize", "key", "actualSize", "id"), &KVManager::getValueSize, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("containsKey", "key", "id"), &KVManager::containsKey, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("count", "id"), &KVManager::count, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("totalSize", "id"), &KVManager::totalSize, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("actualSize", "id"), &KVManager::actualSize, DEFVAL("DEFAULT"));

	ClassDB::bind_method(D_METHOD("allKeys", "id"), &KVManager::allKeys, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("removeValuesForKeys", "arrKeys", "id"), &KVManager::removeValuesForKeys, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("removeValueForKey", "key", "id"), &KVManager::removeValueForKey, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("clearAll", "id"), &KVManager::clearAll, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("sync", "flag", "id"), &KVManager::sync, DEFVAL(KVManager::KVSyncFlag::KV_SYNC), DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("lock", "id"), &KVManager::lock, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("unlock", "id"), &KVManager::unlock, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("try_lock", "id"), &KVManager::try_lock, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("checkContentChanged", "id"), &KVManager::checkContentChanged, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("clearMemoryCache", "id"), &KVManager::clearMemoryCache, DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("backupOneToDirectory", "dstDir", "srcDir", "id"), &KVManager::backupOneToDirectory, DEFVAL(""), DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("backupAllToDirectory", "dstDir", "srcDir"), &KVManager::backupAllToDirectory, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("restoreOneFromDirectory", "srcDir", "dstDir", "id"), &KVManager::restoreOneFromDirectory, DEFVAL(""), DEFVAL("DEFAULT"));
	ClassDB::bind_method(D_METHOD("restoreAllFromDirectory", "srcDir", "dstDir"), &KVManager::restoreAllFromDirectory, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("close", "id"), &KVManager::close, DEFVAL(""));

	BIND_ENUM_CONSTANT(KVLOGDEBUG);
	BIND_ENUM_CONSTANT(KVLOGINFO);
	BIND_ENUM_CONSTANT(KVLOGWARNING);
	BIND_ENUM_CONSTANT(KVLOGERROR);
	BIND_ENUM_CONSTANT(KVLOGNONE);

	BIND_ENUM_CONSTANT(KV_SINGLE_PROCESS);
	BIND_ENUM_CONSTANT(KV_MULTI_PROCESS);

	BIND_ENUM_CONSTANT(KV_ASYNC);
	BIND_ENUM_CONSTANT(KV_SYNC);
}

void KVManager::connect_signals() {
	/*connect("varChanged", callable_mp(this, "variableChanged"));
	connect("varsChanged", callable_mp(this, "variablesChanged"));
	connect("eventTriggered", callable_mp(this, "handleEvent"));*/
}

KVManager::KVManager() {
	singleton = this;
}

KVManager::~KVManager() {
	//singleton = NULL;
	MMKV::onExit();
}

void KVManager::init(const String &p_rootDir/* = ""*/)
{
	/*if (p_rootDir == "")
	{
		std::wstring rootDir = getAppDataRoaming(L"Mindteco", L"MDKV");
		MMKV::defaultMMKV();
	}
	else*/
	{
#ifdef _WIN32
		std::wstring std_rootDir = stringToWstring(p_rootDir.utf8().get_data());
		if (std_rootDir.empty()) {
			wchar_t tmp[MAX_PATH] = { 0 };
			GetTempPathW(MAX_PATH, tmp);
			std_rootDir = std::wstring(tmp) + L"mmkv";
		}
		MMKV::initializeMMKV(std_rootDir);
#elif defined(__linux__)
		std::string std_rootDir = std::string(p_rootDir.utf8().get_data());
		if (std_rootDir == "")
			std_rootDir = "/tmp/mmkv";
		MMKV::initializeMMKV(std_rootDir);
#endif
	}
	MMKV::registerLogHandler(LogHandler);
	MMKV::registerContentChangeHandler(ContentChangeHandler);
	MMKV::registerErrorHandler(ErrorHandler);
}

void KVManager::setLogLevel(KVLogLevel p_lv)
{
	MMKV::setLogLevel((MMKVLogLevel)p_lv);
}

void KVManager::kvWithID(KVMode p_mode/* = KV_MULTI_PROCESS*/, const String &p_id/* = "DEFAULT"*/, const String &p_cryptKey/* = ""*/, const String& p_rootPath/* = ""*/)
{
	std::string* std_cryptKey = nullptr;
	if (p_cryptKey != "") {
		std::string s = std::string(p_cryptKey.utf8().get_data());
		std_cryptKey = &s;
	}

	MMKV* mmkv = nullptr;
	if (p_id == "DEFAULT")
	{
		mmkv = MMKV::defaultMMKV((MMKVMode)p_mode, std_cryptKey);
	}
	else
	{
		std::string std_id = std::string(p_id.utf8().get_data());
#ifdef _WIN32
		std::wstring* std_rootPath = nullptr;
		std::wstring param_wpath;
		if (p_rootPath != "")
		{
			String _rootPath = p_rootPath;
			_rootPath = _rootPath.replace("/", "\\");
			param_wpath = stringToWstring(_rootPath.utf8().get_data());
			std_rootPath = &param_wpath;
		}

		mmkv = MMKV::mmkvWithID(std_id, (MMKVMode)p_mode, std_cryptKey, std_rootPath);
#elif defined(__linux__)
		std::string* std_rootPath = nullptr;
		std::string s;
		if (p_rootPath != "") {
			s = std::string(p_rootPath.utf8().get_data());
			std_rootPath = &s;
		}

		mmkv = MMKV::mmkvWithID(std_id, (MMKVMode)p_mode, std_cryptKey, std_rootPath);
#endif
	}

	if (mmkv != nullptr)
	{
		uint32_t id_hash = p_id.hash();
		if (m_hashMMKV_map.find(id_hash) == m_hashMMKV_map.end())
		{
			// not exist
			m_hashMMKV_map[id_hash] = mmkv;
		}
	}
}

void KVManager::kvWithID1(int p_mode /* = 1*/, const String &p_id /* = "DEFAULT"*/, const String &p_cryptKey /* = ""*/, const String &p_rootPath /* = ""*/) {
	kvWithID((KVMode)p_mode, p_id, p_cryptKey, p_rootPath);
}
bool KVManager::setValue(const String& p_key, const Variant p_value, const String& p_id)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		int typeIndex = p_value.get_type();
		//std::cout << "typeIndex: " << typeIndex << std::endl;
		if (typeIndex == Variant::NIL) {
			return false;
		}
		
		
		//OS::get_singleton()->print("KVManager::setValue p_value typeIndex=%d\n", typeIndex);
		if (typeIndex == Variant::BOOL) {
			return mmkv->set((bool)p_value, std_key);
		} else if (typeIndex == Variant::INT) {
			return mmkv->set((int64_t)p_value, std_key);
		} else if (typeIndex == Variant::FLOAT) {
			return mmkv->set((double)p_value, std_key);
		} else if (typeIndex == Variant::STRING) {
			return mmkv->set(std::string(((String)p_value).utf8().get_data()), std_key);
		} else if (typeIndex == Variant::ARRAY) {
			std::vector<std::string> std_vs;
			Vector<String> p_arr = (Vector<String>)p_value;
			for (int i = 0; i < p_arr.size(); ++i) {
				std::string std_v = std::string(p_arr[i].utf8().get_data());
				std_vs.push_back(std_v);
			}
			return mmkv->set(std_vs, std_key);
		} else if (typeIndex == Variant::DICTIONARY) {
			String p_v_str = JSON::stringify(p_value);
			return mmkv->set(std::string(p_v_str.utf8().get_data()), std_key);
		}
	}
	return false;
}

bool KVManager::getBool(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getBool(std_key);
	}
	return false;
}

int KVManager::getInt32(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getInt32(std_key);
	}
	return 0;
}

int KVManager::getUInt32(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getUInt32(std_key);
	}
	return 0;
}

int KVManager::getInt64(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getInt64(std_key);
	}
	return 0;
}

int KVManager::getUInt64(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getUInt64(std_key);
	}
	return 0;
}

double KVManager::getReal(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->getDouble(std_key);
	}
	return 0;
}

String KVManager::getString(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		std::string std_value_ret;
		if (mmkv->getString(std_key, std_value_ret))
		{
			return getUtf8String(std_value_ret);
		}
	}
	return "";
}

Vector<String> KVManager::getArray(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	Vector<String> vect_ret;
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		std::vector<std::string> std_values_ret;
		if (mmkv->getVector(std_key, std_values_ret)) {
			for (int i = 0; i < std_values_ret.size(); ++i) {
				std::string std_value_ret = std_values_ret[i];
				String s = getUtf8String(std_value_ret);
				vect_ret.push_back(s);
			}
			return vect_ret;
		}
	}
	return vect_ret;
}

Dictionary KVManager::getDict(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		std::string std_value_ret;
		if (mmkv->getString(std_key, std_value_ret))
		{
			String s = getUtf8String(std_value_ret);
			//OS::get_singleton()->print("dict string=%s\n", s.utf8().get_data());
			Variant dict_ret;
			//String r_err_str;
			//int r_err_line;
			//if (Error::OK == JSON::parse(s, dict_ret, r_err_str, r_err_line))
			//{
			//	int typeIndex = dict_ret.get_type();
			//	if (typeIndex == Variant::DICTIONARY)
			//	{
			//		return (Dictionary)dict_ret;
			//	}
			//}
			dict_ret = JSON::parse_string(s);
			int typeIndex = dict_ret.get_type();
			if (typeIndex == Variant::DICTIONARY)
			{
				return (Dictionary)dict_ret;
			}
			OS::get_singleton()->print("dict parse error, r_err_str=%s\n", s.utf8().get_data());
		}
	}
	return Dictionary();
}

int KVManager::getValueSize(const String& p_key, bool p_actualSize/* = true*/, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		std::string std_value_ret;
		return mmkv->getValueSize(std_key, p_actualSize);
	}
	return 0;
}
bool KVManager::containsKey(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		return mmkv->containsKey(std_key);
	}
	return false;
}
int KVManager::count(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		return mmkv->count();
	}
	return 0;
}
int KVManager::totalSize(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		return mmkv->totalSize();
	}
	return 0;
}
int KVManager::actualSize(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		return mmkv->actualSize();
	}
	return 0;
}

Vector<String> KVManager::allKeys(const String& p_id/* = "DEFAULT"*/)
{
	Vector<String> vect_ret;
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::vector<std::string> std_values_ret = mmkv->allKeys();
		for (int i = 0; i < std_values_ret.size(); ++i) {
			std::string std_value_ret = std_values_ret[i];
			//vect_ret.push_back(String(std_value_ret.c_str()));
			vect_ret.push_back(getUtf8String(std_value_ret));
		}
	}
	return vect_ret;
}

void KVManager::removeValuesForKeys(Vector<String> p_arrKeys, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::vector<std::string> std_arrKeys;
		for (int i = 0; i < p_arrKeys.size(); ++i) {
			std::string std_key_ret = std::string(p_arrKeys[i].utf8().get_data());
			std_arrKeys.push_back(std_key_ret);
		}
		mmkv->removeValuesForKeys(std_arrKeys);
	}
}

void KVManager::removeValueForKey(const String& p_key, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		std::string std_key = std::string(p_key.utf8().get_data());
		mmkv->removeValueForKey(std_key);
	}
}

void KVManager::clearAll(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->clearAll();
	}
}
void KVManager::trim(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->trim();
	}
}
void KVManager::close(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->close();
	}
}

// call this method if you are facing memory-warning
// any subsequent call to the instance will load all key-values from file again
void KVManager::clearMemoryCache(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->clearMemoryCache();
	}
}


// you don't need to call this, really, I mean it
// unless you worry about running out of battery
void KVManager::sync(KVSyncFlag p_flag/* = KV_SYNC*/, const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->sync((SyncFlag)p_flag);
	}
}

// get exclusive access
void KVManager::lock(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->lock();
	}
}
void KVManager::unlock(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		mmkv->unlock();
	}
}
bool KVManager::try_lock(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		return mmkv->try_lock();
	}
	return false;
}

String KVManager::getRootDir()
{
#ifdef _WIN32
	std::wstring std_rootDir = MMKV::getRootDir();
	return String(std_rootDir.c_str());
#elif defined(__linux__)
	std::string std_rootDir = MMKV::getRootDir();
	return String(std_rootDir.c_str());
#endif
}

// backup one MMKV instance from srcDir to dstDir
// if srcDir is null, then backup from the root dir of MMKV
bool KVManager::backupOneToDirectory(const String &p_dstDir, const String &p_srcDir/* = ""*/, const String& p_id/* = "DEFAULT"*/)
{
	std::string std_id = std::string(p_id.utf8().get_data());
#ifdef _WIN32
	std::wstring std_dstDir = stringToWstring(p_dstDir.utf8().get_data());
	std::wstring* std_srcDir = nullptr;
	std::wstring ws_dstDir;
	if (p_srcDir != "")
	{
		ws_dstDir = stringToWstring(p_srcDir.utf8().get_data());
		std_srcDir = &ws_dstDir;
	}
	return MMKV::backupOneToDirectory(std_id, std_dstDir, std_srcDir);
#elif defined(__linux__)
	std::string std_dstDir = std::string(p_dstDir.utf8().get_data());
	std::string* std_srcDir = nullptr;
	std::string s;
	if (p_srcDir != "")
	{
		s = std::string(p_srcDir.utf8().get_data());
		std_srcDir = &s;
	}
	return MMKV::backupOneToDirectory(std_id, std_dstDir, std_srcDir);
#endif
}

// restore one MMKV instance from srcDir to dstDir
// if dstDir is null, then restore to the root dir of MMKV
bool KVManager::restoreOneFromDirectory(const String &p_srcDir, const String &p_dstDir/* = ""*/, const String& p_id/* = "DEFAULT"*/)
{
	std::string std_id = std::string(p_id.utf8().get_data());
#ifdef _WIN32
	std::wstring std_srcDir = stringToWstring(p_srcDir.utf8().get_data());
	std::wstring* std_dstDir = nullptr;
	std::wstring ws_dstDir;
	if (p_dstDir != "")
	{
		ws_dstDir = stringToWstring(p_dstDir.utf8().get_data());
		std_dstDir = &ws_dstDir;
	}
	return MMKV::restoreOneFromDirectory(std_id, std_srcDir, std_dstDir);
#elif defined(__linux__)
	std::string std_srcDir = std::string(p_srcDir.utf8().get_data());
	std::string* std_dstDir = nullptr;
	std::string s;
	if (p_dstDir != "")
	{
		s = std::string(p_dstDir.utf8().get_data());
		std_dstDir = &s;
	}
	return MMKV::restoreOneFromDirectory(std_id, std_srcDir, std_dstDir);
#endif
}

// backup all MMKV instance from srcDir to dstDir
// if srcDir is null, then backup from the root dir of MMKV
// return count of MMKV successfully backuped
int KVManager::backupAllToDirectory(const String &p_dstDir, const String &p_srcDir/* = ""*/)
{
#ifdef _WIN32
	std::wstring std_dstDir = stringToWstring(p_dstDir.utf8().get_data());
	std::wstring* std_srcDir = nullptr;
	std::wstring ws_dstDir;
	if (p_srcDir != "")
	{
		ws_dstDir = stringToWstring(p_srcDir.utf8().get_data());
		std_srcDir = &ws_dstDir;
	}
	return MMKV::backupAllToDirectory(std_dstDir, std_srcDir);
#elif defined(__linux__)
	std::string std_dstDir = std::string(p_dstDir.utf8().get_data());
	std::string* std_srcDir = nullptr;
	std::string s;
	if (p_srcDir != "")
	{
		s = std::string(p_srcDir.utf8().get_data());
		std_srcDir = &s;
	}
	return MMKV::backupAllToDirectory(std_dstDir, std_srcDir);
#endif
}

// restore all MMKV instance from srcDir to dstDir
// if dstDir is null, then restore to the root dir of MMKV
// return count of MMKV successfully restored
int KVManager::restoreAllFromDirectory(const String &p_srcDir, const String &p_dstDir/* = ""*/)
{
#ifdef _WIN32
	std::wstring std_srcDir = stringToWstring(p_srcDir.utf8().get_data());
	std::wstring* std_dstDir = nullptr;
	std::wstring ws_dstDir;
	if (p_dstDir != "")
	{
		ws_dstDir = stringToWstring(p_dstDir.utf8().get_data());
		std_dstDir = &ws_dstDir;
	}
	return MMKV::restoreAllFromDirectory(std_srcDir, std_dstDir);
#elif defined(__linux__)
	std::string std_srcDir = std::string(p_srcDir.utf8().get_data());
	std::string* std_dstDir = nullptr;
	std::string s;
	if (p_dstDir != "")
	{
		s = std::string(p_dstDir.utf8().get_data());
		std_dstDir = &s;
	}
	return MMKV::restoreAllFromDirectory(std_srcDir, std_dstDir);
#endif
}

// check if content been changed by other process
void KVManager::checkContentChanged(const String& p_id/* = "DEFAULT"*/)
{
	uint32_t id_hash = p_id.hash();
	if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end())
	{
		// pid existed
		auto mmkv = m_hashMMKV_map[id_hash];
		return mmkv->checkContentChanged();
	}
}

//// called when content is changed by other process
//// doesn't guarantee real-time notification
//void KVManager::registerContentChangeHandler(mmkv::ContentChangeHandler handler);
//void KVManager::unRegisterContentChangeHandler();
//
//// by default MMKV will discard all datas on failure
//// return `OnErrorRecover` to recover any data from file
//void KVManager::registerErrorHandler(mmkv::ErrorHandler handler);
//void KVManager::unRegisterErrorHandler();

//// by default MMKV will print log to the console
//// implement this method to redirect MMKV's log
//void KVManager::registerLogHandler(mmkv::LogHandler handler);
//void KVManager::unRegisterLogHandler();

// detect if the MMKV file is valid or not
// Note: Don't use this to check the existence of the instance, the return value is undefined if the file was never created.
bool KVManager::isFileValid(const String& p_relatePath/* = ""*/, const String& p_id/* = "DEFAULT"*/)
{
	std::string std_id = std::string(p_id.utf8().get_data());
#ifdef _WIN32
	std::wstring* std_relatePath = nullptr;
	std::wstring tmp;
	if (p_relatePath != "")
	{
		tmp = stringToWstring(p_relatePath.utf8().get_data());
		std_relatePath = &tmp;
	}
	return MMKV::isFileValid(std_id, std_relatePath);
#elif defined(__linux__)
	std::string* std_relatePath = nullptr;
	std::string tmp;
	if (p_relatePath != "")
	{
		tmp = std::string(p_relatePath.utf8().get_data());
		std_relatePath = &tmp;
	}
	return MMKV::isFileValid(std_id, std_relatePath);
#endif
}

void KVManager::finish()
{
	MMKV::unRegisterLogHandler();
	MMKV::unRegisterContentChangeHandler();
	MMKV::unRegisterErrorHandler();
}

void KVManager::onVarRefreshed(const String &topic, const String &msg, const int qos) {
	// topic: DataChange/#
	// msg:
	/*
    {
        "H": {
            "V": "1.0"
        },
        "GW": "AnHui_YC_EMT_test001",
        "VS": [{
            "ID": "71E5C58A5554A7001DCA16AFD6B9689D",
			"N": "virtual/dev1/u1/temp1",
            "Q": "192",
            "T": "2017-12-08 23:28:59.389",
            "V": "2",
            "TP": "2"
            }
        ]
    }
	*/
	// todo: 根据项目路径创建对应的实时数据kv库
	//kvWithID(KVMode::KV_MULTI_PROCESS, "__varmq__", "", /*const String &p_rootPath*/ "path2prj");
	if (topic.begins_with("DataChange/")) {
		Dictionary msg_dict = JSON::parse_string(msg);
		Array vs = msg_dict["VS"];
		
		uint32_t id_hash = String("__varmq__").hash();
		if (m_hashMMKV_map.find(id_hash) != m_hashMMKV_map.end()) {
			// pid existed
			auto mmkv = m_hashMMKV_map[id_hash];

			//OS::get_singleton()->print("KVManager::onVarRefreshed topic=%s msg=%s qos=%d\n", topic.utf8().get_data(), msg.utf8().get_data(), qos);
	
			/*
			if (typeIndex == Variant::BOOL) {
				return mmkv->set((bool)p_value, std_key);
			} else if (typeIndex == Variant::INT) {
				return mmkv->set((int64_t)p_value, std_key);
			} else if (typeIndex == Variant::FLOAT) {
				return mmkv->set((double)p_value, std_key);
			} else if (typeIndex == Variant::STRING) {
				return mmkv->set(std::string(((String)p_value).utf8().get_data()), std_key);
			} else if (typeIndex == Variant::ARRAY) {
				std::vector<std::string> std_vs;
				Vector<String> p_arr = (Vector<String>)p_value;
				for (int i = 0; i < p_arr.size(); ++i) {
					std::string std_v = std::string(p_arr[i].utf8().get_data());
					std_vs.push_back(std_v);
				}
				return mmkv->set(std_vs, std_key);
			} else if (typeIndex == Variant::DICTIONARY) {
				String p_v_str = JSON::stringify(p_value);
				return mmkv->set(std::string(p_v_str.utf8().get_data()), std_key);
			}
		}
			*/
			for (int i = 0; i < vs.size(); i++) {
				Dictionary v = vs[i];
				String name = v["N"];
				std::string std_key = std::string(name.utf8().get_data());
#ifdef _WIN32
				int tp = signed int(v["TP"]);
#elif defined(__linux__)
				int tp = (signed int)(v["TP"]);
#endif
				String value = String(v["V"]);
				bool ret = false;
				switch (tp) {
					case 1: //QVariant::Type::Bool
						ret = mmkv->set((bool)(value.to_int()), std_key);
						break;
					case 2: //QVariant::Type::Int
					case 3: //QVariant::Type::UInt
					case 4: //QVariant::Type::LongLong
					case 5: //QVariant::Type::ULongLong
						ret = mmkv->set((int64_t)(value.to_int()), std_key);
						break;
					case 6: //QVariant::Type::Double
					case 38: //QVariant::float
					{
						bool b = true;
						double d = value.to_float();
						//qDebug() << "setMMKVValue(" << name << "," << value << ")" << d;
						if (b)
							ret = mmkv->set(d, std_key);
					} break;
					case 16: //QVariant::Type::DateTime
						// todo: write datetime to kv from mqtt
						//ret = mmkv->set((int64_t)(value.toDateTime().toSecsSinceEpoch()), std_key);
					{
						bool b = true;
						int64_t i = value.to_int();
						if (b)
							ret = mmkv->set(i, std_key);
					}
						break;
					case 10: //QVariant::Type::String
					{
						std::string s = std::string(value.utf8().get_data());
						ret = mmkv->set(s, std_key);
					}
						break;
					default:
						OS::get_singleton()->print("KVManager::onVarRefreshed to kv convert error: not a valid type=%d\n", tp);
						break;
				}
				if (!ret) {
					OS::get_singleton()->print("KVManager::onVarRefreshed to kv convert error: topic=%s msg=%s qos=%d\n", topic.utf8().get_data(), msg.utf8().get_data(), qos);
				}
			}
		}
	}
}

std::wstring KVManager::stringToWstring(const std::string& t_str)
{
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


std::wstring KVManager::stringToWstring(const char* utf8Bytes)
{
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

String KVManager::getUtf8String(std::string in)
{
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

#endif
