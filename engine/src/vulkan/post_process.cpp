#include "vulkan/post_process.hpp"
#include "vulkan/textures.hpp"
#include "vulkan/utils.hpp"
#include <array>
#include <stdexcept>

namespace OLIA_ENGINE
{
    void CreateOffscreenResources(VKContext &ctx)
    {
        CreateImage(
            ctx.device, ctx.physicalDevice,
            ctx.swapchainExtent.width, ctx.swapchainExtent.height,
            ctx.swapchainFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ctx.offscreenColorImage, ctx.offscreenColorImageMemory,
            VK_SAMPLE_COUNT_1_BIT, ctx.allocator.callbacks());

        ctx.offscreenColorImageView = CreateImageView(ctx.device, ctx.offscreenColorImage, ctx.swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, ctx.allocator.callbacks());
        
        // 2. Create Linear Sampler for Post-Processing
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

        if (vkCreateSampler(ctx.device, &samplerInfo, ctx.allocator.callbacks(), &ctx.offscreenSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create offscreen sampler!");
        }
    }

    void DestroyOffscreenResources(VKContext &ctx)
    {
        if (ctx.offscreenFramebuffer)
        {
            vkDestroyFramebuffer(ctx.device, ctx.offscreenFramebuffer, ctx.allocator.callbacks());
            ctx.offscreenFramebuffer = VK_NULL_HANDLE;
        }
        if (ctx.offscreenSampler)
        {
            vkDestroySampler(ctx.device, ctx.offscreenSampler, ctx.allocator.callbacks());
            ctx.offscreenSampler = VK_NULL_HANDLE;
        }
        if (ctx.offscreenColorImageView)
        {
            vkDestroyImageView(ctx.device, ctx.offscreenColorImageView, ctx.allocator.callbacks());
            ctx.offscreenColorImageView = VK_NULL_HANDLE;
        }
        if (ctx.offscreenColorImage)
        {
            vkDestroyImage(ctx.device, ctx.offscreenColorImage, ctx.allocator.callbacks());
            ctx.offscreenColorImage = VK_NULL_HANDLE;
        }
        if (ctx.offscreenColorImageMemory)
        {
            vkFreeMemory(ctx.device, ctx.offscreenColorImageMemory, ctx.allocator.callbacks());
            ctx.offscreenColorImageMemory = VK_NULL_HANDLE;
        }
    }

    bool CreateOffscreenRenderPass(VKContext &ctx)
    {
        // Attachment 0: MSAA Color
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = ctx.swapchainFormat;
        colorAttachment.samples = ctx.msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        // Attachment 1: MSAA Depth
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = FindDepthFormat(ctx.physicalDevice);
        depthAttachment.samples = ctx.msaaSamples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        // Attachment 2: Resolve Target (Offscreen Image for Post-Process)
        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = ctx.swapchainFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Ready for screen.frag sampling!

        VkAttachmentReference colorAttachmentResolveRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        return vkCreateRenderPass(ctx.device, &renderPassInfo, ctx.allocator.callbacks(), &ctx.offscreenRenderPass) == VK_SUCCESS;
    }

    bool CreateOffscreenFramebuffer(VKContext &ctx)
    {
        std::array<VkImageView, 3> attachments = {
            ctx.colorImageView,
            ctx.depthImageView,
            ctx.offscreenColorImageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = ctx.offscreenRenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = ctx.swapchainExtent.width;
        framebufferInfo.height = ctx.swapchainExtent.height;
        framebufferInfo.layers = 1;

        return vkCreateFramebuffer(ctx.device, &framebufferInfo, ctx.allocator.callbacks(), &ctx.offscreenFramebuffer) == VK_SUCCESS;
    }

    void CreatePostProcessDescriptors(VKContext &ctx)
    {
        // 1. Layout
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        if (vkCreateDescriptorSetLayout(ctx.device, &layoutInfo, ctx.allocator.callbacks(), &ctx.postProcessPipeline.descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create post-process descriptor layout!");

        // 2. Pool
        VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(ctx.device, &poolInfo, ctx.allocator.callbacks(), &ctx.postProcessDescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create post-process descriptor pool!");

        // 3. Allocate Set
        VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocInfo.descriptorPool = ctx.postProcessDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &ctx.postProcessPipeline.descriptorSetLayout;

        if (vkAllocateDescriptorSets(ctx.device, &allocInfo, &ctx.postProcessDescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate post-process descriptor set!");

        // 4. Update Write Descriptor Set
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = ctx.offscreenColorImageView;
        imageInfo.sampler = ctx.offscreenSampler;

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = ctx.postProcessDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);
    }

    bool CreatePostProcessPipeline(VKContext &ctx)
    {
        std::vector<char> vertCode = ReadFile("assets/shaders/compiled/screen.vert.spv");
        std::vector<char> fragCode = ReadFile("assets/shaders/compiled/screen.frag.spv");

        VkShaderModule vertModule = CreateShaderModule(vertCode, ctx.device, ctx.allocator.callbacks());
        VkShaderModule fragModule = CreateShaderModule(fragCode, ctx.device, ctx.allocator.callbacks());

        VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main"};
        VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main"};
        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // No vertex buffer input required for screen quad!
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

        VkViewport viewport{0.0f, 0.0f, static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, ctx.swapchainExtent};
        VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, &viewport, 1, &scissor};

        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f};
        VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE};

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &colorBlendAttachment};

        std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &ctx.postProcessPipeline.descriptorSetLayout;

        if (vkCreatePipelineLayout(ctx.device, &pipelineLayoutInfo, ctx.allocator.callbacks(), &ctx.postProcessPipeline.layout) != VK_SUCCESS)
            return false;

        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // No depth testing needed for screen quad
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = ctx.postProcessPipeline.layout;
        pipelineInfo.renderPass = ctx.renderPass; // Swapchain RenderPass
        pipelineInfo.subpass = 0;

        VkResult result = vkCreateGraphicsPipelines(ctx.device, ctx.pipelineCache, 1, &pipelineInfo, ctx.allocator.callbacks(), &ctx.postProcessPipeline.pipeline);

        vkDestroyShaderModule(ctx.device, fragModule, ctx.allocator.callbacks());
        vkDestroyShaderModule(ctx.device, vertModule, ctx.allocator.callbacks());

        return result == VK_SUCCESS;
    }

    void DestroyPostProcessPipeline(VKContext &ctx)
    {
        if (ctx.postProcessPipeline.pipeline)
        {
            vkDestroyPipeline(ctx.device, ctx.postProcessPipeline.pipeline, ctx.allocator.callbacks());
            ctx.postProcessPipeline.pipeline = VK_NULL_HANDLE;
        }
        if (ctx.postProcessPipeline.layout)
        {
            vkDestroyPipelineLayout(ctx.device, ctx.postProcessPipeline.layout, ctx.allocator.callbacks());
            ctx.postProcessPipeline.layout = VK_NULL_HANDLE;
        }
        if (ctx.postProcessPipeline.descriptorSetLayout)
        {
            vkDestroyDescriptorSetLayout(ctx.device, ctx.postProcessPipeline.descriptorSetLayout, ctx.allocator.callbacks());
            ctx.postProcessPipeline.descriptorSetLayout = VK_NULL_HANDLE;
        }
        if (ctx.postProcessDescriptorPool)
        {
            vkDestroyDescriptorPool(ctx.device, ctx.postProcessDescriptorPool, ctx.allocator.callbacks());
            ctx.postProcessDescriptorPool = VK_NULL_HANDLE;
        }
    }
}