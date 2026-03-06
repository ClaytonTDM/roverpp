// Created by Unium on 04.03.26

#include "rvSkServ.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace RV {
CServ g_objServ;

/*---------------------------------------------------------
 * FN: CServ
 * DESC: constructor
 * PARMS: none
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
CServ::CServ() {}

/*---------------------------------------------------------
 * FN: ~CServ
 * DESC: destructor
 * PARMS: none
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
CServ::~CServ() { vStop(); }

/*---------------------------------------------------------
 * FN: bStart
 * DESC: starts the unix socket server
 * PARMS: szSocketPath (path to socket)
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
auto CServ::bStart(const std::string &szSocketPath) -> bool {
    m_szSocketPath = szSocketPath;
    unlink(m_szSocketPath.c_str());

    m_iServerFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_iServerFd < 0) {
        perror("[recar|roverpp] socket");
        return false;
    }

    struct sockaddr_un objAddr;
    memset(&objAddr, 0, sizeof(objAddr));
    objAddr.sun_family = AF_UNIX;
    strncpy(objAddr.sun_path, m_szSocketPath.c_str(), sizeof(objAddr.sun_path) - 1);

    if (bind(m_iServerFd, (struct sockaddr *)&objAddr, sizeof(objAddr)) < 0) {
        perror("[recar|roverpp] bind");
        close(m_iServerFd);
        m_iServerFd = -1;
        return false;
    }

    if (listen(m_iServerFd, 5) < 0) {
        perror("[recar|roverpp] listen");
        close(m_iServerFd);
        m_iServerFd = -1;
        return false;
    }

    m_bRunning = true;
    m_objThread = std::thread(&CServ::vServerLoop, this);

    fprintf(stderr, "[recar|roverpp] socket server started at %s\n", m_szSocketPath.c_str());
    return true;
}

/*---------------------------------------------------------
 * FN: vStop
 * DESC: stops the unix socket server
 * PARMS: none
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
auto CServ::vStop() -> void {
    m_bRunning = false;
    if (m_iServerFd >= 0) {
        shutdown(m_iServerFd, SHUT_RDWR);
        close(m_iServerFd);
        m_iServerFd = -1;
    }
    if (m_objThread.joinable()) {
        m_objThread.join();
    }
    unlink(m_szSocketPath.c_str());
}

/*---------------------------------------------------------
 * FN: bConsumeFrameSignal
 * DESC: consumes pending frame signals
 * PARMS: none
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
auto CServ::bConsumeFrameSignal() -> bool { return m_bFrameReady.exchange(false, std::memory_order_acquire); }

/*---------------------------------------------------------
 * FN: vServerLoop
 * DESC: thread loop processing incoming connections
 * PARMS: none
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
auto CServ::vServerLoop() -> void {
    while (m_bRunning) {
        struct pollfd objPfd;
        objPfd.fd = m_iServerFd;
        objPfd.events = POLLIN;
        objPfd.revents = 0;

        int iRet = poll(&objPfd, 1, 200);
        if (iRet <= 0)
            continue;
        if (!(objPfd.revents & POLLIN))
            continue;

        int iClientFd = accept(m_iServerFd, nullptr, nullptr);
        if (iClientFd < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("[recar|roverpp] accept");
            }
            continue;
        }

        int iFlags = fcntl(iClientFd, F_GETFL, 0);
        if (iFlags >= 0) {
            fcntl(iClientFd, F_SETFL, iFlags | O_NONBLOCK);
        }

        char aBuf[1024];
        std::string szMessage;
        int iReadTimeout = 500;

        while (iReadTimeout > 0) {
            struct pollfd objCpfd;
            objCpfd.fd = iClientFd;
            objCpfd.events = POLLIN;
            objCpfd.revents = 0;

            int iR = poll(&objCpfd, 1, 50);
            if (iR > 0 && (objCpfd.revents & POLLIN)) {
                ssize_t iN = read(iClientFd, aBuf, sizeof(aBuf) - 1);
                if (iN <= 0)
                    break;
                aBuf[iN] = '\0';
                szMessage.append(aBuf, (size_t)iN);
                iReadTimeout = 100;
            } else if (iR == 0) {
                if (!szMessage.empty())
                    break;
                iReadTimeout -= 50;
            } else {
                break;
            }
        }

        close(iClientFd);

        if (!szMessage.empty()) {
            vHandleMessage(szMessage);
        }
    }
}

/*---------------------------------------------------------
 * FN: vHandleMessage
 * DESC: process message string
 * PARMS: szMsg (string to process)
 * AUTH: unium (04.03.26)
 *-------------------------------------------------------*/
auto CServ::vHandleMessage(const std::string &szMsg) -> void {
    if (szMsg.find("FRAME_UPDATE") != std::string::npos) {
        m_bFrameReady.store(true, std::memory_order_release);
        return;
    }

    fprintf(stderr, "[recar|roverpp] unknown message: %.200s\n", szMsg.c_str());
}
} // namespace RV
