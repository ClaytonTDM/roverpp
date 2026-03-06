// Created by Unium on 04.03.26

#include "rvOrRend.hpp"
#include "rvShShdr.hpp"
#include "rvSkServ.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace RV {
CRend g_objRend;

/*---------------------------------------------------------
 * FN: vInitDispatchKey
 * DESC: initializes dispatcher key locally
 * PARMS: hCmd, hDevice
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
static auto vInitDispatchKey(VkCommandBuffer hCmd, VkDevice hDevice) -> void {
    if (hCmd && hDevice) {
        *reinterpret_cast<void **>(hCmd) = *reinterpret_cast<void **>(hDevice);
    }
}

/*---------------------------------------------------------
 * FN: iFindMemoryType
 * DESC: fetches target device mem
 * PARMS: pDt, iTypeBits, iProperties
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::iFindMemoryType(CDevDisp *pDt, uint32_t iTypeBits, VkMemoryPropertyFlags iProperties) -> uint32_t {
    for (uint32_t i = 0; i < pDt->objMemoryProperties.memoryTypeCount; i++) {
        if ((iTypeBits & (1 << i)) &&
            (pDt->objMemoryProperties.memoryTypes[i].propertyFlags & iProperties) == iProperties) {
            return i;
        }
    }
    return 0;
}

/*---------------------------------------------------------
 * FN: vEnsureStagingBuffer
 * DESC: allocates staging buffer per capacity
 * PARMS: pDt, objData, iRequiredSize
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vEnsureStagingBuffer(CDevDisp *pDt, CSwapData &objData, size_t iRequiredSize) -> void {
    if (objData.m_hStagingBuffer != VK_NULL_HANDLE && objData.m_iStagingCapacity >= iRequiredSize)
        return;

    if (objData.m_hStagingBuffer != VK_NULL_HANDLE) {
        pDt->pDestroyBuffer(pDt->hDevice, objData.m_hStagingBuffer, nullptr);
        pDt->pFreeMemory(pDt->hDevice, objData.m_hStagingMemory, nullptr);
        objData.m_hStagingBuffer = VK_NULL_HANDLE;
        objData.m_hStagingMemory = VK_NULL_HANDLE;
    }

    size_t iCapacity = std::max(iRequiredSize, s_iShmPixelSize);

    VkBufferCreateInfo objBufInfo = {};
    objBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    objBufInfo.size = iCapacity;
    objBufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    objBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    pDt->pCreateBuffer(pDt->hDevice, &objBufInfo, nullptr, &objData.m_hStagingBuffer);

    VkMemoryRequirements objMemReq;
    pDt->pGetBufferMemoryRequirements(pDt->hDevice, objData.m_hStagingBuffer, &objMemReq);

    VkMemoryAllocateInfo objAllocInfo = {};
    objAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    objAllocInfo.allocationSize = objMemReq.size;
    objAllocInfo.memoryTypeIndex = iFindMemoryType(
        pDt, objMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    pDt->pAllocateMemory(pDt->hDevice, &objAllocInfo, nullptr, &objData.m_hStagingMemory);
    pDt->pBindBufferMemory(pDt->hDevice, objData.m_hStagingBuffer, objData.m_hStagingMemory, 0);

    objData.m_iStagingCapacity = iCapacity;
}

/*---------------------------------------------------------
 * FN: bInitSharedMemory
 * DESC: inits the shm transport
 * PARMS: none
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::bInitSharedMemory() -> bool {
    if (m_bShmOpen)
        return true;
    m_bShmOpen = m_objShm.bOpen();
    return m_bShmOpen;
}

/*---------------------------------------------------------
 * FN: vCreateRenderPass
 * DESC: local wrapper to create vkrenderpass
 * PARMS: pDt, objData
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vCreateRenderPass(CDevDisp *pDt, CSwapData &objData) -> void {
    VkAttachmentDescription objAttachment = {};
    objAttachment.format = objData.m_iFormat;
    objAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    objAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    objAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    objAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    objAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    objAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    objAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference objColorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription objSubpass = {};
    objSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    objSubpass.colorAttachmentCount = 1;
    objSubpass.pColorAttachments = &objColorRef;

    VkSubpassDependency objDep = {};
    objDep.srcSubpass = VK_SUBPASS_EXTERNAL;
    objDep.dstSubpass = 0;
    objDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    objDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    objDep.srcAccessMask = 0;
    objDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo objRpInfo = {};
    objRpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    objRpInfo.attachmentCount = 1;
    objRpInfo.pAttachments = &objAttachment;
    objRpInfo.subpassCount = 1;
    objRpInfo.pSubpasses = &objSubpass;
    objRpInfo.dependencyCount = 1;
    objRpInfo.pDependencies = &objDep;

    pDt->pCreateRenderPass(pDt->hDevice, &objRpInfo, nullptr, &objData.m_hRenderPass);
}

/*---------------------------------------------------------
 * FN: vCreatePipeline
 * DESC: local wrapper to create graphics pipeline
 * PARMS: pDt, objData
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vCreatePipeline(CDevDisp *pDt, CSwapData &objData) -> void {
    VkDescriptorSetLayoutBinding objBinding = {};
    objBinding.binding = 0;
    objBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    objBinding.descriptorCount = 1;
    objBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo objDsLayoutInfo = {};
    objDsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    objDsLayoutInfo.bindingCount = 1;
    objDsLayoutInfo.pBindings = &objBinding;
    pDt->pCreateDescriptorSetLayout(pDt->hDevice, &objDsLayoutInfo, nullptr, &objData.m_hDescriptorSetLayout);

    VkDescriptorPoolSize objPoolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo objPoolInfo = {};
    objPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    objPoolInfo.maxSets = 1;
    objPoolInfo.poolSizeCount = 1;
    objPoolInfo.pPoolSizes = &objPoolSize;
    pDt->pCreateDescriptorPool(pDt->hDevice, &objPoolInfo, nullptr, &objData.m_hDescriptorPool);

    VkSamplerCreateInfo objSamplerInfo = {};
    objSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    objSamplerInfo.magFilter = VK_FILTER_LINEAR;
    objSamplerInfo.minFilter = VK_FILTER_LINEAR;
    objSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    objSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    objSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    objSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    pDt->pCreateSampler(pDt->hDevice, &objSamplerInfo, nullptr, &objData.m_hSampler);

    VkPushConstantRange objPushRange = {};
    objPushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    objPushRange.offset = 0;
    objPushRange.size = sizeof(COverlayPushConstants);

    VkPipelineLayoutCreateInfo objLayoutInfo = {};
    objLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    objLayoutInfo.setLayoutCount = 1;
    objLayoutInfo.pSetLayouts = &objData.m_hDescriptorSetLayout;
    objLayoutInfo.pushConstantRangeCount = 1;
    objLayoutInfo.pPushConstantRanges = &objPushRange;
    pDt->pCreatePipelineLayout(pDt->hDevice, &objLayoutInfo, nullptr, &objData.m_hPipelineLayout);

    VkShaderModuleCreateInfo objVertModuleInfo = {};
    objVertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    objVertModuleInfo.codeSize = s_iOverlayVertSpvSize;
    objVertModuleInfo.pCode = s_aOverlayVertSpv;
    VkShaderModule hVertModule;
    pDt->pCreateShaderModule(pDt->hDevice, &objVertModuleInfo, nullptr, &hVertModule);

    VkShaderModuleCreateInfo objFragModuleInfo = {};
    objFragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    objFragModuleInfo.codeSize = s_iOverlayFragSpvSize;
    objFragModuleInfo.pCode = s_aOverlayFragSpv;
    VkShaderModule hFragModule;
    pDt->pCreateShaderModule(pDt->hDevice, &objFragModuleInfo, nullptr, &hFragModule);

    VkPipelineShaderStageCreateInfo aStages[2] = {};
    aStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    aStages[0].module = hVertModule;
    aStages[0].pName = "main";
    aStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    aStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    aStages[1].module = hFragModule;
    aStages[1].pName = "main";

    VkVertexInputBindingDescription objVertexBinding = {0, sizeof(COverlayVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription aVertexAttribs[2] = {};
    aVertexAttribs[0] = {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(COverlayVertex, m_aPos)};
    aVertexAttribs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(COverlayVertex, m_aUv)};

    VkPipelineVertexInputStateCreateInfo objVertexInput = {};
    objVertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    objVertexInput.vertexBindingDescriptionCount = 1;
    objVertexInput.pVertexBindingDescriptions = &objVertexBinding;
    objVertexInput.vertexAttributeDescriptionCount = 2;
    objVertexInput.pVertexAttributeDescriptions = aVertexAttribs;

    VkPipelineInputAssemblyStateCreateInfo objInputAssembly = {};
    objInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    objInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo objViewportState = {};
    objViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    objViewportState.viewportCount = 1;
    objViewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo objRasterizer = {};
    objRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    objRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    objRasterizer.lineWidth = 1.0f;
    objRasterizer.cullMode = VK_CULL_MODE_NONE;

    VkPipelineMultisampleStateCreateInfo objMultisampling = {};
    objMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    objMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState objBlendAttachment = {};
    objBlendAttachment.blendEnable = VK_TRUE;
    objBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    objBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    objBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    objBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    objBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    objBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    objBlendAttachment.colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo objColorBlending = {};
    objColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    objColorBlending.attachmentCount = 1;
    objColorBlending.pAttachments = &objBlendAttachment;

    VkDynamicState aDynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo objDynamicState = {};
    objDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    objDynamicState.dynamicStateCount = 2;
    objDynamicState.pDynamicStates = aDynStates;

    VkPipelineDepthStencilStateCreateInfo objDepthStencil = {};
    objDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkGraphicsPipelineCreateInfo objPipelineInfo = {};
    objPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    objPipelineInfo.stageCount = 2;
    objPipelineInfo.pStages = aStages;
    objPipelineInfo.pVertexInputState = &objVertexInput;
    objPipelineInfo.pInputAssemblyState = &objInputAssembly;
    objPipelineInfo.pViewportState = &objViewportState;
    objPipelineInfo.pRasterizationState = &objRasterizer;
    objPipelineInfo.pMultisampleState = &objMultisampling;
    objPipelineInfo.pColorBlendState = &objColorBlending;
    objPipelineInfo.pDynamicState = &objDynamicState;
    objPipelineInfo.pDepthStencilState = &objDepthStencil;
    objPipelineInfo.layout = objData.m_hPipelineLayout;
    objPipelineInfo.renderPass = objData.m_hRenderPass;

    pDt->pCreateGraphicsPipelines(pDt->hDevice, VK_NULL_HANDLE, 1, &objPipelineInfo, nullptr, &objData.m_hPipeline);

    pDt->pDestroyShaderModule(pDt->hDevice, hVertModule, nullptr);
    pDt->pDestroyShaderModule(pDt->hDevice, hFragModule, nullptr);
}

/*---------------------------------------------------------
 * FN: vCreateOverlayImage
 * DESC: instantiates image mapped to overlay
 * PARMS: pDt, objData
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vCreateOverlayImage(CDevDisp *pDt, CSwapData &objData) -> void {
    VkImageCreateInfo objImgInfo = {};
    objImgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    objImgInfo.imageType = VK_IMAGE_TYPE_2D;
    objImgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    objImgInfo.extent = {s_iShmMaxWidth, s_iShmMaxHeight, 1};
    objImgInfo.mipLevels = 1;
    objImgInfo.arrayLayers = 1;
    objImgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    objImgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    objImgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    objImgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    pDt->pCreateImage(pDt->hDevice, &objImgInfo, nullptr, &objData.m_hOverlayImage);

    VkMemoryRequirements objMemReq;
    pDt->pGetImageMemoryRequirements(pDt->hDevice, objData.m_hOverlayImage, &objMemReq);

    VkMemoryAllocateInfo objAllocInfo = {};
    objAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    objAllocInfo.allocationSize = objMemReq.size;
    objAllocInfo.memoryTypeIndex = iFindMemoryType(pDt, objMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    pDt->pAllocateMemory(pDt->hDevice, &objAllocInfo, nullptr, &objData.m_hOverlayImageMemory);
    pDt->pBindImageMemory(pDt->hDevice, objData.m_hOverlayImage, objData.m_hOverlayImageMemory, 0);

    VkImageViewCreateInfo objViewInfo = {};
    objViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    objViewInfo.image = objData.m_hOverlayImage;
    objViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    objViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    objViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    pDt->pCreateImageView(pDt->hDevice, &objViewInfo, nullptr, &objData.m_hOverlayImageView);

    VkDescriptorSetAllocateInfo objDsAllocInfo = {};
    objDsAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    objDsAllocInfo.descriptorPool = objData.m_hDescriptorPool;
    objDsAllocInfo.descriptorSetCount = 1;
    objDsAllocInfo.pSetLayouts = &objData.m_hDescriptorSetLayout;
    pDt->pAllocateDescriptorSets(pDt->hDevice, &objDsAllocInfo, &objData.m_hDescriptorSet);

    VkDescriptorImageInfo objImgDescInfo = {};
    objImgDescInfo.sampler = objData.m_hSampler;
    objImgDescInfo.imageView = objData.m_hOverlayImageView;
    objImgDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet objWrite = {};
    objWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    objWrite.dstSet = objData.m_hDescriptorSet;
    objWrite.dstBinding = 0;
    objWrite.descriptorCount = 1;
    objWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    objWrite.pImageInfo = &objImgDescInfo;
    pDt->pUpdateDescriptorSets(pDt->hDevice, 1, &objWrite, 0, nullptr);

    objData.m_bOverlayImageReady = false;
    objData.m_bHasContent = false;
}

/*---------------------------------------------------------
 * FN: vCreateQuadBuffers
 * DESC: instantiates the vertex/index buffer quad
 * PARMS: pDt, objData
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vCreateQuadBuffers(CDevDisp *pDt, CSwapData &objData) -> void {
    const COverlayVertex aVertices[4] = {
        {{-1.0f, -1.0f}, {0.0f, 0.0f}},
        {{1.0f, -1.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f, 1.0f}, {0.0f, 1.0f}},
    };
    const uint16_t aIndices[6] = {0, 1, 2, 0, 2, 3};

    {
        VkBufferCreateInfo objBufInfo = {};
        objBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        objBufInfo.size = sizeof(aVertices);
        objBufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        objBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        pDt->pCreateBuffer(pDt->hDevice, &objBufInfo, nullptr, &objData.m_hQuadVertexBuffer);

        VkMemoryRequirements objMemReq;
        pDt->pGetBufferMemoryRequirements(pDt->hDevice, objData.m_hQuadVertexBuffer, &objMemReq);

        VkMemoryAllocateInfo objAllocInfo = {};
        objAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        objAllocInfo.allocationSize = objMemReq.size;
        objAllocInfo.memoryTypeIndex = iFindMemoryType(
            pDt, objMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        pDt->pAllocateMemory(pDt->hDevice, &objAllocInfo, nullptr, &objData.m_hQuadVertexMemory);
        pDt->pBindBufferMemory(pDt->hDevice, objData.m_hQuadVertexBuffer, objData.m_hQuadVertexMemory, 0);

        void *pMapped;
        pDt->pMapMemory(pDt->hDevice, objData.m_hQuadVertexMemory, 0, sizeof(aVertices), 0, &pMapped);
        memcpy(pMapped, aVertices, sizeof(aVertices));
        pDt->pUnmapMemory(pDt->hDevice, objData.m_hQuadVertexMemory);
    }

    {
        VkBufferCreateInfo objBufInfo = {};
        objBufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        objBufInfo.size = sizeof(aIndices);
        objBufInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        objBufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        pDt->pCreateBuffer(pDt->hDevice, &objBufInfo, nullptr, &objData.m_hQuadIndexBuffer);

        VkMemoryRequirements objMemReq;
        pDt->pGetBufferMemoryRequirements(pDt->hDevice, objData.m_hQuadIndexBuffer, &objMemReq);

        VkMemoryAllocateInfo objAllocInfo = {};
        objAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        objAllocInfo.allocationSize = objMemReq.size;
        objAllocInfo.memoryTypeIndex = iFindMemoryType(
            pDt, objMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        pDt->pAllocateMemory(pDt->hDevice, &objAllocInfo, nullptr, &objData.m_hQuadIndexMemory);
        pDt->pBindBufferMemory(pDt->hDevice, objData.m_hQuadIndexBuffer, objData.m_hQuadIndexMemory, 0);

        void *pMapped;
        pDt->pMapMemory(pDt->hDevice, objData.m_hQuadIndexMemory, 0, sizeof(aIndices), 0, &pMapped);
        memcpy(pMapped, aIndices, sizeof(aIndices));
        pDt->pUnmapMemory(pDt->hDevice, objData.m_hQuadIndexMemory);
    }
}

/*---------------------------------------------------------
 * FN: vCreateSwapchainResources
 * DESC: creates render resources
 * PARMS: pDt, hSwapchain, pCreateInfo
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vCreateSwapchainResources(CDevDisp *pDt, VkSwapchainKHR hSwapchain,
                                      const VkSwapchainCreateInfoKHR *pCreateInfo) -> void {
    std::lock_guard<std::mutex> objLock(m_objMtx);

    CSwapData &objData = m_mapSwapchains[hSwapchain];
    objData.m_hSwapchain = hSwapchain;
    objData.m_objExtent = pCreateInfo->imageExtent;
    objData.m_iFormat = pCreateInfo->imageFormat;

    auto objNow = std::chrono::steady_clock::now();
    m_objLastSwapchainCreate = objNow;

    uint32_t iImageCount;
    pDt->pGetSwapchainImagesKHR(pDt->hDevice, hSwapchain, &iImageCount, nullptr);
    objData.m_vImages.resize(iImageCount);
    pDt->pGetSwapchainImagesKHR(pDt->hDevice, hSwapchain, &iImageCount, objData.m_vImages.data());

    VkCommandPoolCreateInfo objPoolInfo = {};
    objPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    objPoolInfo.queueFamilyIndex = pDt->iGraphicsQueueFamily;
    objPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pDt->pCreateCommandPool(pDt->hDevice, &objPoolInfo, nullptr, &objData.m_hCommandPool);

    vCreateRenderPass(pDt, objData);
    vCreatePipeline(pDt, objData);
    vCreateOverlayImage(pDt, objData);
    vCreateQuadBuffers(pDt, objData);

    objData.m_vImageViews.resize(iImageCount);
    objData.m_vFramebuffers.resize(iImageCount);
    objData.m_vCommandBuffers.resize(iImageCount);
    objData.m_vFences.resize(iImageCount);
    objData.m_vPerFrame.resize(iImageCount);

    for (uint32_t i = 0; i < iImageCount; i++) {
        VkImageViewCreateInfo objViewInfo = {};
        objViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        objViewInfo.image = objData.m_vImages[i];
        objViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        objViewInfo.format = objData.m_iFormat;
        objViewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        pDt->pCreateImageView(pDt->hDevice, &objViewInfo, nullptr, &objData.m_vImageViews[i]);

        VkFramebufferCreateInfo objFbInfo = {};
        objFbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        objFbInfo.renderPass = objData.m_hRenderPass;
        objFbInfo.attachmentCount = 1;
        objFbInfo.pAttachments = &objData.m_vImageViews[i];
        objFbInfo.width = objData.m_objExtent.width;
        objFbInfo.height = objData.m_objExtent.height;
        objFbInfo.layers = 1;
        pDt->pCreateFramebuffer(pDt->hDevice, &objFbInfo, nullptr, &objData.m_vFramebuffers[i]);

        VkFenceCreateInfo objFenceInfo = {};
        objFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        objFenceInfo.flags = 0;
        pDt->pCreateFence(pDt->hDevice, &objFenceInfo, nullptr, &objData.m_vFences[i]);

        objData.m_vPerFrame[i].m_bSubmitted = false;

        VkSemaphoreCreateInfo objSemInfo = {};
        objSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        pDt->pCreateSemaphore(pDt->hDevice, &objSemInfo, nullptr, &objData.m_vPerFrame[i].m_hRenderSemaphore);
    }

    VkCommandBufferAllocateInfo objCmdAllocInfo = {};
    objCmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    objCmdAllocInfo.commandPool = objData.m_hCommandPool;
    objCmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    objCmdAllocInfo.commandBufferCount = iImageCount;
    pDt->pAllocateCommandBuffers(pDt->hDevice, &objCmdAllocInfo, objData.m_vCommandBuffers.data());

    for (VkCommandBuffer hCmd : objData.m_vCommandBuffers) {
        vInitDispatchKey(hCmd, pDt->hDevice);
    }

    objData.m_bInitialized = true;
    fprintf(stderr, "[recar|roverpp] swapchain resources created (%ux%u, %u images)\n", objData.m_objExtent.width,
            objData.m_objExtent.height, iImageCount);
}

/*---------------------------------------------------------
 * FN: vDestroySwapchainResources
 * DESC: destroys resources
 * PARMS: pDt, hSwapchain
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vDestroySwapchainResources(CDevDisp *pDt, VkSwapchainKHR hSwapchain) -> void {
    std::lock_guard<std::mutex> objLock(m_objMtx);

    auto objIt = m_mapSwapchains.find(hSwapchain);
    if (objIt == m_mapSwapchains.end())
        return;

    pDt->pDeviceWaitIdle(pDt->hDevice);
    CSwapData &objData = objIt->second;

    for (size_t i = 0; i < objData.m_vImages.size(); i++) {
        if (objData.m_vFences[i])
            pDt->pDestroyFence(pDt->hDevice, objData.m_vFences[i], nullptr);
        if (objData.m_vFramebuffers[i])
            pDt->pDestroyFramebuffer(pDt->hDevice, objData.m_vFramebuffers[i], nullptr);
        if (objData.m_vImageViews[i])
            pDt->pDestroyImageView(pDt->hDevice, objData.m_vImageViews[i], nullptr);
        if (objData.m_vPerFrame[i].m_hRenderSemaphore)
            pDt->pDestroySemaphore(pDt->hDevice, objData.m_vPerFrame[i].m_hRenderSemaphore, nullptr);
    }

    if (objData.m_hQuadVertexBuffer)
        pDt->pDestroyBuffer(pDt->hDevice, objData.m_hQuadVertexBuffer, nullptr);
    if (objData.m_hQuadVertexMemory)
        pDt->pFreeMemory(pDt->hDevice, objData.m_hQuadVertexMemory, nullptr);
    if (objData.m_hQuadIndexBuffer)
        pDt->pDestroyBuffer(pDt->hDevice, objData.m_hQuadIndexBuffer, nullptr);
    if (objData.m_hQuadIndexMemory)
        pDt->pFreeMemory(pDt->hDevice, objData.m_hQuadIndexMemory, nullptr);

    if (objData.m_hOverlayImageView)
        pDt->pDestroyImageView(pDt->hDevice, objData.m_hOverlayImageView, nullptr);
    if (objData.m_hOverlayImage)
        pDt->pDestroyImage(pDt->hDevice, objData.m_hOverlayImage, nullptr);
    if (objData.m_hOverlayImageMemory)
        pDt->pFreeMemory(pDt->hDevice, objData.m_hOverlayImageMemory, nullptr);

    if (objData.m_hStagingBuffer)
        pDt->pDestroyBuffer(pDt->hDevice, objData.m_hStagingBuffer, nullptr);
    if (objData.m_hStagingMemory)
        pDt->pFreeMemory(pDt->hDevice, objData.m_hStagingMemory, nullptr);

    if (objData.m_hPipeline)
        pDt->pDestroyPipeline(pDt->hDevice, objData.m_hPipeline, nullptr);
    if (objData.m_hPipelineLayout)
        pDt->pDestroyPipelineLayout(pDt->hDevice, objData.m_hPipelineLayout, nullptr);
    if (objData.m_hRenderPass)
        pDt->pDestroyRenderPass(pDt->hDevice, objData.m_hRenderPass, nullptr);
    if (objData.m_hCommandPool)
        pDt->pDestroyCommandPool(pDt->hDevice, objData.m_hCommandPool, nullptr);
    if (objData.m_hSampler)
        pDt->pDestroySampler(pDt->hDevice, objData.m_hSampler, nullptr);
    if (objData.m_hDescriptorPool)
        pDt->pDestroyDescriptorPool(pDt->hDevice, objData.m_hDescriptorPool, nullptr);
    if (objData.m_hDescriptorSetLayout)
        pDt->pDestroyDescriptorSetLayout(pDt->hDevice, objData.m_hDescriptorSetLayout, nullptr);

    m_mapSwapchains.erase(objIt);
}

/*---------------------------------------------------------
 * FN: bUploadOverlayTexture
 * DESC: pushes pixels to gpu
 * PARMS: pDt, objData, hCmd, pPixels, iWidth, iHeight
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::bUploadOverlayTexture(CDevDisp *pDt, CSwapData &objData, VkCommandBuffer hCmd, const uint8_t *pPixels,
                                  uint32_t iWidth, uint32_t iHeight) -> bool {
    if (iWidth == 0 || iHeight == 0 || iWidth > s_iShmMaxWidth || iHeight > s_iShmMaxHeight)
        return false;

    size_t iPixelSize = (size_t)iWidth * iHeight * 4;
    vEnsureStagingBuffer(pDt, objData, iPixelSize);

    void *pMapped;
    pDt->pMapMemory(pDt->hDevice, objData.m_hStagingMemory, 0, iPixelSize, 0, &pMapped);
    memcpy(pMapped, pPixels, iPixelSize);
    pDt->pUnmapMemory(pDt->hDevice, objData.m_hStagingMemory);

    VkImageMemoryBarrier objBarrier = {};
    objBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    objBarrier.oldLayout =
        objData.m_bOverlayImageReady ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
    objBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    objBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    objBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    objBarrier.image = objData.m_hOverlayImage;
    objBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    objBarrier.srcAccessMask = objData.m_bOverlayImageReady ? VK_ACCESS_SHADER_READ_BIT : 0;
    objBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    pDt->pCmdPipelineBarrier(hCmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &objBarrier);

    VkBufferImageCopy objCopyRegion = {};
    objCopyRegion.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    objCopyRegion.imageExtent = {iWidth, iHeight, 1};
    pDt->pCmdCopyBufferToImage(hCmd, objData.m_hStagingBuffer, objData.m_hOverlayImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &objCopyRegion);

    VkImageMemoryBarrier objBarrier2 = objBarrier;
    objBarrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    objBarrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    objBarrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    objBarrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    pDt->pCmdPipelineBarrier(hCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                             0, nullptr, 1, &objBarrier2);

    objData.m_bOverlayImageReady = true;
    objData.m_iOverlayWidth = iWidth;
    objData.m_iOverlayHeight = iHeight;
    objData.m_bHasContent = true;

    return true;
}

/*---------------------------------------------------------
 * FN: vRender
 * DESC: renders to given swapchain img
 * PARMS: pDt, hQueue, hSwapchain, iImageIndex, hWaitSemaphore, pSignalSemaphore
 * AUTH: unium (06.03.26)
 *-------------------------------------------------------*/
auto CRend::vRender(CDevDisp *pDt, VkQueue hQueue, VkSwapchainKHR hSwapchain, uint32_t iImageIndex,
                    VkSemaphore hWaitSemaphore, VkSemaphore *pSignalSemaphore) -> void {
    std::lock_guard<std::mutex> objLock(m_objMtx);

    auto objIt = m_mapSwapchains.find(hSwapchain);
    if (objIt == m_mapSwapchains.end() || !objIt->second.m_bInitialized) {
        *pSignalSemaphore = VK_NULL_HANDLE;
        return;
    }

    CSwapData &objData = objIt->second;
    if (iImageIndex >= objData.m_vImages.size()) {
        *pSignalSemaphore = VK_NULL_HANDLE;
        return;
    }

    auto &objPf = objData.m_vPerFrame[iImageIndex];

    if (objPf.m_bSubmitted) {
        VkResult iFr = pDt->pWaitForFences(pDt->hDevice, 1, &objData.m_vFences[iImageIndex], VK_TRUE, 500000000ULL);
        if (iFr != VK_SUCCESS) {
            *pSignalSemaphore = VK_NULL_HANDLE;
            return;
        }
        pDt->pResetFences(pDt->hDevice, 1, &objData.m_vFences[iImageIndex]);
        objPf.m_bSubmitted = false;
    }

    bool bNeedUpload = false;
    uint32_t iFrameW = 0, iFrameH = 0;
    const uint8_t *pPixels = nullptr;

    if (m_bShmOpen) {
        CShmHead *pFrame = m_objShm.pTryAcquireFrame();
        if (pFrame) {
            iFrameW = pFrame->m_iOverlayWidth;
            iFrameH = pFrame->m_iOverlayHeight;
            pPixels = m_objShm.pPixelData();
            bNeedUpload = (iFrameW > 0 && iFrameH > 0 && iFrameW <= s_iShmMaxWidth && iFrameH <= s_iShmMaxHeight);
        }
    }

    if (!objData.m_bHasContent && !bNeedUpload) {
        *pSignalSemaphore = VK_NULL_HANDLE;
        return;
    }

    VkCommandBuffer hCmd = objData.m_vCommandBuffers[iImageIndex];
    pDt->pResetCommandBuffer(hCmd, 0);

    VkCommandBufferBeginInfo objBeginInfo = {};
    objBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    objBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    pDt->pBeginCommandBuffer(hCmd, &objBeginInfo);

    if (bNeedUpload) {
        bUploadOverlayTexture(pDt, objData, hCmd, pPixels, iFrameW, iFrameH);
    }

    if (objData.m_bHasContent && objData.m_bOverlayImageReady) {
        float iScreenW = (float)objData.m_objExtent.width;
        float iScreenH = (float)objData.m_objExtent.height;

        VkRenderPassBeginInfo objRpBegin = {};
        objRpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        objRpBegin.renderPass = objData.m_hRenderPass;
        objRpBegin.framebuffer = objData.m_vFramebuffers[iImageIndex];
        objRpBegin.renderArea.extent = objData.m_objExtent;

        pDt->pCmdBeginRenderPass(hCmd, &objRpBegin, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport objVp = {0, 0, iScreenW, iScreenH, 0, 1};
        pDt->pCmdSetViewport(hCmd, 0, 1, &objVp);

        VkRect2D objSc = {{0, 0}, objData.m_objExtent};
        pDt->pCmdSetScissor(hCmd, 0, 1, &objSc);

        pDt->pCmdBindPipeline(hCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objData.m_hPipeline);
        pDt->pCmdBindDescriptorSets(hCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, objData.m_hPipelineLayout, 0, 1,
                                    &objData.m_hDescriptorSet, 0, nullptr);

        COverlayPushConstants objPc;
        objPc.m_aUvScale[0] = (float)objData.m_iOverlayWidth / (float)s_iShmMaxWidth;
        objPc.m_aUvScale[1] = (float)objData.m_iOverlayHeight / (float)s_iShmMaxHeight;
        pDt->pCmdPushConstants(hCmd, objData.m_hPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(objPc), &objPc);

        VkDeviceSize iOffset = 0;
        pDt->pCmdBindVertexBuffers(hCmd, 0, 1, &objData.m_hQuadVertexBuffer, &iOffset);
        pDt->pCmdBindIndexBuffer(hCmd, objData.m_hQuadIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
        pDt->pCmdDrawIndexed(hCmd, 6, 1, 0, 0, 0);

        pDt->pCmdEndRenderPass(hCmd);
    }

    pDt->pEndCommandBuffer(hCmd);

    VkSemaphore hOverlaySem = objPf.m_hRenderSemaphore;
    VkPipelineStageFlags iWaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo objSi = {};
    objSi.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    objSi.commandBufferCount = 1;
    objSi.pCommandBuffers = &hCmd;

    if (hWaitSemaphore != VK_NULL_HANDLE) {
        objSi.waitSemaphoreCount = 1;
        objSi.pWaitSemaphores = &hWaitSemaphore;
        objSi.pWaitDstStageMask = &iWaitStage;
    }

    objSi.signalSemaphoreCount = 1;
    objSi.pSignalSemaphores = &hOverlaySem;

    VkResult iSr = pDt->pQueueSubmit(hQueue, 1, &objSi, objData.m_vFences[iImageIndex]);
    if (iSr == VK_SUCCESS) {
        objPf.m_bSubmitted = true;
        *pSignalSemaphore = hOverlaySem;
    } else {
        *pSignalSemaphore = VK_NULL_HANDLE;
    }
}
} // namespace RV
