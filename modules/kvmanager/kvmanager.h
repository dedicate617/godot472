/* kvmanager.h */

#ifndef KVMANGER_H
#define KVMANGER_H

#pragma once

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#include <Shlwapi.h>
#ifdef _DEBUG
//#pragma comment(lib, "libcmtd.lib")
#pragma comment(lib, "msvcrtd.lib")
//#pragma comment(lib, "vcruntimed.lib")
//#pragma comment(lib, "ucrtd.lib")
#endif
#endif /* _WIN32 */

#include <map>
#include <vector>

#include "core/variant/variant.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
#include "kvTransferInterface.h"

//#include "ICallback.h"

// There is a parameter limit of 5 in C++ modules for things such as subclasses.
// This can be raised to 13 by including the header file core/method_bind_ext.gen.inc.
//#include "core/object/method_bind_ext.gen.inc"
using namespace std;

class MMKV;
class KVManager : public KVTransferInterface /*, ICallback*/ {
	GDCLASS(KVManager, KVTransferInterface);
	
	//static void thread_func(void* p_date);

protected:
	static void _bind_methods();

public:
	KVManager();
	virtual ~KVManager();

	enum KVLogLevel {
		KVLOGDEBUG = 0, // not available for release/product build
		KVLOGINFO = 1,  // default level
		KVLOGWARNING,
		KVLOGERROR,
		KVLOGNONE, // special level used to disable all log messages
	};

	enum KVMode {
		KV_SINGLE_PROCESS = 0,
		KV_MULTI_PROCESS = 2,
	};

	enum KVSyncFlag {
		KV_ASYNC = false,
		KV_SYNC = true,
	};

	void connect_signals();

	virtual void init(const String &p_rootDir = "") override;
	void setLogLevel(KVLogLevel p_lv);
	void kvWithID(KVMode p_mode = KV_MULTI_PROCESS, const String &p_id = "DEFAULT", const String &p_cryptKey = "", const String& p_rootPath = "");

	bool setValue(const String& p_key, const Variant p_value, const String& p_id = "DEFAULT");

	bool getBool(const String& p_key, const String& p_id = "DEFAULT");
	int getInt32(const String& p_key, const String& p_id = "DEFAULT");
	int getUInt32(const String& p_key, const String& p_id = "DEFAULT");
	int getInt64(const String& p_key, const String& p_id = "DEFAULT");
	int getUInt64(const String& p_key, const String& p_id = "DEFAULT");
	double getReal(const String& p_key, const String& p_id = "DEFAULT");
	String getString(const String& p_key, const String& p_id = "DEFAULT");
	Vector<String> getArray(const String& p_key, const String& p_id = "DEFAULT");
	Dictionary getDict(const String& p_key, const String& p_id = "DEFAULT");

	int getValueSize(const String& p_key, bool p_actualSize = true, const String& p_id = "DEFAULT");
	bool containsKey(const String& p_key, const String& p_id = "DEFAULT");
	int count(const String& p_id = "DEFAULT");
	int totalSize(const String& p_id = "DEFAULT");
	int actualSize(const String& p_id = "DEFAULT");

	Vector<String> allKeys(const String& p_id = "DEFAULT");
	void removeValuesForKeys(Vector<String> p_arrKeys, const String& p_id = "DEFAULT");
	void removeValueForKey(const String& p_key, const String& p_id = "DEFAULT");
	void clearAll(const String& p_id = "DEFAULT");
	// MMKV's size won't reduce after deleting key-values
	// call this method after lots of deleting if you care about disk usage
	// note that `clearAll` has the similar effect of `trim`
	void trim(const String& p_id = "DEFAULT");
	// call this method if the instance is no longer needed in the near future
	// any subsequent call to the instance is undefined behavior
	void close(const String& p_id = "DEFAULT");
	// call this method if you are facing memory-warning
	// any subsequent call to the instance will load all key-values from file again
	void clearMemoryCache(const String& p_id = "DEFAULT");

	//// you don't need to call this, really, I mean it
	//// unless you worry about running out of battery
	void sync(KVSyncFlag p_flag = KV_SYNC, const String& p_id = "DEFAULT");

	// get exclusive access
	void lock(const String& p_id = "DEFAULT");
	void unlock(const String& p_id = "DEFAULT");
	bool try_lock(const String& p_id = "DEFAULT");

	String getRootDir();

	// backup one MMKV instance from srcDir to dstDir
	// if srcDir is null, then backup from the root dir of MMKV
	bool backupOneToDirectory(const String &p_dstDir, const String &p_srcDir = "", const String& p_id = "DEFAULT");

	// restore one MMKV instance from srcDir to dstDir
	// if dstDir is null, then restore to the root dir of MMKV
	bool restoreOneFromDirectory(const String &p_srcDir, const String &p_dstDir = "", const String& p_id = "DEFAULT");

	// backup all MMKV instance from srcDir to dstDir
	// if srcDir is null, then backup from the root dir of MMKV
	// return count of MMKV successfully backuped
	int backupAllToDirectory(const String &p_dstDir, const String &p_srcDir = "");

	// restore all MMKV instance from srcDir to dstDir
	// if dstDir is null, then restore to the root dir of MMKV
	// return count of MMKV successfully restored
	int restoreAllFromDirectory(const String &p_srcDir, const String &p_dstDir = "");

	// check if content been changed by other process
	void checkContentChanged(const String& p_id = "DEFAULT");

	//// called when content is changed by other process
	//// doesn't guarantee real-time notification
	//void registerContentChangeHandler(mmkv::ContentChangeHandler handler);
	//void unRegisterContentChangeHandler();

	//// by default MMKV will discard all datas on failure
	//// return `OnErrorRecover` to recover any data from file
	//void registerErrorHandler(mmkv::ErrorHandler handler);
	//void unRegisterErrorHandler();

	//// by default MMKV will print log to the console
	//// implement this method to redirect MMKV's log
	//void registerLogHandler(mmkv::LogHandler handler);
	//void unRegisterLogHandler();

	// detect if the MMKV file is valid or not
	// Note: Don't use this to check the existence of the instance, the return value is undefined if the file was never created.
	bool isFileValid(const String& p_relatePath = "", const String& p_id = "DEFAULT");


	static KVManager *get_singleton();
	void finish();


	virtual void kvWithID1(int p_mode = 1, const String &p_id = "DEFAULT", const String &p_cryptKey = "", const String &p_rootPath = "") override;
	virtual void onVarRefreshed(const String &topic, const String &msg, const int qos) override;

private:

	static KVManager *singleton;
	// <mmid_hashid, mmkv object pointer>
	std::map<uint32_t, MMKV*> m_hashMMKV_map;

	//wstring getAppDataRoaming(const wstring &company, const wstring &appName);

	std::wstring stringToWstring(const std::string& t_str);
	std::wstring stringToWstring(const char* utf8Bytes);
	String getUtf8String(std::string in);
};

VARIANT_ENUM_CAST(KVManager::KVLogLevel);
VARIANT_ENUM_CAST(KVManager::KVMode);
VARIANT_ENUM_CAST(KVManager::KVSyncFlag);

#endif // VARMANGER_H
