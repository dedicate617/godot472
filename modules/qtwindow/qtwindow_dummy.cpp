// SHM consumer — no-op stub for unsupported platforms
// The SCsub only includes this file when platform != "windows" and != "linuxbsd",
// so _WIN32 is not defined here. The header's #else branch applies (POSIX member layout).
#include "qtwindow_shm.h"
#include <semaphore.h>   // for SEM_FAILED

QtWindowShm::QtWindowShm()
    : m_shmFd(-1)
    , m_frameSem(SEM_FAILED)
    , m_inputSem(SEM_FAILED)
    , m_ptr(nullptr)
    , m_size(0)
{
    m_shmPath[0]   = '\0';
    m_framePath[0] = '\0';
    m_inputPath[0] = '\0';
}

QtWindowShm::~QtWindowShm() {
    close();
}

bool QtWindowShm::open(const char* /*bridge_id*/) { return false; }
void QtWindowShm::close() {}
bool QtWindowShm::is_open() const { return false; }
QtWindowHeader* QtWindowShm::header() const { return nullptr; }
uint8_t*        QtWindowShm::pixels()  const { return nullptr; }
bool QtWindowShm::wait_frame_signal(int /*timeout_ms*/) { return false; }
void QtWindowShm::signal_input() {}
