#pragma once

#include "./mesh.h"
#include "./material.h"
#include "vulkan/buffers.hpp"

#include <memory>
#include <stdexcept>

namespace OLIA_ENGINE
{
    class MeshRenderer
    {
    private:
        std::unique_ptr<Mesh> mesh;
        Material *material = nullptr; // Use Pointer instead of Reference!

        Buffer vertex;
        Buffer index;

    public:
        // Default constructor now works cleanly with ECS ComponentPool
        MeshRenderer() = default;

        explicit MeshRenderer(std::unique_ptr<Mesh> m, Material *mat = nullptr)
            : mesh(std::move(m)), material(mat)
        {
        }

        // --------------------------------------------------------
        // Mesh & Material Getters / Setters
        // --------------------------------------------------------

        void SetMesh(std::unique_ptr<Mesh> m)
        {
            mesh = std::move(m);
        }

        void SetMaterial(Material *mat)
        {
            material = mat;
        }

        Material *GetMaterial() { return material; }
        const Material *GetMaterial() const { return material; }

        Mesh *GetMesh() { return mesh.get(); }
        const Mesh *GetMesh() const { return mesh.get(); }

        // --------------------------------------------------------
        // GPU Setup
        // --------------------------------------------------------

        void Setup(
            VkDevice device,
            VkPhysicalDevice physicalDevice,
            VkCommandPool commandPool,
            VkQueue queue,
            const VkAllocationCallbacks *allocator = nullptr)
        {
            if (!mesh)
            {
                throw std::runtime_error("MeshRenderer::Setup(): mesh is null");
            }

            if (mesh->vertices.empty() || mesh->indices.empty())
            {
                throw std::runtime_error("MeshRenderer::Setup(): mesh has empty vertices/indices");
            }

            UploadBuffer(
                device, physicalDevice, commandPool, queue,
                mesh->vertices.data(), sizeof(mesh->vertices[0]) * mesh->vertices.size(),
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                vertex.buffer, vertex.memory, allocator);

            UploadBuffer(
                device, physicalDevice, commandPool, queue,
                mesh->indices.data(), sizeof(mesh->indices[0]) * mesh->indices.size(),
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                index.buffer, index.memory, allocator);
        }

        // --------------------------------------------------------
        // GPU Cleanup
        // --------------------------------------------------------

        void Destroy(
            VkDevice device,
            const VkAllocationCallbacks *allocator = nullptr)
        {
            DestroyBuffer(device, vertex, allocator);
            DestroyBuffer(device, index, allocator);
        }

        // --------------------------------------------------------
        // Draw (Uses assigned Material if available)
        // --------------------------------------------------------

        void Draw(
            VkCommandBuffer commandBuffer,
            VkPipelineLayout fallbackLayout = VK_NULL_HANDLE,
            VkPipeline fallbackPipeline = VK_NULL_HANDLE,
            VkDescriptorSet fallbackDescriptorSet = VK_NULL_HANDLE) const
        {
            if (!mesh || !vertex.valid() || !index.valid())
                return;

            // 1. If a Material is assigned, use its Shader & Descriptors
            if (material && material->shader)
            {
                material->Bind(commandBuffer);
            }
            else if (fallbackPipeline != VK_NULL_HANDLE)
            {
                // Fallback to default engine pipeline
                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fallbackPipeline);
                
                if (fallbackDescriptorSet != VK_NULL_HANDLE)
                {
                    vkCmdBindDescriptorSets(
                        commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        fallbackLayout, 0, 1, &fallbackDescriptorSet, 0, nullptr);
                }
            }
            else
            {
                return; // No valid material or fallback pipeline
            }

            // 2. Bind Vertex and Index Buffers
            VkBuffer vertexBuffers[] = {vertex.buffer};
            VkDeviceSize offsets[] = {0};

            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, index.buffer, 0, VK_INDEX_TYPE_UINT16);

            // 3. Draw Geometry
            vkCmdDrawIndexed(
                commandBuffer,
                static_cast<uint32_t>(mesh->indices.size()),
                1, 0, 0, 0);
        }
    };
}

using MeshRenderer = OLIA_ENGINE::MeshRenderer;