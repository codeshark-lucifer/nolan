#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdlib>

#include "vulkan/buffers.hpp"
#include "vulkan/vertex.hpp"
#include "vulkan/utils.hpp"
#include "vulkan/pipeline.hpp"

namespace OLIA_ENGINE
{
    const uint32_t PARTICLE_COUNT = 100000;

    // Unity-like Particle System Module Settings
    struct ParticleSettings
    {
        uint32_t maxParticles = PARTICLE_COUNT;
        float startLifetime   = 2.5f;     // Duration of particle life (seconds)
        float startSpeed      = 2.0f;     // Initial launch speed
        float particleSize    = 14.0f;    // Rendered point size
        float gravityModifier = -0.8f;    // Negative = floats upward (like fire/magic)
        float swirlSpeed      = 6.0f;     // Angular velocity (swirling vortex effect)
        float emitterRadius   = 1.2f;     // Spawn ring radius

        // Color over Lifetime (Start -> End interpolation)
        glm::vec4 startColor = glm::vec4(0.1f, 0.8f, 1.0f, 1.0f); // Cosmic Cyan
        glm::vec4 endColor   = glm::vec4(0.9f, 0.1f, 0.8f, 0.0f); // Fades to Deep Violet
    };

    // Push Constants Layout sent to GPU Compute Shader (64 bytes)
    struct ComputePushConstants
    {
        float deltaTime;
        float totalTime;
        float startSpeed;
        float gravity;
        float swirlSpeed;
        float emitterRadius;
        float startLifetime;
        float padding = 0.0f;
        glm::vec4 startColor;
        glm::vec4 endColor;
    };

    class ParticleSystem
    {
    private:
        Buffer particleBuffer;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;

    public:
        ParticleSettings settings; // Exposed Unity-like Inspector Settings

        VkBuffer GetBuffer() const { return particleBuffer.buffer; }

        /**
         * @brief Initializes GPU particle buffer, descriptor sets, and compute pipeline.
         */
        bool Create(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                    VkQueue graphicsQueue, VkPipelineCache pipelineCache = VK_NULL_HANDLE,
                    const VkAllocationCallbacks *allocator = nullptr)
        {
            // 1. Initial Particle CPU Allocation
            std::vector<Particle> particles(PARTICLE_COUNT);
            for (size_t i = 0; i < PARTICLE_COUNT; ++i)
            {
                particles[i].pos = glm::vec4(0.0f, -100.0f, 0.0f, 0.0f); // Hide offscreen initially (life = 0)
                particles[i].vel = glm::vec4(0.0f, 0.0f, 0.0f, settings.startLifetime);
                particles[i].color = settings.startColor;
            }

            VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

            // 2. Create Staging Buffer & Transfer
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            CreateBuffer(device, physicalDevice, bufferSize,
                         VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         stagingBuffer, stagingMemory, allocator);

            void *data = nullptr;
            vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &data);
            memcpy(data, particles.data(), static_cast<size_t>(bufferSize));
            vkUnmapMemory(device, stagingMemory);

            CreateBuffer(device, physicalDevice, bufferSize,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         particleBuffer.buffer, particleBuffer.memory, allocator);

            copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, particleBuffer.buffer, bufferSize);

            vkDestroyBuffer(device, stagingBuffer, allocator);
            vkFreeMemory(device, stagingMemory, allocator);

            // 3. Descriptor Set Layout
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            layoutInfo.bindingCount = 1;
            layoutInfo.pBindings = &binding;

            if (vkCreateDescriptorSetLayout(device, &layoutInfo, allocator, &descriptorSetLayout) != VK_SUCCESS)
                return false;

            // 4. Descriptor Pool & Set Allocation
            VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
            VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes = &poolSize;
            poolInfo.maxSets = 1;

            if (vkCreateDescriptorPool(device, &poolInfo, allocator, &descriptorPool) != VK_SUCCESS)
                return false;

            VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = 1;
            allocInfo.pSetLayouts = &descriptorSetLayout;

            if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
                return false;

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = particleBuffer.buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;

            VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            write.dstSet = descriptorSet;
            write.dstBinding = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

            // 5. Pipeline Layout (Compute Push Constants: 64 bytes)
            VkPushConstantRange pushConstantRange{};
            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRange.offset = 0;
            pushConstantRange.size = sizeof(ComputePushConstants);

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, allocator, &pipelineLayout) != VK_SUCCESS)
                return false;

            // 6. Compute Pipeline Creation
            std::vector<char> compCode = ReadFile("assets/shaders/compiled/main.comp.spv");
            VkShaderModule compModule = CreateShaderModule(compCode, device, allocator);

            VkComputePipelineCreateInfo pipelineCreateInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineCreateInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compModule, "main"};
            pipelineCreateInfo.layout = pipelineLayout;

            VkResult result = vkCreateComputePipelines(device, pipelineCache, 1, &pipelineCreateInfo, allocator, &pipeline);

            vkDestroyShaderModule(device, compModule, allocator);

            return result == VK_SUCCESS;
        }

        /**
         * @brief Updates settings on GPU and dispatches compute workgroups.
         */
        void Dispatch(VkCommandBuffer commandBuffer, float deltaTime, float totalTime)
        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

            // Pack dynamic settings into Push Constants
            ComputePushConstants push{};
            push.deltaTime      = deltaTime;
            push.totalTime      = totalTime;
            push.startSpeed     = settings.startSpeed;
            push.gravity        = settings.gravityModifier;
            push.swirlSpeed     = settings.swirlSpeed;
            push.emitterRadius  = settings.emitterRadius;
            push.startLifetime  = settings.startLifetime;
            push.startColor     = settings.startColor;
            push.endColor       = settings.endColor;

            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &push);

            // Dispatch Workgroups
            uint32_t groupCountX = (PARTICLE_COUNT + 255) / 256;
            vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

            // Memory Barrier (Compute Write -> Vertex Attribute Read)
            VkBufferMemoryBarrier bufferBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
            bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
            bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            bufferBarrier.buffer = particleBuffer.buffer;
            bufferBarrier.offset = 0;
            bufferBarrier.size = VK_WHOLE_SIZE;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                0, 0, nullptr,
                1, &bufferBarrier,
                0, nullptr);
        }

        void Destroy(VkDevice device, const VkAllocationCallbacks *allocator = nullptr)
        {
            if (pipeline != VK_NULL_HANDLE)
            {
                vkDestroyPipeline(device, pipeline, allocator);
                pipeline = VK_NULL_HANDLE;
            }
            if (pipelineLayout != VK_NULL_HANDLE)
            {
                vkDestroyPipelineLayout(device, pipelineLayout, allocator);
                pipelineLayout = VK_NULL_HANDLE;
            }
            if (descriptorPool != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorPool(device, descriptorPool, allocator);
                descriptorPool = VK_NULL_HANDLE;
            }
            if (descriptorSetLayout != VK_NULL_HANDLE)
            {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, allocator);
                descriptorSetLayout = VK_NULL_HANDLE;
            }
            if (particleBuffer.buffer != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, particleBuffer.buffer, allocator);
                particleBuffer.buffer = VK_NULL_HANDLE;
            }
            if (particleBuffer.memory != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, particleBuffer.memory, allocator);
                particleBuffer.memory = VK_NULL_HANDLE;
            }
        }
    };
}

using ParticleSystem = OLIA_ENGINE::ParticleSystem;