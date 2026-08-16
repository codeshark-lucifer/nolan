#include "vulkan/pipeline.hpp"
#include "vulkan/utils.hpp"
#include "vulkan/vertex.hpp"
#include "vulkan/descriptors.hpp"
#include <stdexcept>
#include <array>

namespace OLIA_ENGINE
{
    VkShaderModule CreateShaderModule(
        const std::vector<char> &code,
        VkDevice device,
        const VkAllocationCallbacks *allocator)
    {
        if (code.empty())
            throw std::runtime_error("Shader code is empty!");
        if (code.size() % sizeof(uint32_t) != 0)
            throw std::runtime_error("Invalid SPIR-V shader code size!");

        VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(device, &createInfo, allocator, &module) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module!");

        return module;
    }

    GraphicsPipeline CreateParticlePipeline(
        const std::string &vertFile,
        const std::string &fragFile,
        VkExtent2D swapchainExtent,
        VkDevice device,
        const VkAllocationCallbacks *allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaaSamples,
        VkPipelineCache pipelineCache)
    {
        GraphicsPipeline graphics{};
        CreateDescriptorSetLayout(device, &graphics.descriptorSetLayout, allocator);

        const auto vertCode = ReadFile(vertFile);
        const auto fragCode = ReadFile(fragFile);

        const VkShaderModule vertShader = CreateShaderModule(vertCode, device, allocator);
        const VkShaderModule fragShader = CreateShaderModule(fragCode, device, allocator);

        VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShader;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertStage, fragStage};

        // 1. Particle Buffer Layout (Particle struct: 48 bytes)
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Particle);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        attributeDescriptions[0] = {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Particle, pos))};
        attributeDescriptions[1] = {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Particle, vel))};
        attributeDescriptions[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Particle, color))};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // 2. Point List Topology
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{0.0f, 0.0f, static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, swapchainExtent};

        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        const std::vector<VkDynamicState> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 3. Disable Culling
        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisampling.rasterizationSamples = msaaSamples;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        // 4. Enable Depth Testing, Disable Depth Writing
        VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &graphics.descriptorSetLayout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, allocator, &graphics.layout) != VK_SUCCESS)
        {
            vkDestroyShaderModule(device, fragShader, allocator);
            vkDestroyShaderModule(device, vertShader, allocator);
            return {};
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = graphics.layout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        const VkResult result = vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, allocator, &graphics.pipeline);

        vkDestroyShaderModule(device, fragShader, allocator);
        vkDestroyShaderModule(device, vertShader, allocator);

        if (result != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(device, graphics.layout, allocator);
            return {};
        }

        return graphics;
    }

    GraphicsPipeline CreateGraphicsPipeline(
        const std::string &vertFile,
        const std::string &fragFile,
        VkExtent2D swapchainExtent,
        VkDevice device,
        const VkAllocationCallbacks *allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaaSamples,
        VkPipelineCache pipelineCache,
        uint32_t contantSize
    )
    {
        GraphicsPipeline graphics{};
        CreateDescriptorSetLayout(device, &graphics.descriptorSetLayout, allocator);

        const auto vertCode = ReadFile(vertFile);
        const auto fragCode = ReadFile(fragFile);

        const VkShaderModule vertShader =
            CreateShaderModule(vertCode, device, allocator);

        const VkShaderModule fragShader =
            CreateShaderModule(fragCode, device, allocator);

        VkPipelineShaderStageCreateInfo vertStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShader;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertStage,
            fragStage};

        const auto bindingDescription =
            Vertex::getBindingDescription();

        const auto attributeDescriptions =
            Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;

        vertexInputInfo.vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size());

        vertexInputInfo.pVertexAttributeDescriptions =
            attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

        inputAssembly.topology =
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{
            0.0f,
            0.0f,
            static_cast<float>(swapchainExtent.width),
            static_cast<float>(swapchainExtent.height),
            0.0f,
            1.0f};

        VkRect2D scissor{
            {0, 0},
            swapchainExtent};

        VkPipelineViewportStateCreateInfo viewportState{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};

        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        const std::vector<VkDynamicState> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};

        dynamicState.dynamicStateCount =
            static_cast<uint32_t>(dynamicStates.size());

        dynamicState.pDynamicStates =
            dynamicStates.data();

        VkPipelineRasterizationStateCreateInfo rasterizer{
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = msaaSamples;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};

        colorBlendAttachment.blendEnable = VK_FALSE;

        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineDepthStencilStateCreateInfo depthStencil{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        // Enable Push Constants for Vertex and Fragment stages
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = contantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &graphics.descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(
                device,
                &pipelineLayoutInfo,
                allocator,
                &graphics.layout) != VK_SUCCESS)
        {
            vkDestroyShaderModule(device, fragShader, allocator);
            vkDestroyShaderModule(device, vertShader, allocator);

            return {};
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;

        pipelineInfo.layout = graphics.layout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        const VkResult result = vkCreateGraphicsPipelines(
            device,
            pipelineCache,
            1,
            &pipelineInfo,
            allocator,
            &graphics.pipeline);

        vkDestroyShaderModule(device, fragShader, allocator);
        vkDestroyShaderModule(device, vertShader, allocator);

        if (result != VK_SUCCESS)
        {
            vkDestroyPipelineLayout(
                device,
                graphics.layout,
                allocator);

            return {};
        }

        return graphics;
    }

    void DestroyGraphicsPipeline(GraphicsPipeline &graphicsPipeline, VkDevice device, const VkAllocationCallbacks *allocator)
    {
        if (graphicsPipeline.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(device, graphicsPipeline.pipeline, allocator);
            graphicsPipeline.pipeline = VK_NULL_HANDLE;
        }

        if (graphicsPipeline.layout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(device, graphicsPipeline.layout, allocator);
            graphicsPipeline.layout = VK_NULL_HANDLE;
        }
    }
}