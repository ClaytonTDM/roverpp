// Created by Unium on 06.03.26

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace RV {
// smem layout (64b header):
// producer region:
//   b 0:       status flag     (atomic uint8: 0 = idle, 1 = writing, 2 = ready)
//   b 1-3:     padding
//   b 4-7:     overlay width   (uint32_t LE)
//   b 8-11:    overlay height  (uint32_t LE)
//   b 12-15:   reserved
//
// consumer region:
//   b 16-19:   game width      (uint32_t LE)
//   b 20-23:   game height     (uint32_t LE)
//   b 24-27:   reserved
//   b 28-31:   reserved
//
// b 32-63: reserved for future use
// b 64+:   raw rgba pixel data (w*h*4b)

static constexpr size_t s_iShmHeaderSize = 64;
static constexpr uint32_t s_iShmMaxWidth = 3840;
static constexpr uint32_t s_iShmMaxHeight = 2160;
static constexpr size_t s_iShmPixelSize = (size_t)s_iShmMaxWidth * s_iShmMaxHeight * 4;
static constexpr size_t s_iShmTotalSize = s_iShmHeaderSize + s_iShmPixelSize;

static constexpr const char *s_szShmName = "/recar_overlay";

enum EShmStatus : uint8_t {
    SHM_IDLE = 0,
    SHM_WRITING = 1,
    SHM_READY = 2,
};

struct CShmHead {
    std::atomic<uint8_t> m_iStatus;
    uint8_t m_aPad0[3];
    uint32_t m_iOverlayWidth;
    uint32_t m_iOverlayHeight;
    uint32_t m_iReserved0;

    uint32_t m_iGameWidth;
    uint32_t m_iGameHeight;
    uint32_t m_iReserved1;
    uint32_t m_iReserved2;

    uint8_t m_aReserved3[32];
};

static_assert(sizeof(CShmHead) == s_iShmHeaderSize, "CShmHead must be 64 bytes");

class CTrns {
public:
    /*---------------------------------------------------------
     * FN: CTrns
     * DESC: constructor
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    CTrns() = default;

    /*---------------------------------------------------------
     * FN: ~CTrns
     * DESC: destructor
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    ~CTrns();

    CTrns(const CTrns &) = delete;
    auto operator=(const CTrns &) -> CTrns & = delete;

    /*---------------------------------------------------------
     * FN: bOpen
     * DESC: opens shared memory
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto bOpen() -> bool;

    /*---------------------------------------------------------
     * FN: vClose
     * DESC: closes shared memory
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vClose() -> void;

    /*---------------------------------------------------------
     * FN: bIsOpen
     * DESC: check if shm is open
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    inline auto bIsOpen() const -> bool { return m_pPtr != nullptr; }

    /*---------------------------------------------------------
     * FN: pTryAcquireFrame
     * DESC: atomically checks frame status and returns header
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto pTryAcquireFrame() -> CShmHead *;

    /*---------------------------------------------------------
     * FN: vSetGameResolution
     * DESC: writes resolution into shm
     * PARMS: iWidth, iHeight
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vSetGameResolution(uint32_t iWidth, uint32_t iHeight) -> void;

    /*---------------------------------------------------------
     * FN: pPixelData
     * DESC: returns ptr to pixel segment
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto pPixelData() const -> const uint8_t *;

private:
    void *m_pPtr = nullptr;
    int m_iFd = -1;
    bool m_bCreated = false;
};
} // namespace RV
