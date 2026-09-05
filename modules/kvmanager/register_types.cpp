/* register_types.cpp */

#include "register_types.h"

#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "kvmanager.h"

#ifdef _WIN32
//#include <Winsock2.h>
#endif /* _WIN32 */

static KVManager *kvmanager = nullptr;

void initialize_kvmanager_module(ModuleInitializationLevel p_level) {

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	#ifdef _WIN32
		//! Windows netword DLL init
		/*WORD version = MAKEWORD(2, 2);
		WSADATA data;

		if (WSAStartup(version, &data) != 0) {
		std::cerr << "WSAStartup() failure" << std::endl;
		}*/
    #endif /* _WIN32 */
	
	kvmanager = memnew(KVManager);
	kvmanager->init();
	ClassDB::register_class<KVManager>();
	Engine::get_singleton()->add_singleton(Engine::Singleton("g_KV", KVManager::get_singleton()));
}

void uninitialize_kvmanager_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	
	if(kvmanager) {
		kvmanager->finish();
		memdelete(kvmanager);
	}
	// Nothing to do here in this example.
	#ifdef _WIN32
		//WSACleanup();
	#endif /* _WIN32 */
}
