#include "qtwindow_keymapper.h"

// BASE_TABLE maps Godot Key enum values to Qt::Key values.
// Sorted ascending by .godot for binary search.
// Godot special keys start at SPECIAL = 1 << 22 = 4194304.
// Printable ASCII keys (32-126) are handled by passthrough in godot_to_qt().
const QtKeyMapper::Entry QtKeyMapper::BASE_TABLE[] = {
    // Navigation / editing keys
    { 4194305, 0x01000000 }, // KEY_ESCAPE      -> Qt::Key_Escape
    { 4194306, 0x01000001 }, // KEY_TAB         -> Qt::Key_Tab
    { 4194307, 0x01000002 }, // KEY_BACKTAB     -> Qt::Key_Backtab
    { 4194308, 0x01000003 }, // KEY_BACKSPACE   -> Qt::Key_Backspace
    { 4194309, 0x01000004 }, // KEY_ENTER       -> Qt::Key_Return
    { 4194310, 0x01000005 }, // KEY_KP_ENTER    -> Qt::Key_Enter
    { 4194311, 0x01000006 }, // KEY_INSERT      -> Qt::Key_Insert
    { 4194312, 0x01000007 }, // KEY_DELETE      -> Qt::Key_Delete
    { 4194313, 0x01000008 }, // KEY_PAUSE       -> Qt::Key_Pause
    { 4194314, 0x01000009 }, // KEY_PRINT       -> Qt::Key_Print
    { 4194315, 0x0100000A }, // KEY_SYSREQ      -> Qt::Key_SysReq
    { 4194316, 0x0100000B }, // KEY_CLEAR       -> Qt::Key_Clear
    { 4194317, 0x01000010 }, // KEY_HOME        -> Qt::Key_Home
    { 4194318, 0x01000011 }, // KEY_END         -> Qt::Key_End
    { 4194319, 0x01000012 }, // KEY_LEFT        -> Qt::Key_Left
    { 4194320, 0x01000013 }, // KEY_UP          -> Qt::Key_Up
    { 4194321, 0x01000014 }, // KEY_RIGHT       -> Qt::Key_Right
    { 4194322, 0x01000015 }, // KEY_DOWN        -> Qt::Key_Down
    { 4194323, 0x01000016 }, // KEY_PAGEUP      -> Qt::Key_PageUp
    { 4194324, 0x01000017 }, // KEY_PAGEDOWN    -> Qt::Key_PageDown
    // Modifier keys
    { 4194325, 0x01000020 }, // KEY_SHIFT       -> Qt::Key_Shift
    { 4194326, 0x01000021 }, // KEY_CTRL        -> Qt::Key_Control
    { 4194327, 0x01000022 }, // KEY_META        -> Qt::Key_Meta
    { 4194328, 0x01000023 }, // KEY_ALT         -> Qt::Key_Alt
    { 4194329, 0x01000024 }, // KEY_CAPSLOCK    -> Qt::Key_CapsLock
    { 4194330, 0x01000025 }, // KEY_NUMLOCK     -> Qt::Key_NumLock
    { 4194331, 0x01000026 }, // KEY_SCROLLLOCK  -> Qt::Key_ScrollLock
    // Function keys
    { 4194332, 0x01000030 }, // KEY_F1          -> Qt::Key_F1
    { 4194333, 0x01000031 }, // KEY_F2          -> Qt::Key_F2
    { 4194334, 0x01000032 }, // KEY_F3          -> Qt::Key_F3
    { 4194335, 0x01000033 }, // KEY_F4          -> Qt::Key_F4
    { 4194336, 0x01000034 }, // KEY_F5          -> Qt::Key_F5
    { 4194337, 0x01000035 }, // KEY_F6          -> Qt::Key_F6
    { 4194338, 0x01000036 }, // KEY_F7          -> Qt::Key_F7
    { 4194339, 0x01000037 }, // KEY_F8          -> Qt::Key_F8
    { 4194340, 0x01000038 }, // KEY_F9          -> Qt::Key_F9
    { 4194341, 0x01000039 }, // KEY_F10         -> Qt::Key_F10
    { 4194342, 0x0100003A }, // KEY_F11         -> Qt::Key_F11
    { 4194343, 0x0100003B }, // KEY_F12         -> Qt::Key_F12
    // Numpad keys (mapped to ASCII equivalents)
    { 4194433, 0x2A },       // KEY_KP_MULTIPLY -> Qt::Key_Asterisk
    { 4194434, 0x2F },       // KEY_KP_DIVIDE   -> Qt::Key_Slash
    { 4194435, 0x2D },       // KEY_KP_SUBTRACT -> Qt::Key_Minus
    { 4194436, 0x2E },       // KEY_KP_PERIOD   -> Qt::Key_Period
    { 4194437, 0x2B },       // KEY_KP_ADD      -> Qt::Key_Plus
    { 4194438, 0x30 },       // KEY_KP_0        -> Qt::Key_0
    { 4194439, 0x31 },       // KEY_KP_1        -> Qt::Key_1
    { 4194440, 0x32 },       // KEY_KP_2        -> Qt::Key_2
    { 4194441, 0x33 },       // KEY_KP_3        -> Qt::Key_3
    { 4194442, 0x34 },       // KEY_KP_4        -> Qt::Key_4
    { 4194443, 0x35 },       // KEY_KP_5        -> Qt::Key_5
    { 4194444, 0x36 },       // KEY_KP_6        -> Qt::Key_6
    { 4194445, 0x37 },       // KEY_KP_7        -> Qt::Key_7
    { 4194446, 0x38 },       // KEY_KP_8        -> Qt::Key_8
    { 4194447, 0x39 },       // KEY_KP_9        -> Qt::Key_9
};

const int QtKeyMapper::BASE_TABLE_SIZE = sizeof(BASE_TABLE) / sizeof(BASE_TABLE[0]);

QtKeyMapper::QtKeyMapper() : _ext_count(0) {
    // Zero extension arrays
    for (int i = 0; i < 64; i++) { _ext_godot[i] = 0; _ext_qt[i] = 0; }
}

int QtKeyMapper::godot_to_qt(int godot_key) const {
    // 1. Check runtime extensions first (linear scan, small)
    for (int i = 0; i < _ext_count; i++) {
        if (_ext_godot[i] == godot_key) return _ext_qt[i];
    }
    // 2. ASCII printable passthrough: Godot uses Unicode codepoints for A-Z, 0-9, punctuation
    if (godot_key >= 32 && godot_key <= 126) return godot_key;
    // 3. Binary search BASE_TABLE (sorted by godot key)
    int lo = 0, hi = BASE_TABLE_SIZE - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (BASE_TABLE[mid].godot == godot_key) return BASE_TABLE[mid].qt;
        if (BASE_TABLE[mid].godot < godot_key) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

void QtKeyMapper::add_mapping(int godot_key, int qt_key) {
    // Update existing if present
    for (int i = 0; i < _ext_count; i++) {
        if (_ext_godot[i] == godot_key) { _ext_qt[i] = qt_key; return; }
    }
    // Add new if space available
    if (_ext_count < 64) {
        _ext_godot[_ext_count] = godot_key;
        _ext_qt[_ext_count] = qt_key;
        _ext_count++;
    }
}

void QtKeyMapper::remove_mapping(int godot_key) {
    for (int i = 0; i < _ext_count; i++) {
        if (_ext_godot[i] == godot_key) {
            _ext_godot[i] = _ext_godot[_ext_count - 1];
            _ext_qt[i] = _ext_qt[_ext_count - 1];
            _ext_count--;
            return;
        }
    }
}

void QtKeyMapper::reset_to_defaults() { _ext_count = 0; }
