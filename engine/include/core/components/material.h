#pragma once

#include "vulkan/pipeline.hpp"
#include "vulkan/utils.hpp"
#include "vulkan/vertex.hpp"
#include "graphics/type.hpp"

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace OLIA_ENGINE
{
    template <typename T>
    using Dict = std::unordered_map<std::string, T>;

    struct MaterialProperties
    {
        Dict<int> integers;
        Dict<float> floats;
        Dict<glm::vec2> vectors2;
        Dict<glm::vec3> vectors3;
        Dict<glm::vec4> vectors4;
        Dict<glm::mat2> matrices2;
        Dict<glm::mat3> matrices3;
        Dict<glm::mat4> matrices4;
    };

    struct Material
    {
        GraphicsPipeline *shader = nullptr;
        MaterialProperties properties;

        VkImageView textureView = VK_NULL_HANDLE;
        VkSampler textureSampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        void SetInt(const std::string &name, int val) { properties.integers[name] = val; }
        void SetFloat(const std::string &name, float val) { properties.floats[name] = val; }
        void SetVector2(const std::string &name, const glm::vec2 &val) { properties.vectors2[name] = val; }
        void SetVector3(const std::string &name, const glm::vec3 &val) { properties.vectors3[name] = val; }
        void SetVector4(const std::string &name, const glm::vec4 &val) { properties.vectors4[name] = val; }

        int GetInt(const std::string &name, int defaultValue = 0) const
        {
            auto it = properties.integers.find(name);
            return (it != properties.integers.end()) ? it->second : defaultValue;
        }

        float GetFloat(const std::string &name, float defaultValue = 0.0f) const
        {
            auto it = properties.floats.find(name);
            return (it != properties.floats.end()) ? it->second : defaultValue;
        }

        glm::vec4 GetVector4(const std::string &name, const glm::vec4 &defaultValue = glm::vec4(1.0f)) const
        {
            auto it = properties.vectors4.find(name);
            return (it != properties.vectors4.end()) ? it->second : defaultValue;
        }

        glm::vec2 GetVector2(const std::string &name, const glm::vec2 &defaultValue = glm::vec2(1.0f)) const
        {
            auto it = properties.vectors2.find(name);
            return (it != properties.vectors2.end()) ? it->second : defaultValue;
        }

        // Packages the Model Matrix + Material Properties into Push Constants
        MeshPushConstants GetMeshPushConstants(const glm::mat4 &modelMatrix) const
        {
            MeshPushConstants push{};
            push.model = modelMatrix;
            push.baseColor = GetVector4("_Color", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
            push.tiling = GetVector2("_Tiling", glm::vec2(1.0f, 1.0f));
            push.offset = GetVector2("_Offset", glm::vec2(0.0f, 0.0f));
            push.roughness = GetFloat("_Roughness", 0.5f);
            push.metallic = GetFloat("_Metallic", 0.0f);
            return push;
        }

        void Bind(VkCommandBuffer commandBuffer) const
        {
            if (!shader || shader->pipeline == VK_NULL_HANDLE)
                return;

            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);

            if (descriptorSet != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(
                    commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    shader->layout, 0, 1, &descriptorSet, 0, nullptr);
            }
        }

        static Material CreateErrorMaterial(GraphicsPipeline *errorShader, VkDescriptorSet descriptorSet)
        {
            Material errMat;
            errMat.shader = errorShader;
            errMat.descriptorSet = descriptorSet;
            errMat.SetVector4("_Color", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)); // Magenta
            return errMat;
        }
    };
} // namespace OLIA_ENGINE

using Material = OLIA_ENGINE::Material;