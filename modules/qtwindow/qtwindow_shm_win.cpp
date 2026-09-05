// SHM consumer — Windows implementation (Task 9)
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "qtwindow_shm.h"

// ---------------------------------------------------------------------------
// Helper: build wide-string name from ASCII bridge_id
// Naming convention (Qt side creates these):
//   SHM mapping : "MindSCADA_WB_{bridge_id}"
//   Frame event : "MindSCADA_FR_{bridge_id}"
//   Input event : "MindSCADA_IN_{bridge_id}"
// ---------------------------------------------------------------------------
static void make_wide(const char* narrow, wchar_t* wide, int wide_len) {
    MultiByteToWideChar(CP_UTF8, 0, narrow, -1, wide, wide_len);
}

// ---------------------------------------------------------------------------
QtWindowShm::QtWindowShm()
    : m_hMapping(NULL)
    , m_hFrameEvent(NULL)
    , m_hInputEvent(NULL)
    , m_ptr(NULL)
    , m_size(0)
{}

QtWindowShm::~QtWindowShm() {
    close();
}

bool QtWindowShm::open(const char* bridge_id) {
    close(); // ensure clean state

    // Build names
    char shmName[192], frameName[192], inputName[192];
    snprintf(shmName,   sizeof(shmName),   "MindSCADA_WB_%s", bridge_id);
    snprintf(frameName, sizeof(frameName), "MindSCADA_FR_%s", bridge_id);
    snprintf(inputName, sizeof(inputName), "MindSCADA_IN_%s", bridge_id);

    wchar_t wShmName[192], wFrameName[192], wInputName[192];
    make_wide(shmName,   wShmName,   192);
    make_wide(frameName, wFrameName, 192);
    make_wide(inputName, wInputName, 192);

    // Open the file mapping (Qt created and sized it)
    fprintf(stderr, "[qtwindow] open: trying SHM '%s'\n", shmName);
    m_hMapping = (void*)OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, wShmName);
    if (!m_hMapping) {
        fprintf(stderr, "[qtwindow] open: OpenFileMappingW FAILED '%s' err=%lu\n", shmName, GetLastError());
        return false;
    }
    fprintf(stderr, "[qtwindow] open: SHM mapping opened OK\n");

    // Step 1: map just the header to read max_width / max_height
    void* probe = MapViewOfFile((HANDLE)m_hMapping, FILE_MAP_READ, 0, 0, sizeof(QtWindowHeader));
    if (!probe) {
        fprintf(stderr, "[qtwindow] open: MapViewOfFile(probe) FAILED err=%lu\n", GetLastError());
        CloseHandle((HANDLE)m_hMapping);
        m_hMapping = NULL;
        return false;
    }

    const QtWindowHeader* probe_hdr = static_cast<const QtWindowHeader*>(probe);
    if (probe_hdr->magic != QTWINDOW_MAGIC || probe_hdr->max_width == 0 || probe_hdr->max_height == 0) {
        fprintf(stderr, "[qtwindow] open: header INVALID magic=0x%08X max=%ux%u\n",
                probe_hdr->magic, probe_hdr->max_width, probe_hdr->max_height);
        UnmapViewOfFile(probe);
        CloseHandle((HANDLE)m_hMapping);
        m_hMapping = NULL;
        return false;
    }
    size_t pixel_bytes = (size_t)probe_hdr->max_width * (size_t)probe_hdr->max_height * 4u;
    size_t full_size   = sizeof(QtWindowHeader) + QTWINDOW_RING_BYTES + pixel_bytes;
    fprintf(stderr, "[qtwindow] open: header OK max=%ux%u full_size=%zu\n",
            probe_hdr->max_width, probe_hdr->max_height, full_size);
    UnmapViewOfFile(probe);

    // Step 2: map the full region read-write
    void* full = MapViewOfFile((HANDLE)m_hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, full_size);
    if (!full) {
        fprintf(stderr, "[qtwindow] open: MapViewOfFile(full) FAILED err=%lu\n", GetLastError());
        CloseHandle((HANDLE)m_hMapping);
        m_hMapping = NULL;
        return false;
    }

    m_ptr  = static_cast<uint8_t*>(full);
    m_size = full_size;

    // Open the named events (Qt created them as auto-reset)
    m_hFrameEvent = (void*)OpenEventW(SYNCHRONIZE,             FALSE, wFrameName);
    m_hInputEvent = (void*)OpenEventW(EVENT_MODIFY_STATE,      FALSE, wInputName);

    if (!m_hFrameEvent || !m_hInputEvent) {
        fprintf(stderr, "[qtwindow] open: events FAILED frame=%p input=%p err=%lu\n",
                m_hFrameEvent, m_hInputEvent, GetLastError());
        // Events not yet available — clean up
        UnmapViewOfFile(m_ptr);
        m_ptr = NULL; m_size = 0;
        if (m_hFrameEvent) { CloseHandle((HANDLE)m_hFrameEvent); m_hFrameEvent = NULL; }
        if (m_hInputEvent) { CloseHandle((HANDLE)m_hInputEvent); m_hInputEvent = NULL; }
        CloseHandle((HANDLE)m_hMapping);
        m_hMapping = NULL;
        return false;
    }

    fprintf(stderr, "[qtwindow] open: SUCCESS bridge_id='%s'\n", bridge_id);
    return true;
}

void QtWindowShm::close() {
    if (m_ptr) {
        UnmapViewOfFile(m_ptr);
        m_ptr  = NULL;
        m_size = 0;
    }
    if (m_hMapping)    { CloseHandle((HANDLE)m_hMapping);    m_hMapping    = NULL; }
    if (m_hFrameEvent) { CloseHandle((HANDLE)m_hFrameEvent); m_hFrameEvent = NULL; }
    if (m_hInputEvent) { CloseHandle((HANDLE)m_hInputEvent); m_hInputEvent = NULL; }
}

bool QtWindowShm::is_open() const {
    return m_ptr != NULL;
}

QtWindowHeader* QtWindowShm::header() const {
    if (!m_ptr) return NULL;
    return reinterpret_cast<QtWindowHeader*>(m_ptr);
}

uint8_t* QtWindowShm::pixels() const {
    if (!m_ptr) return NULL;
    return m_ptr + sizeof(QtWindowHeader) + QTWINDOW_RING_BYTES;
}

bool QtWindowShm::wait_frame_signal(int timeout_ms) {
    if (!m_hFrameEvent) return false;
    DWORD timeout_dw = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject((HANDLE)m_hFrameEvent, timeout_dw);
    return result == WAIT_OBJECT_0;
}

void QtWindowShm::signal_input() {
    if (m_hInputEvent) {
        SetEvent((HANDLE)m_hInputEvent);
    }
}

#endif // _WIN32
