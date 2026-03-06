// Created by Unium on 04.03.26

#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace RV {
class CServ {
public:
    /*---------------------------------------------------------
     * FN: CServ
     * DESC: constructor
     * PARMS: none
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    CServ();

    /*---------------------------------------------------------
     * FN: ~CServ
     * DESC: destructor
     * PARMS: none
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    ~CServ();

    /*---------------------------------------------------------
     * FN: bStart
     * DESC: starts the unix socket server
     * PARMS: szSocketPath (path to socket)
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    auto bStart(const std::string &szSocketPath = "/tmp/recar_overlay.sock") -> bool;

    /*---------------------------------------------------------
     * FN: vStop
     * DESC: stops the unix socket server
     * PARMS: none
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    auto vStop() -> void;

    /*---------------------------------------------------------
     * FN: bConsumeFrameSignal
     * DESC: consumes pending frame signals
     * PARMS: none
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    auto bConsumeFrameSignal() -> bool;

private:
    /*---------------------------------------------------------
     * FN: vServerLoop
     * DESC: thread loop processing incoming connections
     * PARMS: none
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    auto vServerLoop() -> void;

    /*---------------------------------------------------------
     * FN: vHandleMessage
     * DESC: process message string
     * PARMS: szMsg (string to process)
     * AUTH: unium (04.03.26)
     *-------------------------------------------------------*/
    auto vHandleMessage(const std::string &szMsg) -> void;

    std::thread m_objThread;
    std::atomic<bool> m_bRunning{false};
    std::atomic<bool> m_bFrameReady{false};
    int m_iServerFd = -1;
    std::string m_szSocketPath;
};

extern CServ g_objServ;
} // namespace RV
