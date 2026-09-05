#ifndef QTWINDOW_H
#define QTWINDOW_H

#include "scene/gui/control.h"
#include "scene/resources/image_texture.h"
#include "core/os/thread.h"
#include "core/os/mutex.h"
#include "core/io/image.h"
#include "core/input/input_event.h"
#include <atomic>

#include "qtwindow_shm.h"
#include "qtwindow_keymapper.h"

class QtWindowOverlay : public Control {
    GDCLASS(QtWindowOverlay, Control);

public:
    enum ScaleMode { SCALE_ASPECT_FIT = 0, SCALE_FILL = 1 };

protected:
    static void _bind_methods();
    void _notification(int p_what);
    // gui_input is the correct C++ override point for Control subclasses (not _input which is GDVIRTUAL-only)
    virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
    QtWindowOverlay();
    ~QtWindowOverlay() override;

    void open(const String &p_form_name);
    void close();
    bool is_open() const;
    String get_form_name() const;
    void set_scale_mode(ScaleMode p_mode);
    ScaleMode get_scale_mode() const;

    // Called on main thread via call_deferred
    void _on_frame_ready();
    void _on_bridge_alive();

private:
    static void _frame_thread_entry(void *p_ud);
    void _frame_thread_func();
    void _update_draw_rect();
    bool _write_input_slot(const QtWindowInputSlot &slot);
    static uint16_t _godot_mods_to_qt(const Ref<InputEventWithModifiers> &ev);
    static int _godot_mouse_button_to_qt(int godot_btn);

    String          _form_name;
    QtWindowShm     _shm;
    QtKeyMapper     _keymapper;
    Thread          _frame_thread;
    Mutex           _frame_mutex;
    std::atomic<bool> _thread_running{false};
    bool            _is_open = false;
    ScaleMode       _scale_mode = SCALE_ASPECT_FIT;

    Ref<Image>        _frame_image;
    Ref<ImageTexture> _texture;
    PackedByteArray   _frame_buf;

    // Draw cache (updated in _update_draw_rect, used in _notification DRAW)
    Rect2 _draw_rect;
    uint32_t _last_frame_seq = 0;
    // Dimensions of the pixels currently held in _frame_buf. Written by the
    // frame thread, read by _on_frame_ready — both under _frame_mutex — so the
    // main thread always uses the size that matches the buffer contents.
    int _buf_w = 0;
    int _buf_h = 0;
};

VARIANT_ENUM_CAST(QtWindowOverlay::ScaleMode);

#endif
