// Created by Unium on 04.03.26

#pragma once

#include "rvDpDisp.hpp"
#include "rvStTrns.hpp"
#include <vulkan/vulkan.h>

#include <mutex>
#include <unordered_map>
#include <vector>

namespace RV {
struct COverlayVertex {
    float m_aPos[2];
    float m_aUv[2];
};

struct COverlayPushConstants {
    float m_aUvScale[2];
};

struct CSwapData {
    struct CPerFrame {
        VkSemaphore m_hRenderSemaphore = VK_NULL_HANDLE;
        bool m_bSubmitted = false;
    };

    VkSwapchainKHR m_hSwapchain = VK_NULL_HANDLE;
    VkExtent2D m_objExtent = {};
    VkFormat m_iFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> m_vImages;
    std::vector<VkImageView> m_vImageViews;
    std::vector<VkFramebuffer> m_vFramebuffers;
    std::vector<VkCommandBuffer> m_vCommandBuffers;
    std::vector<VkFence> m_vFences;

    VkRenderPass m_hRenderPass = VK_NULL_HANDLE;
    VkCommandPool m_hCommandPool = VK_NULL_HANDLE;
    VkPipelineLayout m_hPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_hPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_hDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_hDescriptorPool = VK_NULL_HANDLE;
    VkSampler m_hSampler = VK_NULL_HANDLE;

    VkImage m_hOverlayImage = VK_NULL_HANDLE;
    VkDeviceMemory m_hOverlayImageMemory = VK_NULL_HANDLE;
    VkImageView m_hOverlayImageView = VK_NULL_HANDLE;
    VkDescriptorSet m_hDescriptorSet = VK_NULL_HANDLE;

    VkBuffer m_hStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hStagingMemory = VK_NULL_HANDLE;
    size_t m_iStagingCapacity = 0;

    VkBuffer m_hQuadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hQuadVertexMemory = VK_NULL_HANDLE;
    VkBuffer m_hQuadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_hQuadIndexMemory = VK_NULL_HANDLE;

    std::vector<CPerFrame> m_vPerFrame;

    uint32_t m_iOverlayWidth = 0;
    uint32_t m_iOverlayHeight = 0;
    bool m_bOverlayImageReady = false;
    bool m_bHasContent = false;
    bool m_bInitialized = false;
};

class CRend {
public:
    /*---------------------------------------------------------
     * FN: vCreateSwapchainResources
     * DESC: creates render resources
     * PARMS: pDt, hSwapchain, pCreateInfo
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vCreateSwapchainResources(CDevDisp *pDt, VkSwapchainKHR hSwapchain,
                                   const VkSwapchainCreateInfoKHR *pCreateInfo) -> void;

    /*---------------------------------------------------------
     * FN: vDestroySwapchainResources
     * DESC: destroys resources
     * PARMS: pDt, hSwapchain
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vDestroySwapchainResources(CDevDisp *pDt, VkSwapchainKHR hSwapchain) -> void;

    /*---------------------------------------------------------
     * FN: vRender
     * DESC: renders to given swapchain img
     * PARMS: pDt, hQueue, hSwapchain, iImageIndex, hWaitSemaphore, pSignalSemaphore
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vRender(CDevDisp *pDt, VkQueue hQueue, VkSwapchainKHR hSwapchain, uint32_t iImageIndex,
                 VkSemaphore hWaitSemaphore, VkSemaphore *pSignalSemaphore) -> void;

    /*---------------------------------------------------------
     * FN: bInitSharedMemory
     * DESC: inits the shm transport
     * PARMS: none
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto bInitSharedMemory() -> bool;

private:
    /*---------------------------------------------------------
     * FN: vCreateRenderPass
     * DESC: local wrapper to create vkrenderpass
     * PARMS: pDt, objData
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vCreateRenderPass(CDevDisp *pDt, CSwapData &objData) -> void;

    /*---------------------------------------------------------
     * FN: vCreatePipeline
     * DESC: local wrapper to create graphics pipeline
     * PARMS: pDt, objData
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vCreatePipeline(CDevDisp *pDt, CSwapData &objData) -> void;

    /*---------------------------------------------------------
     * FN: vCreateOverlayImage
     * DESC: instantiates image mapped to overlay
     * PARMS: pDt, objData
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vCreateOverlayImage(CDevDisp *pDt, CSwapData &objData) -> void;

    /*---------------------------------------------------------
     * FN: vCreateQuadBuffers
     * DESC: instantiates the vertex/index buffer quad
     * PARMS: pDt, objData
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vCreateQuadBuffers(CDevDisp *pDt, CSwapData &objData) -> void;

    /*---------------------------------------------------------
     * FN: iFindMemoryType
     * DESC: fetches target device mem
     * PARMS: pDt, iTypeBits, iProperties
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto iFindMemoryType(CDevDisp *pDt, uint32_t iTypeBits, VkMemoryPropertyFlags iProperties) -> uint32_t;

    /*---------------------------------------------------------
     * FN: vEnsureStagingBuffer
     * DESC: allocates staging buffer per capacity
     * PARMS: pDt, objData, iRequiredSize
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto vEnsureStagingBuffer(CDevDisp *pDt, CSwapData &objData, size_t iRequiredSize) -> void;

    /*---------------------------------------------------------
     * FN: bUploadOverlayTexture
     * DESC: pushes pixels to gpu
     * PARMS: pDt, objData, hCmd, pPixels, iWidth, iHeight
     * AUTH: unium (06.03.26)
     *-------------------------------------------------------*/
    auto bUploadOverlayTexture(CDevDisp *pDt, CSwapData &objData, VkCommandBuffer hCmd, const uint8_t *pPixels,
                               uint32_t iWidth, uint32_t iHeight) -> bool;

    std::chrono::steady_clock::time_point m_objLastSwapchainCreate{};
    std::mutex m_objMtx;
    std::unordered_map<VkSwapchainKHR, CSwapData> m_mapSwapchains;
    CTrns m_objShm;
    bool m_bShmOpen = false;
};

extern CRend g_objRend;
} // namespace RV
