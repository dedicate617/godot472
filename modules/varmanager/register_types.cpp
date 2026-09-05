/* register_types.cpp */

#include "register_types.h"

#include "core/object/class_db.h"
#include "core/config/engine.h"
#include "varmanager.h"

#ifdef _WIN32
#include <Winsock2.h>
#endif /* _WIN32 */

static VarManager *varmanager = nullptr;

void initialize_varmanager_module(ModuleInitializationLevel p_level) {

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	
	#ifdef _WIN32
		//! Windows netword DLL init
		WORD version = MAKEWORD(2, 2);
		WSADATA data;

		if (WSAStartup(version, &data) != 0) {
		std::cerr << "WSAStartup() failure" << std::endl;
		}
    #endif /* _WIN32 */
	
	varmanager = memnew(VarManager);
	varmanager->init();
	ClassDB::register_class<VarManager>();
	Engine::get_singleton()->add_singleton(Engine::Singleton("g_Variable", VarManager::get_singleton()));
	//Engine::get_singleton()->add_singleton(Engine::Singleton("g_Alarm", VarManager::get_alarmsingleton()));
}

void uninitialize_varmanager_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	if(varmanager) {
		varmanager->finish();
		memdelete(varmanager);
	}
	// Nothing to do here in this example.
	#ifdef _WIN32
		WSACleanup();
	#endif /* _WIN32 */
}
