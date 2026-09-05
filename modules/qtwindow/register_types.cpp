#include "register_types.h"
#include "core/object/class_db.h"
#include "qtwindow.h"

void initialize_qtwindow_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    ClassDB::register_class<QtWindowOverlay>();
}

void uninitialize_qtwindow_module(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}
