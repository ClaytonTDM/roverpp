// Created by Unium on 04.03.26

#include "rvDpDisp.hpp"
#include "rvOrRend.hpp"
#include "rvSkServ.hpp"

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RV {
/*---------------------------------------------------------
 * FN: onLibraryLoad
 * DESC: constructor
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
__attribute__((constructor)) static auto onLibraryLoad() -> void {
    fprintf(stderr, "[recar|roverpp] roverlaypp layer loaded\n");
}

/*---------------------------------------------------------
 * FN: onLibraryUnload
 * DESC: destructor
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
__attribute__((destructor)) static auto onLibraryUnload() -> void {
    fprintf(stderr, "[recar|roverpp] bye bye!\n");
    g_objServ.vStop();
}

#define VK_LAYER_EXPORT __attribute__((visibility("default")))

typedef auto(VKAPI_PTR *PFN_vkGetPhysicalDeviceProcAddr)(VkInstance hInstance, const char *pName) -> PFN_vkVoidFunction;

typedef enum EVkLayerFunction {
    VK_LAYER_LINK_INFO = 0,
    VK_LOADER_DATA_CALLBACK = 1,
    VK_LOADER_LAYER_CREATE_DEVICE_CALLBACK = 2,
    VK_LOADER_FEATURES = 3,
} EVkLayerFunction;

typedef struct CVkLayerInstanceLink {
    struct CVkLayerInstanceLink *pNext;
    PFN_vkGetInstanceProcAddr pNextGetInstanceProcAddr;
    PFN_vkGetPhysicalDeviceProcAddr pNextGetPhysicalDeviceProcAddr;
} CVkLayerInstanceLink;

typedef struct CVkLayerInstanceCreateInfo {
    VkStructureType sType;
    const void *pNext;
    EVkLayerFunction function;
    union {
        CVkLayerInstanceLink *pLayerInfo;
        void *pSetInstanceLoaderData;
    } u;
} CVkLayerInstanceCreateInfo;

typedef struct CVkLayerDeviceLink {
    struct CVkLayerDeviceLink *pNext;
    PFN_vkGetInstanceProcAddr pNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pNextGetDeviceProcAddr;
} CVkLayerDeviceLink;

typedef struct CVkLayerDeviceCreateInfo {
    VkStructureType sType;
    const void *pNext;
    EVkLayerFunction function;
    union {
        CVkLayerDeviceLink *pLayerInfo;
        void *pSetDeviceLoaderData;
    } u;
} CVkLayerDeviceCreateInfo;

typedef enum EVkNegotiateLayerStructType {
    LAYER_NEGOTIATE_INTERFACE_STRUCT = 1,
} EVkNegotiateLayerStructType;

typedef struct CVkNegotiateLayerInterface {
    EVkNegotiateLayerStructType sType;
    void *pNext;
    uint32_t loaderLayerInterfaceVersion;
    PFN_vkGetInstanceProcAddr pGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pGetDeviceProcAddr;
    PFN_vkGetPhysicalDeviceProcAddr pGetPhysicalDeviceProcAddr;
} CVkNegotiateLayerInterface;

static auto VKAPI_CALL recar_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) -> VkResult;
static auto VKAPI_CALL recar_DestroyInstance(VkInstance hInstance, const VkAllocationCallbacks *pAllocator) -> void;
static auto VKAPI_CALL recar_CreateDevice(VkPhysicalDevice hPhysicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) -> VkResult;
static auto VKAPI_CALL recar_DestroyDevice(VkDevice hDevice, const VkAllocationCallbacks *pAllocator) -> void;
static auto VKAPI_CALL recar_CreateSwapchainKHR(VkDevice hDevice, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
    -> VkResult;
static auto VKAPI_CALL recar_DestroySwapchainKHR(VkDevice hDevice, VkSwapchainKHR hSwapchain,
                                                 const VkAllocationCallbacks *pAllocator) -> void;
static auto VKAPI_CALL recar_QueuePresentKHR(VkQueue hQueue, const VkPresentInfoKHR *pPresentInfo) -> VkResult;

static std::once_flag g_objInitOnce;
static bool g_bLayerActive = false;

static std::mutex g_objPhysDeviceMapLock;
static std::unordered_map<void *, VkInstance> g_mapPhysDeviceToInstance;

/*---------------------------------------------------------
 * FN: initLayer
 * DESC: layer core init step
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto initLayer() -> void {
    fprintf(stderr, "[recar|roverpp] layer init'ing...\n");
    g_objRend.bInitSharedMemory();
    g_objServ.bStart("/tmp/recar_overlay.sock");
    g_bLayerActive = true;
    fprintf(stderr, "[recar|roverpp] layer init'd successfully!!\n");
}

/*---------------------------------------------------------
 * FN: recar_CreateInstance
 * DESC: instance creation hook
 * PARMS: pCreateInfo, pAllocator, pInstance
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_CreateInstance(const VkInstanceCreateInfo *pCreateInfo,
                                            const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
    -> VkResult {
    auto *pChainInfo = (CVkLayerInstanceCreateInfo *)pCreateInfo->pNext;
    while (pChainInfo && !(pChainInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
                           pChainInfo->function == VK_LAYER_LINK_INFO)) {
        pChainInfo = (CVkLayerInstanceCreateInfo *)pChainInfo->pNext;
    }
    if (!pChainInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr pGetInstanceProcAddr = pChainInfo->u.pLayerInfo->pNextGetInstanceProcAddr;
    pChainInfo->u.pLayerInfo = pChainInfo->u.pLayerInfo->pNext;

    auto pCreateInstance = (PFN_vkCreateInstance)pGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    if (!pCreateInstance)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkResult iResult = pCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (iResult != VK_SUCCESS)
        return iResult;

    CInstDisp objDt = {};
    objDt.pGetInstanceProcAddr = pGetInstanceProcAddr;
#define LOAD_INSTANCE(fn) objDt.p##fn = (PFN_vk##fn)pGetInstanceProcAddr(*pInstance, "vk" #fn)
    LOAD_INSTANCE(DestroyInstance);
    LOAD_INSTANCE(EnumerateDeviceExtensionProperties);
    LOAD_INSTANCE(CreateDevice);
    LOAD_INSTANCE(EnumeratePhysicalDevices);
    LOAD_INSTANCE(GetPhysicalDeviceProperties);
    LOAD_INSTANCE(GetPhysicalDeviceMemoryProperties);
    LOAD_INSTANCE(GetPhysicalDeviceQueueFamilyProperties);
#undef LOAD_INSTANCE

    {
        std::lock_guard<std::mutex> objLock(g_objInstDispLock);
        g_mapInstDisp[pGetDispKey(*pInstance)] = objDt;
    }

    uint32_t iPhysDeviceCount = 0;
    objDt.pEnumeratePhysicalDevices(*pInstance, &iPhysDeviceCount, nullptr);
    std::vector<VkPhysicalDevice> vPhysDevices(iPhysDeviceCount);
    objDt.pEnumeratePhysicalDevices(*pInstance, &iPhysDeviceCount, vPhysDevices.data());

    {
        std::lock_guard<std::mutex> objLock(g_objPhysDeviceMapLock);
        for (auto hPd : vPhysDevices) {
            g_mapPhysDeviceToInstance[pGetDispKey(hPd)] = *pInstance;
        }
    }

    std::call_once(g_objInitOnce, initLayer);
    fprintf(stderr, "[recar|roverpp] CreateInstance succeeded\n");
    return VK_SUCCESS;
}

/*---------------------------------------------------------
 * FN: recar_DestroyInstance
 * DESC: inst destroy hook
 * PARMS: hInstance, pAllocator
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_DestroyInstance(VkInstance hInstance, const VkAllocationCallbacks *pAllocator) -> void {
    void *pKey = pGetDispKey(hInstance);
    CInstDisp *pDt = pGetInstDisp(hInstance);
    if (pDt)
        pDt->pDestroyInstance(hInstance, pAllocator);
    std::lock_guard<std::mutex> objLock(g_objInstDispLock);
    g_mapInstDisp.erase(pKey);
}

/*---------------------------------------------------------
 * FN: recar_CreateDevice
 * DESC: device create hook
 * PARMS: hPhysicalDevice, pCreateInfo, pAllocator, pDevice
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_CreateDevice(VkPhysicalDevice hPhysicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) -> VkResult {
    auto *pChainInfo = (CVkLayerDeviceCreateInfo *)pCreateInfo->pNext;
    while (pChainInfo && !(pChainInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
                           pChainInfo->function == VK_LAYER_LINK_INFO)) {
        pChainInfo = (CVkLayerDeviceCreateInfo *)pChainInfo->pNext;
    }
    if (!pChainInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    PFN_vkGetInstanceProcAddr pGetInstanceProcAddr = pChainInfo->u.pLayerInfo->pNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr pGetDeviceProcAddr = pChainInfo->u.pLayerInfo->pNextGetDeviceProcAddr;
    pChainInfo->u.pLayerInfo = pChainInfo->u.pLayerInfo->pNext;

    VkInstance hInstance = VK_NULL_HANDLE;

    {
        std::lock_guard<std::mutex> objLock(g_objPhysDeviceMapLock);
        auto objIt = g_mapPhysDeviceToInstance.find(pGetDispKey(hPhysicalDevice));
        if (objIt != g_mapPhysDeviceToInstance.end())
            hInstance = objIt->second;
    }

    auto pCreateDevice = (PFN_vkCreateDevice)pGetInstanceProcAddr(hInstance, "vkCreateDevice");
    VkResult iResult = pCreateDevice(hPhysicalDevice, pCreateInfo, pAllocator, pDevice);
    if (iResult != VK_SUCCESS)
        return iResult;

    CDevDisp objDt = {};
    objDt.hDevice = *pDevice;
    objDt.hPhysicalDevice = hPhysicalDevice;
    objDt.pGetDeviceProcAddr = pGetDeviceProcAddr;

#define LOAD_DEVICE(fn) objDt.p##fn = (PFN_vk##fn)pGetDeviceProcAddr(*pDevice, "vk" #fn)
    LOAD_DEVICE(DestroyDevice);
    LOAD_DEVICE(QueuePresentKHR);
    LOAD_DEVICE(CreateSwapchainKHR);
    LOAD_DEVICE(DestroySwapchainKHR);
    LOAD_DEVICE(GetSwapchainImagesKHR);
    LOAD_DEVICE(QueueSubmit);
    LOAD_DEVICE(QueueWaitIdle);
    LOAD_DEVICE(DeviceWaitIdle);
    LOAD_DEVICE(CreateCommandPool);
    LOAD_DEVICE(DestroyCommandPool);
    LOAD_DEVICE(AllocateCommandBuffers);
    LOAD_DEVICE(FreeCommandBuffers);
    LOAD_DEVICE(BeginCommandBuffer);
    LOAD_DEVICE(EndCommandBuffer);
    LOAD_DEVICE(CmdBeginRenderPass);
    LOAD_DEVICE(CmdEndRenderPass);
    LOAD_DEVICE(CmdBindPipeline);
    LOAD_DEVICE(CmdBindVertexBuffers);
    LOAD_DEVICE(CmdBindIndexBuffer);
    LOAD_DEVICE(CmdBindDescriptorSets);
    LOAD_DEVICE(CmdDrawIndexed);
    LOAD_DEVICE(CmdSetViewport);
    LOAD_DEVICE(CmdSetScissor);
    LOAD_DEVICE(CmdPipelineBarrier);
    LOAD_DEVICE(CmdCopyBufferToImage);
    LOAD_DEVICE(CreateRenderPass);
    LOAD_DEVICE(DestroyRenderPass);
    LOAD_DEVICE(CreateFramebuffer);
    LOAD_DEVICE(DestroyFramebuffer);
    LOAD_DEVICE(CreateImageView);
    LOAD_DEVICE(DestroyImageView);
    LOAD_DEVICE(CreateShaderModule);
    LOAD_DEVICE(DestroyShaderModule);
    LOAD_DEVICE(CreatePipelineLayout);
    LOAD_DEVICE(DestroyPipelineLayout);
    LOAD_DEVICE(CreateGraphicsPipelines);
    LOAD_DEVICE(DestroyPipeline);
    LOAD_DEVICE(CreateBuffer);
    LOAD_DEVICE(DestroyBuffer);
    LOAD_DEVICE(CreateImage);
    LOAD_DEVICE(DestroyImage);
    LOAD_DEVICE(AllocateMemory);
    LOAD_DEVICE(FreeMemory);
    LOAD_DEVICE(BindBufferMemory);
    LOAD_DEVICE(BindImageMemory);
    LOAD_DEVICE(MapMemory);
    LOAD_DEVICE(UnmapMemory);
    LOAD_DEVICE(GetBufferMemoryRequirements);
    LOAD_DEVICE(GetImageMemoryRequirements);
    LOAD_DEVICE(CreateSampler);
    LOAD_DEVICE(DestroySampler);
    LOAD_DEVICE(CreateDescriptorSetLayout);
    LOAD_DEVICE(DestroyDescriptorSetLayout);
    LOAD_DEVICE(CreateDescriptorPool);
    LOAD_DEVICE(DestroyDescriptorPool);
    LOAD_DEVICE(AllocateDescriptorSets);
    LOAD_DEVICE(UpdateDescriptorSets);
    LOAD_DEVICE(CreateSemaphore);
    LOAD_DEVICE(DestroySemaphore);
    LOAD_DEVICE(CreateFence);
    LOAD_DEVICE(DestroyFence);
    LOAD_DEVICE(WaitForFences);
    LOAD_DEVICE(ResetFences);
    LOAD_DEVICE(ResetCommandBuffer);
    LOAD_DEVICE(ResetCommandPool);
    LOAD_DEVICE(CmdPushConstants);
#undef LOAD_DEVICE

    CInstDisp *pIdt = pGetInstDisp(hInstance);
    if (pIdt) {
        uint32_t iQueueFamilyCount = 0;
        pIdt->pGetPhysicalDeviceQueueFamilyProperties(hPhysicalDevice, &iQueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> vQueueFamilies(iQueueFamilyCount);
        pIdt->pGetPhysicalDeviceQueueFamilyProperties(hPhysicalDevice, &iQueueFamilyCount, vQueueFamilies.data());

        objDt.iGraphicsQueueFamily = 0;
        for (uint32_t i = 0; i < iQueueFamilyCount; i++) {
            if (vQueueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                objDt.iGraphicsQueueFamily = i;
                break;
            }
        }

        pIdt->pGetPhysicalDeviceMemoryProperties(hPhysicalDevice, &objDt.objMemoryProperties);
    }

    auto pGetDeviceQueue = (PFN_vkGetDeviceQueue)pGetDeviceProcAddr(*pDevice, "vkGetDeviceQueue");
    if (pGetDeviceQueue) {
        pGetDeviceQueue(*pDevice, objDt.iGraphicsQueueFamily, 0, &objDt.hGraphicsQueue);
    }

    {
        std::lock_guard<std::mutex> objLock(g_objDevDispLock);
        g_mapDevDisp[pGetDispKey(*pDevice)] = objDt;
    }

    fprintf(stderr, "[recar|roverpp] CreateDevice succeeded (queue family %u)\n", objDt.iGraphicsQueueFamily);
    return VK_SUCCESS;
}

/*---------------------------------------------------------
 * FN: recar_DestroyDevice
 * DESC: device destory hook
 * PARMS: hDevice, pAllocator
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_DestroyDevice(VkDevice hDevice, const VkAllocationCallbacks *pAllocator) -> void {
    void *pKey = pGetDispKey(hDevice);
    CDevDisp *pDt = pGetDevDisp(hDevice);
    if (pDt)
        pDt->pDestroyDevice(hDevice, pAllocator);
    std::lock_guard<std::mutex> objLock(g_objDevDispLock);
    g_mapDevDisp.erase(pKey);
}

/*---------------------------------------------------------
 * FN: recar_CreateSwapchainKHR
 * DESC: swapchain creation hook
 * PARMS: hDevice, pCreateInfo, pAllocator, pSwapchain
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_CreateSwapchainKHR(VkDevice hDevice, const VkSwapchainCreateInfoKHR *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
    -> VkResult {
    CDevDisp *pDt = pGetDevDisp(hDevice);
    if (!pDt)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkResult iResult = pDt->pCreateSwapchainKHR(hDevice, pCreateInfo, pAllocator, pSwapchain);
    if (iResult != VK_SUCCESS)
        return iResult;

    if (g_bLayerActive) {
        g_objRend.vCreateSwapchainResources(pDt, *pSwapchain, pCreateInfo);
    }

    return VK_SUCCESS;
}

/*---------------------------------------------------------
 * FN: recar_DestroySwapchainKHR
 * DESC: swapchain destroy hook
 * PARMS: hDevice, hSwapchain, pAllocator
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_DestroySwapchainKHR(VkDevice hDevice, VkSwapchainKHR hSwapchain,
                                                 const VkAllocationCallbacks *pAllocator) -> void {
    CDevDisp *pDt = pGetDevDisp(hDevice);
    if (!pDt)
        return;

    if (g_bLayerActive) {
        g_objRend.vDestroySwapchainResources(pDt, hSwapchain);
    }

    pDt->pDestroySwapchainKHR(hDevice, hSwapchain, pAllocator);
}

/*---------------------------------------------------------
 * FN: recar_QueuePresentKHR
 * DESC: presentation frame boundary hook
 * PARMS: hQueue, pPresentInfo
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_QueuePresentKHR(VkQueue hQueue, const VkPresentInfoKHR *pPresentInfo) -> VkResult {
    CDevDisp *pDt = pGetDevDisp(hQueue);
    if (!pDt || !g_bLayerActive) {
        if (pDt)
            return pDt->pQueuePresentKHR(hQueue, pPresentInfo);
        return VK_ERROR_DEVICE_LOST;
    }

    std::vector<VkSemaphore> vFinalWaitSemaphores(pPresentInfo->pWaitSemaphores,
                                                  pPresentInfo->pWaitSemaphores + pPresentInfo->waitSemaphoreCount);

    for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++) {
        VkSwapchainKHR hSwapchain = pPresentInfo->pSwapchains[i];
        uint32_t iImageIndex = pPresentInfo->pImageIndices[i];

        VkSemaphore hWaitSem = VK_NULL_HANDLE;
        if (i == 0 && pPresentInfo->waitSemaphoreCount > 0) {
            hWaitSem = pPresentInfo->pWaitSemaphores[0];
        }

        VkSemaphore hSignalSem = VK_NULL_HANDLE;
        g_objRend.vRender(pDt, hQueue, hSwapchain, iImageIndex, hWaitSem, &hSignalSem);

        if (hSignalSem != VK_NULL_HANDLE) {
            if (i == 0 && !vFinalWaitSemaphores.empty()) {
                vFinalWaitSemaphores[0] = hSignalSem;
            } else {
                vFinalWaitSemaphores.push_back(hSignalSem);
            }
        }
    }

    VkPresentInfoKHR objModifiedPresentInfo = *pPresentInfo;
    objModifiedPresentInfo.waitSemaphoreCount = (uint32_t)vFinalWaitSemaphores.size();
    objModifiedPresentInfo.pWaitSemaphores = vFinalWaitSemaphores.data();

    return pDt->pQueuePresentKHR(hQueue, &objModifiedPresentInfo);
}

/*---------------------------------------------------------
 * FN: recar_GetDeviceProcAddr
 * DESC: fetches hooked or target api dev pointer
 * PARMS: hDevice, pName
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_GetDeviceProcAddr(VkDevice hDevice, const char *pName) -> PFN_vkVoidFunction {
    if (strcmp(pName, "vkDestroyDevice") == 0)
        return (PFN_vkVoidFunction)recar_DestroyDevice;
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
        return (PFN_vkVoidFunction)recar_CreateSwapchainKHR;
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0)
        return (PFN_vkVoidFunction)recar_DestroySwapchainKHR;
    if (strcmp(pName, "vkQueuePresentKHR") == 0)
        return (PFN_vkVoidFunction)recar_QueuePresentKHR;
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0)
        return (PFN_vkVoidFunction)recar_GetDeviceProcAddr;
    CDevDisp *pDt = pGetDevDisp(hDevice);
    if (pDt)
        return pDt->pGetDeviceProcAddr(hDevice, pName);
    return nullptr;
}

/*---------------------------------------------------------
 * FN: recar_GetInstanceProcAddr
 * DESC: fetches hooked or target api inst pointer
 * PARMS: hInstance, pName
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto VKAPI_CALL recar_GetInstanceProcAddr(VkInstance hInstance, const char *pName) -> PFN_vkVoidFunction {
    if (strcmp(pName, "vkCreateInstance") == 0)
        return (PFN_vkVoidFunction)recar_CreateInstance;
    if (strcmp(pName, "vkDestroyInstance") == 0)
        return (PFN_vkVoidFunction)recar_DestroyInstance;
    if (strcmp(pName, "vkCreateDevice") == 0)
        return (PFN_vkVoidFunction)recar_CreateDevice;
    if (strcmp(pName, "vkGetInstanceProcAddr") == 0)
        return (PFN_vkVoidFunction)recar_GetInstanceProcAddr;
    if (strcmp(pName, "vkDestroyDevice") == 0)
        return (PFN_vkVoidFunction)recar_DestroyDevice;
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
        return (PFN_vkVoidFunction)recar_CreateSwapchainKHR;
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0)
        return (PFN_vkVoidFunction)recar_DestroySwapchainKHR;
    if (strcmp(pName, "vkQueuePresentKHR") == 0)
        return (PFN_vkVoidFunction)recar_QueuePresentKHR;
    if (strcmp(pName, "vkGetDeviceProcAddr") == 0)
        return (PFN_vkVoidFunction)recar_GetDeviceProcAddr;
    CInstDisp *pDt = pGetInstDisp(hInstance);
    if (pDt)
        return pDt->pGetInstanceProcAddr(hInstance, pName);
    return nullptr;
}

} // namespace RV

extern "C" {

/*---------------------------------------------------------
 * FN: vkNegotiateLoaderLayerInterfaceVersion
 * DESC: negotiates loader layer interface
 * PARMS: pVersionStruct
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
VK_LAYER_EXPORT auto VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(RV::CVkNegotiateLayerInterface *pVersionStruct)
    -> VkResult {
    fprintf(stderr, "[recar|roverpp] vkNegotiateLoaderLayerInterfaceVersion called\n");
    if (!pVersionStruct)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (pVersionStruct->sType != RV::LAYER_NEGOTIATE_INTERFACE_STRUCT)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (pVersionStruct->loaderLayerInterfaceVersion >= 2) {
        pVersionStruct->pGetInstanceProcAddr = RV::recar_GetInstanceProcAddr;
        pVersionStruct->pGetDeviceProcAddr = RV::recar_GetDeviceProcAddr;
        pVersionStruct->pGetPhysicalDeviceProcAddr = nullptr;
        pVersionStruct->loaderLayerInterfaceVersion = 2;
    }

    fprintf(stderr, "[recar|roverpp] negotiation successful!!\n");
    return VK_SUCCESS;
}

/*---------------------------------------------------------
 * FN: vkGetInstanceProcAddr
 * DESC: primary module external proc interface
 * PARMS: hInstance, pName
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
VK_LAYER_EXPORT auto VKAPI_CALL vkGetInstanceProcAddr(VkInstance hInstance, const char *pName) -> PFN_vkVoidFunction {
    return RV::recar_GetInstanceProcAddr(hInstance, pName);
}

/*---------------------------------------------------------
 * FN: vkGetDeviceProcAddr
 * DESC: device proc external interface
 * PARMS: hDevice, pName
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
VK_LAYER_EXPORT auto VKAPI_CALL vkGetDeviceProcAddr(VkDevice hDevice, const char *pName) -> PFN_vkVoidFunction {
    return RV::recar_GetDeviceProcAddr(hDevice, pName);
}
} // namespace RV
