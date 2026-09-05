// SHM consumer — Linux/POSIX implementation (Task 9)
#ifndef _WIN32

#include <sys/mman.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "qtwindow_shm.h"

// ---------------------------------------------------------------------------
// Naming convention (Qt side creates these via QSharedMemory / POSIX):
//   SHM:       "/MindSCADA_WB_{bridge_id}"
//   Frame sem: "/MindSCADA_FR_{bridge_id}"
//   Input sem: "/MindSCADA_IN_{bridge_id}"
// ---------------------------------------------------------------------------

QtWindowShm::QtWindowShm()
    : m_shmFd(-1)
    , m_frameSem(SEM_FAILED)
    , m_inputSem(SEM_FAILED)
    , m_ptr(NULL)
    , m_size(0)
{
    m_shmPath[0]   = '\0';
    m_framePath[0] = '\0';
    m_inputPath[0] = '\0';
}

QtWindowShm::~QtWindowShm() {
    close();
}

bool QtWindowShm::open(const char* bridge_id) {
    close(); // ensure clean state

    // Build POSIX names
    snprintf(m_shmPath,   sizeof(m_shmPath),   "/MindSCADA_WB_%s",  bridge_id);
    snprintf(m_framePath, sizeof(m_framePath), "/MindSCADA_FR_%s",  bridge_id);
    snprintf(m_inputPath, sizeof(m_inputPath), "/MindSCADA_IN_%s",  bridge_id);

    // Open shared memory (Qt already created and sized it)
    m_shmFd = shm_open(m_shmPath, O_RDWR, 0);
    if (m_shmFd < 0) {
        return false;
    }

    // Step 1: map just the header to read max_width / max_height
    void* probe = mmap(NULL, sizeof(QtWindowHeader), PROT_READ, MAP_SHARED, m_shmFd, 0);
    if (probe == MAP_FAILED) {
        ::close(m_shmFd);
        m_shmFd = -1;
        return false;
    }

    const QtWindowHeader* hdr = static_cast<const QtWindowHeader*>(probe);
    if (hdr->magic != QTWINDOW_MAGIC || hdr->max_width == 0 || hdr->max_height == 0) {
        munmap(probe, sizeof(QtWindowHeader));
        ::close(m_shmFd);
        m_shmFd = -1;
        return false;
    }
    size_t pixel_bytes = (size_t)hdr->max_width * (size_t)hdr->max_height * 4u;
    size_t full_size   = sizeof(QtWindowHeader) + QTWINDOW_RING_BYTES + pixel_bytes;
    munmap(probe, sizeof(QtWindowHeader));

    // Step 2: map the full region read-write
    void* full = mmap(NULL, full_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_shmFd, 0);
    if (full == MAP_FAILED) {
        ::close(m_shmFd);
        m_shmFd = -1;
        return false;
    }

    m_ptr  = static_cast<uint8_t*>(full);
    m_size = full_size;

    // Close the fd now that the mapping is established; the mapping keeps the object alive
    ::close(m_shmFd);
    m_shmFd = -1;

    // Open named semaphores (Qt created them; we open without O_CREAT)
    m_frameSem = sem_open(m_framePath, 0);
    m_inputSem = sem_open(m_inputPath, 0);

    if (m_frameSem == SEM_FAILED || m_inputSem == SEM_FAILED) {
        munmap(m_ptr, m_size);
        m_ptr = NULL; m_size = 0;
        // m_shmFd already closed and set to -1 after the full mmap
        if (m_frameSem != SEM_FAILED) { sem_close(static_cast<sem_t*>(m_frameSem)); m_frameSem = SEM_FAILED; }
        if (m_inputSem != SEM_FAILED) { sem_close(static_cast<sem_t*>(m_inputSem)); m_inputSem = SEM_FAILED; }
        return false;
    }

    return true;
}

void QtWindowShm::close() {
    if (m_ptr) {
        munmap(m_ptr, m_size);
        m_ptr  = NULL;
        m_size = 0;
    }
    if (m_shmFd >= 0) {
        ::close(m_shmFd);
        m_shmFd = -1;
    }
    if (m_frameSem != SEM_FAILED) {
        sem_close(static_cast<sem_t*>(m_frameSem));
        m_frameSem = SEM_FAILED;
    }
    if (m_inputSem != SEM_FAILED) {
        sem_close(static_cast<sem_t*>(m_inputSem));
        m_inputSem = SEM_FAILED;
    }
    m_shmPath[0]   = '\0';
    m_framePath[0] = '\0';
    m_inputPath[0] = '\0';
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
    if (m_frameSem == SEM_FAILED) return false;

    if (timeout_ms < 0) {
        int rc;
        do {
            rc = sem_wait(static_cast<sem_t*>(m_frameSem));
        } while (rc == -1 && errno == EINTR);
        return rc == 0;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec  += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec  += 1;
        ts.tv_nsec -= 1000000000L;
    }

    int rc;
    do {
        rc = sem_timedwait(static_cast<sem_t*>(m_frameSem), &ts);
    } while (rc == -1 && errno == EINTR);
    return rc == 0;
}

void QtWindowShm::signal_input() {
    if (m_inputSem != SEM_FAILED) {
        sem_post(static_cast<sem_t*>(m_inputSem));
    }
}

#endif // !_WIN32
