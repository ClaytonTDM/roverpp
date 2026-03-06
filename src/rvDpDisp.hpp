// Created by Unium on 04.03.26

#pragma once

#include <mutex>
#include <unordered_map>
#include <vulkan/vulkan.h>

namespace RV {
/*---------------------------------------------------------
 * FN: pGetDispKey
 * DESC: retrieves dispatch key from vulkan object handle
 * PARMS: pObj (void pointer to vulkan handle)
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
inline auto pGetDispKey(void *pObj) -> void * { return *reinterpret_cast<void **>(pObj); }

struct CInstDisp {
    PFN_vkGetInstanceProcAddr pGetInstanceProcAddr = nullptr;
    PFN_vkDestroyInstance pDestroyInstance = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties pEnumerateDeviceExtensionProperties = nullptr;
    PFN_vkCreateDevice pCreateDevice = nullptr;
    PFN_vkEnumeratePhysicalDevices pEnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties pGetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties pGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties pGetPhysicalDeviceQueueFamilyProperties = nullptr;
};

struct CDevDisp {
    VkDevice hDevice = VK_NULL_HANDLE;
    VkPhysicalDevice hPhysicalDevice = VK_NULL_HANDLE;
    VkQueue hGraphicsQueue = VK_NULL_HANDLE;
    uint32_t iGraphicsQueueFamily = 0;
    VkPhysicalDeviceMemoryProperties objMemoryProperties = {};

    PFN_vkGetDeviceProcAddr pGetDeviceProcAddr = nullptr;
    PFN_vkDestroyDevice pDestroyDevice = nullptr;
    PFN_vkQueuePresentKHR pQueuePresentKHR = nullptr;
    PFN_vkCreateSwapchainKHR pCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR pDestroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR pGetSwapchainImagesKHR = nullptr;
    PFN_vkQueueSubmit pQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle pQueueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle pDeviceWaitIdle = nullptr;
    PFN_vkCreateCommandPool pCreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool pDestroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers pAllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers pFreeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer pBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer pEndCommandBuffer = nullptr;
    PFN_vkCmdBeginRenderPass pCmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass pCmdEndRenderPass = nullptr;
    PFN_vkCmdBindPipeline pCmdBindPipeline = nullptr;
    PFN_vkCmdBindVertexBuffers pCmdBindVertexBuffers = nullptr;
    PFN_vkCmdBindIndexBuffer pCmdBindIndexBuffer = nullptr;
    PFN_vkCmdBindDescriptorSets pCmdBindDescriptorSets = nullptr;
    PFN_vkCmdDrawIndexed pCmdDrawIndexed = nullptr;
    PFN_vkCmdSetViewport pCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor pCmdSetScissor = nullptr;
    PFN_vkCmdPipelineBarrier pCmdPipelineBarrier = nullptr;
    PFN_vkCmdCopyBufferToImage pCmdCopyBufferToImage = nullptr;
    PFN_vkCreateRenderPass pCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass pDestroyRenderPass = nullptr;
    PFN_vkCreateFramebuffer pCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer pDestroyFramebuffer = nullptr;
    PFN_vkCreateImageView pCreateImageView = nullptr;
    PFN_vkDestroyImageView pDestroyImageView = nullptr;
    PFN_vkCreateShaderModule pCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule pDestroyShaderModule = nullptr;
    PFN_vkCreatePipelineLayout pCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout pDestroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines pCreateGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline pDestroyPipeline = nullptr;
    PFN_vkCreateBuffer pCreateBuffer = nullptr;
    PFN_vkDestroyBuffer pDestroyBuffer = nullptr;
    PFN_vkCreateImage pCreateImage = nullptr;
    PFN_vkDestroyImage pDestroyImage = nullptr;
    PFN_vkAllocateMemory pAllocateMemory = nullptr;
    PFN_vkFreeMemory pFreeMemory = nullptr;
    PFN_vkBindBufferMemory pBindBufferMemory = nullptr;
    PFN_vkBindImageMemory pBindImageMemory = nullptr;
    PFN_vkMapMemory pMapMemory = nullptr;
    PFN_vkUnmapMemory pUnmapMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements pGetBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements pGetImageMemoryRequirements = nullptr;
    PFN_vkCreateSampler pCreateSampler = nullptr;
    PFN_vkDestroySampler pDestroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout pCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout pDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool pCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool pDestroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets pAllocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets pUpdateDescriptorSets = nullptr;
    PFN_vkCreateSemaphore pCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore pDestroySemaphore = nullptr;
    PFN_vkCreateFence pCreateFence = nullptr;
    PFN_vkDestroyFence pDestroyFence = nullptr;
    PFN_vkWaitForFences pWaitForFences = nullptr;
    PFN_vkResetFences pResetFences = nullptr;
    PFN_vkResetCommandBuffer pResetCommandBuffer = nullptr;
    PFN_vkResetCommandPool pResetCommandPool = nullptr;
    PFN_vkCmdPushConstants pCmdPushConstants = nullptr;
};

extern std::mutex g_objInstDispLock;
extern std::unordered_map<void *, CInstDisp> g_mapInstDisp;

extern std::mutex g_objDevDispLock;
extern std::unordered_map<void *, CDevDisp> g_mapDevDisp;

/*---------------------------------------------------------
 * FN: pGetInstDisp
 * DESC: retrieves instance dispatch table for object
 * PARMS: pObj (void pointer to vulkan handle)
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
inline auto pGetInstDisp(void *pObj) -> CInstDisp * {
    std::lock_guard<std::mutex> objLock(g_objInstDispLock);
    auto objIt = g_mapInstDisp.find(pGetDispKey(pObj));
    return (objIt != g_mapInstDisp.end()) ? &objIt->second : nullptr;
}

/*---------------------------------------------------------
 * FN: pGetDevDisp
 * DESC: retrieves device dispatch table for object
 * PARMS: pObj (void pointer to vulkan handle)
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
inline auto pGetDevDisp(void *pObj) -> CDevDisp * {
    std::lock_guard<std::mutex> objLock(g_objDevDispLock);
    auto objIt = g_mapDevDisp.find(pGetDispKey(pObj));
    return (objIt != g_mapDevDisp.end()) ? &objIt->second : nullptr;
}
} // namespace RV
