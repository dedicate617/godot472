/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"

#include "core/object/class_db.h"
#include "core/config/engine.h"

#include "webview.h"

void initialize_webview_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ClassDB::register_class<WebViewOverlay>();
	WebViewOverlay::init();
}

void uninitialize_webview_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	WebViewOverlay::finish();
}
