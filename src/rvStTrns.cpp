// Created by Unium on 06.03.26

#include "rvStTrns.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace RV {
/*---------------------------------------------------------
 * FN: ~CTrns
 * DESC: destructor
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
CTrns::~CTrns() { vClose(); }

/*---------------------------------------------------------
 * FN: bOpen
 * DESC: opens shared memory
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CTrns::bOpen() -> bool {
    if (m_pPtr)
        return true;

    m_iFd = shm_open(s_szShmName, O_RDWR, 0666);
    if (m_iFd < 0) {
        m_iFd = shm_open(s_szShmName, O_CREAT | O_RDWR, 0666);
        if (m_iFd < 0) {
            perror("[recar|roverpp] shm_open");
            return false;
        }
        m_bCreated = true;
        if (ftruncate(m_iFd, (off_t)s_iShmTotalSize) < 0) {
            perror("[recar|roverpp] ftruncate");
            ::close(m_iFd);
            m_iFd = -1;
            shm_unlink(s_szShmName);
            m_bCreated = false;
            return false;
        }
    } else {
        struct stat objSt;
        if (fstat(m_iFd, &objSt) == 0 && objSt.st_size < (off_t)s_iShmTotalSize) {
            if (ftruncate(m_iFd, (off_t)s_iShmTotalSize) < 0) {
                perror("[recar|roverpp] ftruncate (resize)");
                ::close(m_iFd);
                m_iFd = -1;
                return false;
            }
        }
    }

    m_pPtr = mmap(nullptr, s_iShmTotalSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_iFd, 0);
    if (m_pPtr == MAP_FAILED) {
        perror("[recar|roverpp] mmap");
        m_pPtr = nullptr;
        ::close(m_iFd);
        m_iFd = -1;
        return false;
    }

    if (m_bCreated) {
        memset(m_pPtr, 0, s_iShmTotalSize);
    }

    fprintf(stderr, "[recar|roverpp] shared mem opened: %s (%zu bytes, header %zu)\n", s_szShmName, s_iShmTotalSize,
            s_iShmHeaderSize);
    return true;
}

/*---------------------------------------------------------
 * FN: vClose
 * DESC: closes shared memory
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CTrns::vClose() -> void {
    if (m_pPtr) {
        munmap(m_pPtr, s_iShmTotalSize);
        m_pPtr = nullptr;
    }
    if (m_iFd >= 0) {
        ::close(m_iFd);
        m_iFd = -1;
    }
}

/*---------------------------------------------------------
 * FN: pTryAcquireFrame
 * DESC: atomically checks frame status and returns header
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CTrns::pTryAcquireFrame() -> CShmHead * {
    if (!m_pPtr)
        return nullptr;

    auto *pHeader = static_cast<CShmHead *>(m_pPtr);

    uint8_t iExpected = SHM_READY;
    if (pHeader->m_iStatus.compare_exchange_strong(iExpected, SHM_IDLE, std::memory_order_acquire,
                                                   std::memory_order_relaxed)) {
        return pHeader;
    }
    return nullptr;
}

/*---------------------------------------------------------
 * FN: vSetGameResolution
 * DESC: writes resolution into shm
 * PARMS: iWidth, iHeight
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CTrns::vSetGameResolution(uint32_t iWidth, uint32_t iHeight) -> void {
    if (!m_pPtr)
        return;
    auto *pHeader = static_cast<CShmHead *>(m_pPtr);
    pHeader->m_iGameWidth = iWidth;
    pHeader->m_iGameHeight = iHeight;
}

/*---------------------------------------------------------
 * FN: pPixelData
 * DESC: returns ptr to pixel segment
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CTrns::pPixelData() const -> const uint8_t * {
    if (!m_pPtr)
        return nullptr;
    return static_cast<const uint8_t *>(m_pPtr) + s_iShmHeaderSize;
}
} // namespace RV
