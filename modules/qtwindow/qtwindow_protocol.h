// modules/qtwindow/qtwindow_protocol.h
#ifndef QTWINDOW_PROTOCOL_H
#define QTWINDOW_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */
#define QTWINDOW_MAGIC        0x4D534742u  /* "MSGB" */
#define QTWINDOW_VERSION      1u
#define QTWINDOW_MAX_DIRTY    8
#define QTWINDOW_RING_SIZE    128
#define QTWINDOW_SLOT_BYTES   32
#define QTWINDOW_HDR_BYTES    256
#define QTWINDOW_RING_BYTES   4096   /* RING_SIZE * SLOT_BYTES */

/* Pixel format values for QtWindowHeader.pixel_format */
#define QTWINDOW_FMT_BGRA8    0

/* Flag bits for QtWindowHeader.flags */
#define QTWINDOW_FLAG_QT_READY    (1u << 0)
#define QTWINDOW_FLAG_GODOT_READY (1u << 1)
#define QTWINDOW_FLAG_DEBUG_MODE  (1u << 2)

/* Input event types */
#define QTWINDOW_EV_EMPTY          0
#define QTWINDOW_EV_MOUSE_PRESS    1
#define QTWINDOW_EV_MOUSE_RELEASE  2
#define QTWINDOW_EV_MOUSE_MOVE     3
#define QTWINDOW_EV_KEY_PRESS      4
#define QTWINDOW_EV_KEY_RELEASE    5
#define QTWINDOW_EV_WHEEL          6
#define QTWINDOW_EV_FOCUS          7
#define QTWINDOW_EV_MOUSE_DBLCLICK 8

/* ── Dirty rectangle (physical pixels) ─────────────────────────────────── */
typedef struct {
    uint16_t x, y, w, h;
} QtWindowDirtyRect;  /* 8 bytes */

/* ── Shared memory header (256 bytes, natural alignment) ────────────────── */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t width;           /* current frame width  (physical px, <= max_width)  */
    uint16_t height;          /* current frame height (physical px, <= max_height) */
    uint16_t max_width;       /* allocated pixel buffer columns (physical px)      */
    uint16_t max_height;      /* allocated pixel buffer rows    (physical px)      */
    uint16_t qt_dpr_x100;     /* DPR x 100  (e.g. 100=1.0x, 150=1.5x, 200=2.0x)  */
    uint8_t  pixel_format;    /* QTWINDOW_FMT_BGRA8 = 0                           */
    uint8_t  dirty_count;     /* number of valid dirty_rects (0-8)                */
    uint8_t  _pad0[2];        /* explicit pad so frame_seq is 4-byte aligned       */
    uint32_t frame_seq;       /* monotonically increasing; rollback to 0 = SHM rebuild */
    QtWindowDirtyRect dirty_rects[QTWINDOW_MAX_DIRTY];
    uint16_t qt_window_w;     /* Qt logical window width  (for coordinate mapping) */
    uint16_t qt_window_h;     /* Qt logical window height (for coordinate mapping) */
    uint32_t input_write_pos; /* written by Godot (atomic release)                */
    uint32_t input_read_pos;  /* written by Qt    (atomic release)                */
    uint32_t flags;           /* QTWINDOW_FLAG_* bitmask                          */
    uint8_t  _reserved[152];  /* pad to 256 bytes                                 */
} QtWindowHeader;

/* ── Input ring buffer slot (32 bytes) ──────────────────────────────────── */
typedef struct {
    uint8_t  event_type;   /* QTWINDOW_EV_* */
    uint8_t  _pad0[3];     /* explicit pad: timestamp_ms must be 4-byte aligned */
    uint32_t timestamp_ms;
    union {
        struct {
            int16_t  x, y;
            uint8_t  button;   /* Qt::MouseButton value */
            uint8_t  dbl;      /* 1 = double-click */
            uint16_t modifiers;/* Qt::KeyboardModifiers */
            uint8_t  _pad[16];
        } mouse;               /* 24 bytes */
        struct {
            uint32_t qt_key;   /* Qt::Key value */
            uint32_t scan_code;
            uint16_t modifiers;/* Qt::KeyboardModifiers */
            uint8_t  _pad[2];
            uint32_t unicode;
            char     text[8];  /* UTF-8, null-terminated */
        } key;                 /* 24 bytes */
        struct {
            int16_t  x, y, dx, dy;
            uint16_t modifiers;
            uint8_t  _pad[14];
        } wheel;               /* 24 bytes */
        struct {
            uint8_t  has_focus;
            uint8_t  _pad[23];
        } focus;               /* 24 bytes */
        uint8_t _raw[24];
    } data;
} QtWindowInputSlot;  /* 8 + 24 = 32 bytes */

#ifdef __cplusplus
} /* extern "C" */
#endif

/* ── FNV-1a 32-bit hash — C and C++ compatible (outside extern "C" to avoid linkage warning) */
static inline uint32_t qtwindow_fnv1a32(const char *s, size_t len) {
    uint32_t h = 2166136261u;
    size_t i;
    for (i = 0; i < len; i++) { h ^= (uint8_t)s[i]; h *= 16777619u; }
    return h;
}

#ifdef __cplusplus
/* Compile-time layout checks */
static_assert(sizeof(QtWindowDirtyRect) ==  8, "QtWindowDirtyRect layout");
static_assert(sizeof(QtWindowHeader)    == 256, "QtWindowHeader layout");
static_assert(sizeof(QtWindowInputSlot) ==  32, "QtWindowInputSlot layout");
#endif

#endif /* QTWINDOW_PROTOCOL_H */
