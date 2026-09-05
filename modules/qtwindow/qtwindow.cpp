#include "qtwindow.h"

#include "core/os/time.h"
#include "core/os/keyboard.h"

#include <atomic>
#include <string.h>
#include <stdio.h>

void QtWindowOverlay::_bind_methods() {
    ClassDB::bind_method(D_METHOD("open", "form_name"), &QtWindowOverlay::open);
    ClassDB::bind_method(D_METHOD("close"), &QtWindowOverlay::close);
    ClassDB::bind_method(D_METHOD("is_open"), &QtWindowOverlay::is_open);
    ClassDB::bind_method(D_METHOD("get_form_name"), &QtWindowOverlay::get_form_name);
    ClassDB::bind_method(D_METHOD("set_scale_mode", "mode"), &QtWindowOverlay::set_scale_mode);
    ClassDB::bind_method(D_METHOD("get_scale_mode"), &QtWindowOverlay::get_scale_mode);
    ClassDB::bind_method(D_METHOD("_on_frame_ready"), &QtWindowOverlay::_on_frame_ready);
    ClassDB::bind_method(D_METHOD("_on_bridge_alive"), &QtWindowOverlay::_on_bridge_alive);

    ADD_SIGNAL(MethodInfo("frame_ready",
        PropertyInfo(Variant::INT, "frame_seq")));
    // Emitted on every signal from the Qt side (frame OR heartbeat) so the UI
    // can tell an idle-but-alive bridge apart from a dead one.
    ADD_SIGNAL(MethodInfo("bridge_alive"));

    BIND_ENUM_CONSTANT(SCALE_ASPECT_FIT);
    BIND_ENUM_CONSTANT(SCALE_FILL);

    ADD_PROPERTY(PropertyInfo(Variant::INT, "scale_mode", PROPERTY_HINT_ENUM, "AspectFit,Fill"),
                 "set_scale_mode", "get_scale_mode");
}

QtWindowOverlay::QtWindowOverlay() {
    set_focus_mode(FOCUS_ALL);  // receive keyboard input when focused
}

QtWindowOverlay::~QtWindowOverlay() {
    close();
}

void QtWindowOverlay::open(const String &p_form_name) {
    close();
    _form_name = p_form_name;
    CharString cs = p_form_name.utf8();
    fprintf(stderr, "[QtWindowOverlay] open: bridge_id='%s'\n", cs.get_data());
    if (!_shm.open(cs.get_data())) {
        fprintf(stderr, "[QtWindowOverlay] open: _shm.open FAILED\n");
        return;  // SHM not ready yet — caller may retry
    }
    fprintf(stderr, "[QtWindowOverlay] open: _shm.open OK\n");
    QtWindowHeader *hdr = _shm.header();
    int w = hdr->width > 0 ? hdr->width : hdr->max_width;
    int h = hdr->height > 0 ? hdr->height : hdr->max_height;
    _frame_buf.resize(w * h * 4);
    _frame_image = Image::create_empty(w, h, false, Image::FORMAT_RGBA8);
    _texture = ImageTexture::create_from_image(_frame_image);
    _update_draw_rect();
    _last_frame_seq = 0;   // fresh connection — no frames seen yet
    _is_open = true;
    _thread_running = true;
    _frame_thread.start(QtWindowOverlay::_frame_thread_entry, this);
}

void QtWindowOverlay::close() {
    if (!_is_open) return;
    _thread_running = false;
    // The thread uses a 100ms timeout on wait_frame_signal, so it will exit on its own.
    _frame_thread.wait_to_finish();
    _shm.close();
    _is_open = false;
    _texture.unref();
    _frame_image.unref();
    _frame_buf.resize(0);
}

void QtWindowOverlay::_frame_thread_entry(void *p_ud) {
    static_cast<QtWindowOverlay *>(p_ud)->_frame_thread_func();
}

void QtWindowOverlay::_frame_thread_func() {
    while (_thread_running) {
        if (!_shm.wait_frame_signal(100)) continue;  // timeout, check _thread_running
        if (!_thread_running) break;

        // Any signal from the Qt side means the bridge is alive — even a
        // heartbeat that carries no new frame. Reset the watchdog regardless.
        call_deferred(SNAME("_on_bridge_alive"));

        QtWindowHeader *hdr = _shm.header();
        uint32_t seq = hdr->frame_seq;  // read once (Qt increments atomically)
        if (seq == _last_frame_seq) continue;  // heartbeat only — no new frame

        uint16_t w = hdr->width;
        uint16_t h = hdr->height;
        if (w == 0 || h == 0) continue;

        const uint8_t *src = _shm.pixels();
        // stride = max_width * 4 (full allocated row)
        uint32_t stride = (uint32_t)hdr->max_width * 4;

        {
            MutexLock lock(_frame_mutex);
            // Resize to the EXACT frame size — Image::set_data on the main
            // thread requires the buffer length to equal width*height*4
            // precisely, so the buffer must also shrink when the form does.
            if (_frame_buf.size() != (int)(w * h * 4)) {
                _frame_buf.resize(w * h * 4);
            }
            uint8_t *dst = _frame_buf.ptrw();
            // BGRA → RGBA conversion, row by row
            for (uint16_t row = 0; row < h; row++) {
                const uint8_t *srow = src + (uint32_t)row * stride;
                uint8_t *drow = dst + (uint32_t)row * w * 4;
                for (uint16_t col = 0; col < w; col++) {
                    drow[col * 4 + 0] = srow[col * 4 + 2]; // R <- B
                    drow[col * 4 + 1] = srow[col * 4 + 1]; // G <- G
                    drow[col * 4 + 2] = srow[col * 4 + 0]; // B <- R
                    drow[col * 4 + 3] = srow[col * 4 + 3]; // A <- A
                }
            }
            _buf_w = w;
            _buf_h = h;
        }
        _last_frame_seq = seq;
        static int s_frame_log = 0;
        if ((s_frame_log++ % 30) == 0) {
            fprintf(stderr, "[qtwindow] frame RECV seq=%u %ux%u (every 30th)\n", seq, w, h);
        }
        // Notify main thread
        call_deferred(SNAME("_on_frame_ready"));
    }
}

void QtWindowOverlay::_on_bridge_alive() {
    if (_is_open) {
        emit_signal(SNAME("bridge_alive"));
    }
}

void QtWindowOverlay::_on_frame_ready() {
    if (!_is_open || _frame_image.is_null() || _texture.is_null()) return;
    QtWindowHeader *hdr = _shm.header();
    {
        MutexLock lock(_frame_mutex);
        // Use the dimensions that match the data in _frame_buf — hdr->width/
        // height may already have advanced to a newer (not-yet-copied) frame.
        int w = _buf_w;
        int h = _buf_h;
        if (w <= 0 || h <= 0 || _frame_buf.size() != w * h * 4) {
            return;
        }
        bool size_changed = (_frame_image->get_width() != w
                          || _frame_image->get_height() != h);
        if (size_changed) {
            _frame_image = Image::create_empty(w, h, false, Image::FORMAT_RGBA8);
        }
        _frame_image->set_data(w, h, false, Image::FORMAT_RGBA8, _frame_buf);
        if (size_changed) {
            // ImageTexture::update() requires identical dimensions, so a
            // resized frame must rebuild the texture via set_image().
            _texture->set_image(_frame_image);
            _update_draw_rect();
        } else {
            _texture->update(_frame_image);
        }
        queue_redraw();
    }
    emit_signal(SNAME("frame_ready"), (int64_t)hdr->frame_seq);
}

void QtWindowOverlay::_notification(int p_what) {
    if (p_what == NOTIFICATION_DRAW) {
        if (_texture.is_valid()) {
            draw_texture_rect(_texture, _draw_rect, false);
        }
    } else if (p_what == NOTIFICATION_RESIZED) {
        _update_draw_rect();
    } else if (p_what == NOTIFICATION_FOCUS_ENTER) {
        fprintf(stderr, "[qtwindow] NOTIFICATION_FOCUS_ENTER is_open=%d\n", _is_open ? 1 : 0);
        if (_is_open) {
            QtWindowInputSlot slot;
            memset(&slot, 0, sizeof(slot));
            slot.event_type = QTWINDOW_EV_FOCUS;
            slot.data.focus.has_focus = 1;
            _write_input_slot(slot);
        }
    } else if (p_what == NOTIFICATION_FOCUS_EXIT) {
        fprintf(stderr, "[qtwindow] NOTIFICATION_FOCUS_EXIT is_open=%d\n", _is_open ? 1 : 0);
        if (_is_open) {
            QtWindowInputSlot slot;
            memset(&slot, 0, sizeof(slot));
            slot.event_type = QTWINDOW_EV_FOCUS;
            slot.data.focus.has_focus = 0;
            _write_input_slot(slot);
        }
    }
}

void QtWindowOverlay::_update_draw_rect() {
    Size2 ctrl_size = get_size();
    if (_texture.is_null() || ctrl_size.x <= 0 || ctrl_size.y <= 0) {
        _draw_rect = Rect2(Vector2(), ctrl_size);
        return;
    }
    Size2 tex_size = _texture->get_size();
    if (tex_size.x <= 0 || tex_size.y <= 0) {
        _draw_rect = Rect2(Vector2(), ctrl_size);
        return;
    }
    if (_scale_mode == SCALE_FILL) {
        _draw_rect = Rect2(Vector2(), ctrl_size);
    } else {
        // ASPECT_FIT: letterbox
        float sx = ctrl_size.x / tex_size.x;
        float sy = ctrl_size.y / tex_size.y;
        float s = (sx < sy) ? sx : sy;
        float tw = tex_size.x * s;
        float th = tex_size.y * s;
        _draw_rect = Rect2(
                Vector2((ctrl_size.x - tw) * 0.5f, (ctrl_size.y - th) * 0.5f),
                Vector2(tw, th));
    }
}

bool QtWindowOverlay::is_open() const { return _is_open; }

String QtWindowOverlay::get_form_name() const { return _form_name; }

void QtWindowOverlay::set_scale_mode(ScaleMode p_mode) {
    _scale_mode = p_mode;
    _update_draw_rect();
}

QtWindowOverlay::ScaleMode QtWindowOverlay::get_scale_mode() const { return _scale_mode; }

// ── Input modifier helper ────────────────────────────────────────────────────
// Encodes Godot modifiers into a compact uint16_t.
// Convention (matches QtWindowInputReader.cpp which casts the stored value back):
//   0x02 = Shift, 0x04 = Ctrl, 0x08 = Alt, 0x10 = Meta
uint16_t QtWindowOverlay::_godot_mods_to_qt(const Ref<InputEventWithModifiers> &ev) {
    uint16_t mods = 0;
    if (ev->is_shift_pressed()) mods |= 0x02;
    if (ev->is_ctrl_pressed())  mods |= 0x04;
    if (ev->is_alt_pressed())   mods |= 0x08;
    if (ev->is_meta_pressed())  mods |= 0x10;
    return mods;
}

// ── Mouse button mapper: Godot MouseButton → Qt::MouseButton ────────────────
int QtWindowOverlay::_godot_mouse_button_to_qt(int btn) {
    switch (btn) {
        case 1: return 1;  // MouseButton::LEFT   → Qt::LeftButton
        case 2: return 2;  // MouseButton::RIGHT  → Qt::RightButton
        case 3: return 4;  // MouseButton::MIDDLE → Qt::MiddleButton
        default: return 0;
    }
}

// ── Write one slot to the input ring buffer ──────────────────────────────────
bool QtWindowOverlay::_write_input_slot(const QtWindowInputSlot &slot) {
    QtWindowHeader *hdr = _shm.header();
    if (!hdr) return false;

    uint32_t wpos = reinterpret_cast<const std::atomic<uint32_t>*>(&hdr->input_write_pos)
                        ->load(std::memory_order_acquire);
    uint32_t rpos = reinterpret_cast<const std::atomic<uint32_t>*>(&hdr->input_read_pos)
                        ->load(std::memory_order_acquire);
    if (wpos - rpos >= QTWINDOW_RING_SIZE) return false;  // ring full

    uint32_t idx = wpos % QTWINDOW_RING_SIZE;
    uint8_t *ring_base = reinterpret_cast<uint8_t*>(hdr) + QTWINDOW_HDR_BYTES;
    QtWindowInputSlot *ring = reinterpret_cast<QtWindowInputSlot*>(ring_base);
    ring[idx] = slot;

    reinterpret_cast<std::atomic<uint32_t>*>(&hdr->input_write_pos)
        ->store(wpos + 1, std::memory_order_release);
    _shm.signal_input();
    return true;
}

// ── Main input dispatch ──────────────────────────────────────────────────────
void QtWindowOverlay::gui_input(const Ref<InputEvent> &p_event) {
    if (!_is_open) return;
    QtWindowHeader *hdr = _shm.header();
    if (!hdr) return;

    QtWindowInputSlot slot;
    memset(&slot, 0, sizeof(slot));
    slot.timestamp_ms = (uint32_t)(Time::get_singleton()->get_ticks_msec() & 0xFFFFFFFF);

    // Map Godot control-space position to Qt widget coords via _draw_rect.
    // Works correctly for both AspectFit (letterboxed) and Fill modes.
    float dr_x = _draw_rect.position.x;
    float dr_y = _draw_rect.position.y;
    float dr_w = _draw_rect.size.x > 0 ? _draw_rect.size.x : 1.0f;
    float dr_h = _draw_rect.size.y > 0 ? _draw_rect.size.y : 1.0f;
    float qt_w = hdr->qt_window_w > 0 ? (float)hdr->qt_window_w : 1.0f;
    float qt_h = hdr->qt_window_h > 0 ? (float)hdr->qt_window_h : 1.0f;

    // ── Mouse button (press / release / double-click / wheel) ────────────────
    Ref<InputEventMouseButton> mb = p_event;
    if (mb.is_valid()) {
        Vector2 pos = mb->get_position();
        float img_x = pos.x - dr_x;
        float img_y = pos.y - dr_y;
        // Ignore clicks outside the drawn image area
        if (img_x < 0 || img_y < 0 || img_x >= dr_w || img_y >= dr_h) return;
        int16_t qx = (int16_t)CLAMP((int)(img_x * qt_w / dr_w), 0, (int)hdr->qt_window_w - 1);
        int16_t qy = (int16_t)CLAMP((int)(img_y * qt_h / dr_h), 0, (int)hdr->qt_window_h - 1);
        int btn = (int)mb->get_button_index();
        bool is_wheel = (btn >= 4 && btn <= 7);

        if (is_wheel) {
            // Godot emits a wheel notch as a press+release pair; forward only
            // the press so the Qt side scrolls one notch, not two.
            if (!mb->is_pressed()) {
                return;
            }
            slot.event_type = QTWINDOW_EV_WHEEL;
            slot.data.wheel.x = qx;
            slot.data.wheel.y = qy;
            slot.data.wheel.modifiers = _godot_mods_to_qt(mb);
            // 4=WheelUp, 5=WheelDown → vertical; 6=WheelLeft, 7=WheelRight → horizontal
            switch (btn) {
                case 4: slot.data.wheel.dy =  120; break;
                case 5: slot.data.wheel.dy = -120; break;
                case 6: slot.data.wheel.dx = -120; break;
                case 7: slot.data.wheel.dx =  120; break;
                default: break;
            }
        } else if (mb->is_double_click()) {
            slot.event_type = QTWINDOW_EV_MOUSE_DBLCLICK;
            slot.data.mouse.x = qx;
            slot.data.mouse.y = qy;
            slot.data.mouse.button = (uint8_t)_godot_mouse_button_to_qt(btn);
            slot.data.mouse.dbl = 1;
            slot.data.mouse.modifiers = _godot_mods_to_qt(mb);
        } else if (mb->is_pressed()) {
            slot.event_type = QTWINDOW_EV_MOUSE_PRESS;
            slot.data.mouse.x = qx;
            slot.data.mouse.y = qy;
            slot.data.mouse.button = (uint8_t)_godot_mouse_button_to_qt(btn);
            slot.data.mouse.modifiers = _godot_mods_to_qt(mb);
        } else {
            slot.event_type = QTWINDOW_EV_MOUSE_RELEASE;
            slot.data.mouse.x = qx;
            slot.data.mouse.y = qy;
            slot.data.mouse.button = (uint8_t)_godot_mouse_button_to_qt(btn);
            slot.data.mouse.modifiers = _godot_mods_to_qt(mb);
        }
        bool ok = _write_input_slot(slot);
        fprintf(stderr, "[qtwindow] gui_input MOUSE ev=%d btn=%d qx=%d qy=%d written=%d\n",
                slot.event_type, btn, qx, qy, ok ? 1 : 0);
        return;
    }

    // ── Mouse motion ─────────────────────────────────────────────────────────
    Ref<InputEventMouseMotion> mm = p_event;
    if (mm.is_valid()) {
        Vector2 pos = mm->get_position();
        float img_x = CLAMP(pos.x - dr_x, 0.0f, dr_w - 1.0f);
        float img_y = CLAMP(pos.y - dr_y, 0.0f, dr_h - 1.0f);
        slot.event_type = QTWINDOW_EV_MOUSE_MOVE;
        slot.data.mouse.x = (int16_t)(img_x * qt_w / dr_w);
        slot.data.mouse.y = (int16_t)(img_y * qt_h / dr_h);
        slot.data.mouse.modifiers = _godot_mods_to_qt(mm);
        bool ok = _write_input_slot(slot);
        static int s_move_log = 0;
        if ((s_move_log++ % 30) == 0) {
            fprintf(stderr, "[qtwindow] gui_input MOVE qx=%d qy=%d written=%d (every 30th)\n",
                    slot.data.mouse.x, slot.data.mouse.y, ok ? 1 : 0);
        }
        return;
    }

    // ── Key press / release ──────────────────────────────────────────────────
    Ref<InputEventKey> ke = p_event;
    if (ke.is_valid()) {
        slot.event_type = ke->is_pressed() ? QTWINDOW_EV_KEY_PRESS : QTWINDOW_EV_KEY_RELEASE;
        // Strip modifier bits from keycode using KeyModifierMask::CODE_MASK
        int godot_key = (int)(ke->get_keycode() & KeyModifierMask::CODE_MASK);
        slot.data.key.qt_key    = (uint32_t)_keymapper.godot_to_qt(godot_key);
        slot.data.key.scan_code = (uint32_t)(int)(ke->get_physical_keycode()
                                               & KeyModifierMask::CODE_MASK);
        slot.data.key.modifiers = _godot_mods_to_qt(ke);
        slot.data.key.unicode   = (uint32_t)ke->get_unicode();
        // Fill UTF-8 text (up to 7 chars + NUL)
        if (slot.data.key.unicode > 0) {
            String text = String::chr(slot.data.key.unicode);
            CharString cs = text.utf8();
            const char *s = cs.get_data();
            for (int i = 0; i < 7 && s[i] != '\0'; i++) {
                slot.data.key.text[i] = s[i];
            }
        }
        bool ok = _write_input_slot(slot);
        fprintf(stderr, "[qtwindow] gui_input KEY ev=%d qt_key=%u unicode=%u written=%d\n",
                slot.event_type, slot.data.key.qt_key, slot.data.key.unicode, ok ? 1 : 0);
        return;
    }
}
