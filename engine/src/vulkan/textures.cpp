#include "vulkan/textures.hpp"
#include "vulkan/buffers.hpp"
#include "vulkan/utils.hpp"
#include <stb/stb_image.h>
#include <stdexcept>
#include <cstring>

namespace OLIA_ENGINE
{
    VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSampleCountFlags counts = props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_8_BIT)
            return VK_SAMPLE_COUNT_8_BIT;
        if (counts & VK_SAMPLE_COUNT_4_BIT)
            return VK_SAMPLE_COUNT_4_BIT;
        if (counts & VK_SAMPLE_COUNT_2_BIT)
            return VK_SAMPLE_COUNT_2_BIT;

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
    {
        std::vector<VkFormat> candidates = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                return format;
        }
        throw std::runtime_error("failed to find supported depth format!");
    }

    void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory, VkSampleCountFlagBits numSamples, const VkAllocationCallbacks *allocator)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = numSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, allocator, &image) != VK_SUCCESS)
            throw std::runtime_error("failed to create image!");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, image, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, allocator, &imageMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate image memory!");

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, const VkAllocationCallbacks *allocator)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, allocator, &imageView) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view!");

        return imageView;
    }

    void CreateColorResources(VKContext &ctx)
    {
        CreateImage(ctx.device, ctx.physicalDevice, ctx.swapchainExtent.width, ctx.swapchainExtent.height, ctx.swapchainFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ctx.colorImage, ctx.colorImageMemory, ctx.msaaSamples, ctx.allocator.callbacks());
        ctx.colorImageView = CreateImageView(ctx.device, ctx.colorImage, ctx.swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, ctx.allocator.callbacks());
    }

    void DestroyColorResources(VKContext &ctx)
    {
        if (ctx.colorImageView)
        {
            vkDestroyImageView(ctx.device, ctx.colorImageView, ctx.allocator.callbacks());
            ctx.colorImageView = VK_NULL_HANDLE;
        }
        if (ctx.colorImage)
        {
            vkDestroyImage(ctx.device, ctx.colorImage, ctx.allocator.callbacks());
            ctx.colorImage = VK_NULL_HANDLE;
        }
        if (ctx.colorImageMemory)
        {
            vkFreeMemory(ctx.device, ctx.colorImageMemory, ctx.allocator.callbacks());
            ctx.colorImageMemory = VK_NULL_HANDLE;
        }
    }

    void CreateDepthResources(VKContext &ctx)
    {
        ctx.depthFormat = FindDepthFormat(ctx.physicalDevice);
        CreateImage(ctx.device, ctx.physicalDevice, ctx.swapchainExtent.width, ctx.swapchainExtent.height, ctx.depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ctx.depthImage, ctx.depthImageMemory, ctx.msaaSamples, ctx.allocator.callbacks());
        ctx.depthImageView = CreateImageView(ctx.device, ctx.depthImage, ctx.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, ctx.allocator.callbacks());
    }

    void DestroyDepthResources(VKContext &ctx)
    {
        if (ctx.depthImageView)
        {
            vkDestroyImageView(ctx.device, ctx.depthImageView, ctx.allocator.callbacks());
            ctx.depthImageView = VK_NULL_HANDLE;
        }
        if (ctx.depthImage)
        {
            vkDestroyImage(ctx.device, ctx.depthImage, ctx.allocator.callbacks());
            ctx.depthImage = VK_NULL_HANDLE;
        }
        if (ctx.depthImageMemory)
        {
            vkFreeMemory(ctx.device, ctx.depthImageMemory, ctx.allocator.callbacks());
            ctx.depthImageMemory = VK_NULL_HANDLE;
        }
    }

    void CreateTextureImage(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, UniformBuffers &uniforms, const VkAllocationCallbacks *allocator)
    {
        stbi_set_flip_vertically_on_load(true);
        int width, height, channels;
        stbi_uc *pixels = stbi_load("assets/textures/profile.png", &width, &height, &channels, STBI_rgb_alpha);
        if (!pixels)
            throw std::runtime_error("failed to load texture image!");

        VkDeviceSize imageSize = width * height * 4;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(
            device, physicalDevice, imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory, allocator);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);
        stbi_image_free(pixels);

        CreateImage(
            device, physicalDevice,
            static_cast<uint32_t>(width), static_cast<uint32_t>(height),
            VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            uniforms.textureImage, uniforms.textureImageMemory, VK_SAMPLE_COUNT_1_BIT, allocator);

        transitionImageLayout(device, commandPool, graphicsQueue, uniforms.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(device, commandPool, graphicsQueue, stagingBuffer, uniforms.textureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        transitionImageLayout(device, commandPool, graphicsQueue, uniforms.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, allocator);
        vkFreeMemory(device, stagingBufferMemory, allocator);
    }

    void CreateTextureImageView(VkDevice device, UniformBuffers &uniforms, const VkAllocationCallbacks *allocator)
    {
        uniforms.textureImageView = CreateImageView(device, uniforms.textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, allocator);
    }

    void CreateTextureSampler(
        VkDevice device, VkPhysicalDevice physicalDevice,
        VkSampler &textureSampler, const VkAllocationCallbacks *allocator)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = props.limits.maxSamplerAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
        samplerInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(device, &samplerInfo, allocator, &textureSampler) != VK_SUCCESS)
            throw std::runtime_error("failed to create texture sampler!");
    }
}