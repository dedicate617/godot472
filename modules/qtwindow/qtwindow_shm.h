#pragma once
#include "qtwindow_protocol.h"
#include <stddef.h>

class QtWindowShm {
public:
    QtWindowShm();
    ~QtWindowShm();

    // Open shared memory for a given bridge ID. Returns false on failure.
    bool open(const char* bridge_id);
    void close();

    bool is_open() const;

    QtWindowHeader* header() const;   // pointer into SHM; null if not open
    uint8_t*        pixels() const;   // pointer to pixel buffer after header+ring; null if not open

    // Block until Qt signals a new frame, OR timeout_ms elapses.
    // Returns true if signaled, false on timeout/error.
    bool wait_frame_signal(int timeout_ms);

    // Signal Qt that input is available (wake the input reader thread).
    void signal_input();

private:
    // Platform-specific handles
#ifdef _WIN32
    void* m_hMapping;        // HANDLE
    void* m_hFrameEvent;     // HANDLE (auto-reset named event)
    void* m_hInputEvent;     // HANDLE (auto-reset named event)
#else
    int   m_shmFd;
    void* m_frameSem;        // sem_t*
    void* m_inputSem;        // sem_t*
    char  m_shmPath[128];
    char  m_framePath[128];
    char  m_inputPath[128];
#endif
    uint8_t* m_ptr;
    size_t   m_size;
};
