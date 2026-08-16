#include "vulkan/vk_context.h"
#include "vulkan/debugger.hpp"
#include "vulkan/device.hpp"
#include "vulkan/swapchain.hpp"
#include "vulkan/pipeline.hpp"
#include "vulkan/render_pass.hpp"
#include "vulkan/framebuffer.hpp"
#include "vulkan/descriptors.hpp"
#include "vulkan/buffers.hpp"
#include "vulkan/textures.hpp"
#include "vulkan/commands.hpp"
#include "vulkan/post_process.hpp"
#include "vulkan/utils.hpp"
#include "vulkan/vertex.hpp"
#include "graphics/type.hpp"

#include "core/components/mesh-renderer.h"
#include "core/components/particle.h"

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>  // printf, fprintf
#include <stdlib.h> // abort
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <array>
#include <chrono>

namespace OLIA_ENGINE
{
    bool VK_Init(VKContext &ctx, SDL_Window &window)
    {
        std::printf("*vulkan:\n");

        if (!CreateInstance(ctx))
            return false;

        if (!CreateSurface(ctx, window))
            return false;

        if (!PickPhysicalDevice(ctx))
            return false;

        if (!CreateLogicalDevice(ctx))
            return false;

        ctx.msaaSamples = GetMaxUsableSampleCount(ctx.physicalDevice);

        // ---------------------------------------------------------
        // Swapchain
        // ---------------------------------------------------------

        if (!CreateSwapchain(ctx, window))
            return false;

        if (!GetSwapchainImages(ctx))
            return false;

        ctx.swapchainImageViews.resize(ctx.swapchainImages.size());

        for (size_t i = 0; i < ctx.swapchainImages.size(); ++i)
        {
            ctx.swapchainImageViews[i] = CreateImageView(
                ctx.device,
                ctx.swapchainImages[i],
                ctx.swapchainFormat,
                VK_IMAGE_ASPECT_COLOR_BIT,
                ctx.allocator.callbacks());
        }

        // ---------------------------------------------------------
        // Commands + synchronization
        // ---------------------------------------------------------

        if (!CreateCommandPoolAndBuffer(ctx))
            return false;

        if (!CreateSyncObjects(ctx))
            return false;

        // ---------------------------------------------------------
        // Render resources
        // ---------------------------------------------------------

        CreateColorResources(ctx);
        CreateDepthResources(ctx);
        CreateOffscreenResources(ctx);

        // ---------------------------------------------------------
        // Uniform buffers
        // ---------------------------------------------------------

        ctx.uniforms.buffers.resize(MAX_FRAMES_IN_FLIGHT);
        ctx.uniforms.memories.resize(MAX_FRAMES_IN_FLIGHT);
        ctx.uniforms.mappeds.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateUniformBuffer(
                ctx.device,
                ctx.physicalDevice,
                sizeof(CameraUBO),
                ctx.uniforms.buffers[i],
                ctx.uniforms.memories[i],
                &ctx.uniforms.mappeds[i],
                ctx.allocator.callbacks());
        }

        // ---------------------------------------------------------
        // Render passes & Framebuffers
        // ---------------------------------------------------------

        if (!CreateOffscreenRenderPass(ctx))
            return false;

        if (!CreateOffscreenFramebuffer(ctx))
            return false;

        if (!CreateRenderPass(ctx))
            return false;

        if (!CreateFramebuffers(ctx))
            return false;

        // ---------------------------------------------------------
        // Graphics pipelines
        // ---------------------------------------------------------

        ctx.graphics.resize(2);

        // Index 0: Mesh Graphics Pipeline
        ctx.graphics[0] = CreateGraphicsPipeline(
            "assets/shaders/compiled/main.vert.spv",
            "assets/shaders/compiled/main.frag.spv",
            ctx.swapchainExtent,
            ctx.device,
            ctx.allocator.callbacks(),
            ctx.offscreenRenderPass,
            ctx.msaaSamples,
            ctx.pipelineCache,
            sizeof(MeshPushConstants));

        // Index 1: Particle Graphics Pipeline
        ctx.graphics[1] = CreateParticlePipeline(
            "assets/shaders/compiled/particle.vert.spv",
            "assets/shaders/compiled/particle.frag.spv",
            ctx.swapchainExtent,
            ctx.device,
            ctx.allocator.callbacks(),
            ctx.offscreenRenderPass,
            ctx.msaaSamples,
            ctx.pipelineCache);

        ctx.graphics[1].descriptorSetLayout = ctx.graphics[0].descriptorSetLayout;

        // ---------------------------------------------------------
        // Post process
        // ---------------------------------------------------------

        CreatePostProcessDescriptors(ctx);

        if (!CreatePostProcessPipeline(ctx))
            return false;

        // ---------------------------------------------------------
        // Texture
        // ---------------------------------------------------------

        CreateTextureImage(ctx.device, ctx.physicalDevice, ctx.commandPool, ctx.graphicsQueue, ctx.uniforms, ctx.allocator.callbacks());
        CreateTextureImageView(ctx.device, ctx.uniforms, ctx.allocator.callbacks());
        CreateTextureSampler(ctx.device, ctx.physicalDevice, ctx.uniforms.textureSampler, ctx.allocator.callbacks());

        // ---------------------------------------------------------
        // Descriptors
        // ---------------------------------------------------------

        CreateDescriptorPool(ctx.device, &ctx.descriptorPool, ctx.allocator.callbacks());
        CreateDescriptorSets(ctx.device, ctx.graphics[0].descriptorSetLayout,
                             ctx.descriptorPool, ctx.descriptorSets, ctx.uniforms, ctx.allocator.callbacks());

        VK_InitImgui(ctx, window);
        return true;
    }

    ImGuiContext *GetImguiContext()
    {
        return ImGui::GetCurrentContext();
    }

    void VK_NewFrameImgui()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void VK_EndFrameImgui()
    {
        ImGui::Render();
    }

    void VK_RenderImgui(VKContext &ctx)
    {
        ImDrawData *draw_data = ImGui::GetDrawData();
        if (draw_data && draw_data->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(draw_data, ctx.commandBuffers[ctx.currentFrame]);
        }
    }

    void VK_InitImgui(VKContext &ctx, SDL_Window &window)
    {
        // 1. Create a dedicated Descriptor Pool for ImGui
        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000}};

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;

        if (vkCreateDescriptorPool(ctx.device, &pool_info, ctx.allocator.callbacks(), &ctx.imguiPool) != VK_SUCCESS)
        {
            std::printf("[ImGui] Failed to create descriptor pool!\n");
            return;
        }

        // 2. Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls

        // Uncomment the line below ONLY if using the 'docking' branch of Dear ImGui:
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        // 3. Setup Platform (SDL3) backend
        ImGui_ImplSDL3_InitForVulkan(&window);

        // 4. Setup Renderer (Vulkan) backend
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = ctx.instance;
        init_info.PhysicalDevice = ctx.physicalDevice;
        init_info.Device = ctx.device;
        init_info.QueueFamily = ctx.graphicsQueueFamily;
        init_info.Queue = ctx.graphicsQueue;
        init_info.PipelineCache = ctx.pipelineCache;
        init_info.DescriptorPool = ctx.imguiPool;

        // Pipeline settings moved under 'PipelineInfoMain' in newer ImGui versions:
        init_info.PipelineInfoMain.RenderPass = ctx.renderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
        init_info.ImageCount = static_cast<uint32_t>(ctx.swapchainImages.size());
        init_info.Allocator = ctx.allocator.callbacks();

        ImGui_ImplVulkan_Init(&init_info);
    }

    void VK_Setup(VKContext &ctx, World &world)
    {
        auto &meshes = world.pool_mut<MeshRenderer>();
        meshes.for_each_mut(
            [&](Entity entity, MeshRenderer &mesh)
            {
                mesh.Setup(
                    ctx.device,
                    ctx.physicalDevice,
                    ctx.commandPool,
                    ctx.graphicsQueue,
                    ctx.allocator.callbacks());
            });

        auto &particles = world.pool_mut<ParticleSystem>();
        particles.for_each_mut(
            [&](Entity entity, ParticleSystem &particle)
            {
                particle.Create(
                    ctx.device, ctx.physicalDevice,
                    ctx.commandPool, ctx.graphicsQueue,
                    ctx.pipelineCache, ctx.allocator.callbacks());
            });
    }

    bool VK_DrawFrame(VKContext &ctx, SDL_Window &window, World &world)
    {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(&window, &width, &height);
        if (width == 0 || height == 0 || (SDL_GetWindowFlags(&window) & SDL_WINDOW_MINIMIZED))
        {
            SDL_Delay(16);
            return true;
        }

        // Handle pending window resize before acquiring image
        if (ctx.resized)
        {
            ctx.resized = false;
            if (!RecreateSwapchain(ctx, window))
                return false;
            return true;
        }

        vkWaitForFences(ctx.device, 1, &ctx.inFlightFences[ctx.currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(
            ctx.device, ctx.swapchain, UINT64_MAX,
            ctx.imageAvailableSemaphores[ctx.currentFrame],
            VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain(ctx, window);
            return true;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            return false;
        }

        vkResetFences(ctx.device, 1, &ctx.inFlightFences[ctx.currentFrame]);
        vkResetCommandBuffer(ctx.commandBuffers[ctx.currentFrame], 0);

        // Begin Command Buffer Recording
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if (vkBeginCommandBuffer(ctx.commandBuffers[ctx.currentFrame], &beginInfo) != VK_SUCCESS)
        {
            return false;
        }

        // Calculate Delta Time and Total Time
        static auto lastTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        static float totalTime = 0.0f;
        totalTime += deltaTime;

        // ============================================================
        // COMPUTE PASS: Run Compute Shader on GPU
        // ============================================================
        auto &particles = world.pool_mut<ParticleSystem>();
        particles.for_each_mut(
            [&](Entity entity, ParticleSystem &particle)
            {
                particle.Dispatch(ctx.commandBuffers[ctx.currentFrame], deltaTime, totalTime);
            });

        // ============================================================
        // PASS 1: Render Cube + Particles Offscreen
        // ============================================================
        VkViewport viewport{0.0f, 0.0f, static_cast<float>(ctx.swapchainExtent.width), static_cast<float>(ctx.swapchainExtent.height), 0.0f, 1.0f};
        VkRect2D scissor{{0, 0}, ctx.swapchainExtent};

        VkRenderPassBeginInfo offscreenInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        offscreenInfo.renderPass = ctx.offscreenRenderPass;
        offscreenInfo.framebuffer = ctx.offscreenFramebuffer;
        offscreenInfo.renderArea.extent = ctx.swapchainExtent;

        std::array<VkClearValue, 2> offscreenClears{};
        offscreenClears[0].color = {{0.05f, 0.05f, 0.08f, 1.0f}};
        offscreenClears[1].depthStencil = {1.0f, 0};
        offscreenInfo.clearValueCount = static_cast<uint32_t>(offscreenClears.size());
        offscreenInfo.pClearValues = offscreenClears.data();

        vkCmdBeginRenderPass(ctx.commandBuffers[ctx.currentFrame], &offscreenInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(ctx.commandBuffers[ctx.currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(ctx.commandBuffers[ctx.currentFrame], 0, 1, &scissor);

        // Safe evaluation of Camera and Transforms
        const auto *transforms = world.pool<Transform>();
        const auto *meshes = world.pool<MeshRenderer>();
        const auto *cameras = world.pool<Camera>();

        Camera defaultCamera{};
        Transform defaultCamTrans{.position = glm::vec3(0.0f, 0.0f, 3.0f)};

        const Camera *cameraPtr = nullptr;
        const Transform *camTransPtr = nullptr;

        if (cameras && !cameras->is_empty())
        {
            cameraPtr = &cameras->data()[0];
            Entity camEntity = cameras->dense()[0];
            if (transforms)
            {
                camTransPtr = transforms->get(camEntity);
            }
        }

        if (!cameraPtr)
            cameraPtr = &defaultCamera;
        if (!camTransPtr)
            camTransPtr = &defaultCamTrans;

        // Draw Meshes
        if (transforms && meshes)
        {
            // 1. Update Camera View/Projection UBO ONCE per frame
            UpdateUniformBuffer(ctx, *cameraPtr, *camTransPtr);

            Material errorMaterial = Material::CreateErrorMaterial(
                &ctx.graphics[0],
                ctx.descriptorSets[ctx.currentFrame]);

            meshes->for_each(
                [&](Entity entity, const MeshRenderer &mesh)
                {
                    const Transform *transform = transforms->get(entity);
                    if (!transform)
                        return;

                    const Material *mat = mesh.GetMaterial();
                    if (!mat || !mat->shader || mat->shader->pipeline == VK_NULL_HANDLE)
                    {
                        mat = &errorMaterial; // Draw as Magenta Error Mesh
                    }

                    // 2. Push per-object Model Matrix & Material Properties directly into Command Buffer
                    MeshPushConstants push = mat->GetMeshPushConstants(transform->getMatrix());
                    vkCmdPushConstants(
                        ctx.commandBuffers[ctx.currentFrame],
                        mat->shader->layout,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                        0, sizeof(MeshPushConstants),
                        &push);

                    // 3. Draw Geometry
                    mesh.Draw(
                        ctx.commandBuffers[ctx.currentFrame],
                        mat->shader->layout,
                        mat->shader->pipeline,
                        mat->descriptorSet);
                });
        }

        // Draw Particles
        particles.for_each_mut(
            [&](Entity entity, ParticleSystem &particle)
            {
                vkCmdBindPipeline(ctx.commandBuffers[ctx.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.graphics[1].pipeline);
                vkCmdBindDescriptorSets(
                    ctx.commandBuffers[ctx.currentFrame],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ctx.graphics[1].layout,
                    0, 1,
                    &ctx.descriptorSets[ctx.currentFrame],
                    0, nullptr);

                VkBuffer vertexBuffers[] = {particle.GetBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(ctx.commandBuffers[ctx.currentFrame], 0, 1, vertexBuffers, offsets);

                vkCmdDraw(ctx.commandBuffers[ctx.currentFrame], PARTICLE_COUNT, 1, 0, 0);
            });

        vkCmdEndRenderPass(ctx.commandBuffers[ctx.currentFrame]);

        // ============================================================
        // PASS 2: Post-Processing to Swapchain
        // ============================================================
        if (imageIndex >= ctx.swapchainFramebuffers.size())
        {
            return true;
        }

        VkRenderPassBeginInfo swapchainInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        swapchainInfo.renderPass = ctx.renderPass;
        swapchainInfo.framebuffer = ctx.swapchainFramebuffers[imageIndex];
        swapchainInfo.renderArea.extent = ctx.swapchainExtent;

        VkClearValue swapchainClear{{{0.0f, 0.0f, 0.0f, 1.0f}}};
        swapchainInfo.clearValueCount = 1;
        swapchainInfo.pClearValues = &swapchainClear;

        vkCmdBeginRenderPass(ctx.commandBuffers[ctx.currentFrame], &swapchainInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(ctx.commandBuffers[ctx.currentFrame], 0, 1, &viewport);
        vkCmdSetScissor(ctx.commandBuffers[ctx.currentFrame], 0, 1, &scissor);

        vkCmdBindPipeline(ctx.commandBuffers[ctx.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.postProcessPipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.commandBuffers[ctx.currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.postProcessPipeline.layout, 0, 1, &ctx.postProcessDescriptorSet, 0, nullptr);

        // Draw full-screen triangle
        vkCmdDraw(ctx.commandBuffers[ctx.currentFrame], 3, 1, 0, 0);

        // ------------------------------------------------------------
        // RENDER IMGUI UI HERE
        // ------------------------------------------------------------
        VK_RenderImgui(ctx);

        vkCmdEndRenderPass(ctx.commandBuffers[ctx.currentFrame]);

        if (vkEndCommandBuffer(ctx.commandBuffers[ctx.currentFrame]) != VK_SUCCESS)
            return false;

        // Submit and Present
        VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        VkSemaphore waitSemaphores[] = {ctx.imageAvailableSemaphores[ctx.currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &ctx.commandBuffers[ctx.currentFrame];

        VkSemaphore signalSemaphores[] = {ctx.renderFinishedSemaphores[ctx.currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, ctx.inFlightFences[ctx.currentFrame]) != VK_SUCCESS)
            return false;

        VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {ctx.swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(ctx.presentQueue, &presentInfo);
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || ctx.resized)
        {
            ctx.resized = false;
            RecreateSwapchain(ctx, window);
        }

        ctx.currentFrame = (ctx.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return true;
    }

    void UpdateUniformBuffer(VKContext &ctx, const Camera &camera, const Transform &camTrans)
    {
        CameraUBO ubo{};
        ubo.view = camera.GetView(camTrans);
        ubo.proj = camera.GetProjection();
        ubo.proj[1][1] *= -1; // Vulkan Y-axis inversion

        memcpy(ctx.uniforms.mappeds[ctx.currentFrame], &ubo, sizeof(ubo));
    }

    void CleanupSwapchain(VKContext &ctx)
    {
        DestroyColorResources(ctx);
        DestroyDepthResources(ctx);
        DestroyOffscreenResources(ctx);

        for (auto framebuffer : ctx.swapchainFramebuffers)
            if (framebuffer)
                vkDestroyFramebuffer(ctx.device, framebuffer, ctx.allocator.callbacks());
        ctx.swapchainFramebuffers.clear();

        DestroySwapchain(ctx);
    }

    bool RecreateSwapchain(VKContext &ctx, SDL_Window &window)
    {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(&window, &width, &height);

        while (width == 0 || height == 0 || (SDL_GetWindowFlags(&window) & SDL_WINDOW_MINIMIZED))
        {
            SDL_GetWindowSizeInPixels(&window, &width, &height);
            SDL_WaitEvent(nullptr);
        }

        vkDeviceWaitIdle(ctx.device);
        CleanupSwapchain(ctx);

        if (!CreateSwapchain(ctx, window) || !GetSwapchainImages(ctx))
            return false;

        ctx.swapchainImageViews.resize(ctx.swapchainImages.size());
        for (size_t i = 0; i < ctx.swapchainImages.size(); i++)
            ctx.swapchainImageViews[i] = CreateImageView(ctx.device, ctx.swapchainImages[i], ctx.swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, ctx.allocator.callbacks());

        CreateColorResources(ctx);
        CreateDepthResources(ctx);
        CreateOffscreenResources(ctx);
        if (!CreateOffscreenFramebuffer(ctx))
            return false;

        VkDescriptorImageInfo imageInfo{ctx.offscreenSampler, ctx.offscreenColorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ctx.postProcessDescriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo};
        vkUpdateDescriptorSets(ctx.device, 1, &write, 0, nullptr);

        return CreateFramebuffers(ctx);
    }

    void VK_Clean(VKContext &ctx, World &world)
    {
        if (ctx.device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(ctx.device);

        // ---------------------------------------------------------
        // Shutdown ImGui
        // ---------------------------------------------------------
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        if (ctx.imguiPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(ctx.device, ctx.imguiPool, ctx.allocator.callbacks());
            ctx.imguiPool = VK_NULL_HANDLE;
        }

        // ---------------------------------------------------------
        // Engine cleanup...
        // ---------------------------------------------------------
        auto &particles = world.pool_mut<ParticleSystem>();
        particles.for_each_mut(
            [&](Entity entity, ParticleSystem &particle)
            {
                particle.Destroy(ctx.device, ctx.allocator.callbacks());
            });

        DestroyPostProcessPipeline(ctx);
        if (ctx.offscreenRenderPass)
            vkDestroyRenderPass(ctx.device, ctx.offscreenRenderPass, ctx.allocator.callbacks());

        vkDestroySampler(ctx.device, ctx.uniforms.textureSampler, ctx.allocator.callbacks());
        vkDestroyImageView(ctx.device, ctx.uniforms.textureImageView, ctx.allocator.callbacks());
        vkDestroyImage(ctx.device, ctx.uniforms.textureImage, ctx.allocator.callbacks());
        vkFreeMemory(ctx.device, ctx.uniforms.textureImageMemory, ctx.allocator.callbacks());

        if (ctx.descriptorPool)
            vkDestroyDescriptorPool(ctx.device, ctx.descriptorPool, ctx.allocator.callbacks());

        auto &meshes = world.pool_mut<MeshRenderer>();
        meshes.for_each_mut(
            [&](Entity entity, MeshRenderer &mesh)
            {
                mesh.Destroy(
                    ctx.device,
                    ctx.allocator.callbacks());
            });

        if (ctx.pipelineCache)
            vkDestroyPipelineCache(ctx.device, ctx.pipelineCache, ctx.allocator.callbacks());

        DestroyGraphicsPipeline(ctx.graphics[1], ctx.device, ctx.allocator.callbacks());
        DestroyGraphicsPipeline(ctx.graphics[0], ctx.device, ctx.allocator.callbacks());

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkFreeCommandBuffers(ctx.device, ctx.commandPool, 1, &ctx.commandBuffers[i]);
            vkDestroySemaphore(ctx.device, ctx.renderFinishedSemaphores[i], ctx.allocator.callbacks());
            vkDestroySemaphore(ctx.device, ctx.imageAvailableSemaphores[i], ctx.allocator.callbacks());
            vkDestroyFence(ctx.device, ctx.inFlightFences[i], ctx.allocator.callbacks());
        }

        if (ctx.commandPool)
            vkDestroyCommandPool(ctx.device, ctx.commandPool, ctx.allocator.callbacks());

        CleanupSwapchain(ctx);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(ctx.device, ctx.uniforms.buffers[i], ctx.allocator.callbacks());
            vkFreeMemory(ctx.device, ctx.uniforms.memories[i], ctx.allocator.callbacks());
        }

        vkDestroyDescriptorSetLayout(ctx.device, ctx.graphics[1].descriptorSetLayout, ctx.allocator.callbacks());
        vkDestroyDescriptorSetLayout(ctx.device, ctx.graphics[0].descriptorSetLayout, ctx.allocator.callbacks());
        if (ctx.renderPass)
            vkDestroyRenderPass(ctx.device, ctx.renderPass, ctx.allocator.callbacks());

        if (ctx.device)
            vkDestroyDevice(ctx.device, ctx.allocator.callbacks());
        if (ctx.surface)
            SDL_Vulkan_DestroySurface(ctx.instance, ctx.surface, ctx.allocator.callbacks());
        if (ctx.debugMessenger)
            VK_DebuggerClean(ctx.instance, ctx.debugMessenger, ctx.allocator.callbacks());
        if (ctx.instance)
            vkDestroyInstance(ctx.instance, ctx.allocator.callbacks());
    }

    bool CreateInstance(VKContext &ctx)
    {
        bool enableValidationLayers = checkValidationLayerSupport();

        if (!enableValidationLayers)
        {
            std::printf("Warning: Vulkan validation layers are not available.\n");
        }

        uint32_t instanceExtensionCount = 0;
        const char *const *instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&instanceExtensionCount);

        if (instanceExtensions == nullptr)
        {
            std::printf("SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
            return false;
        }

        std::vector<const char *> extensions;
        for (uint32_t i = 0; i < instanceExtensionCount; ++i)
        {
            extensions.push_back(instanceExtensions[i]);
        }

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "olia";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "olia";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
        }

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            debugCreateInfo = VK_DebuggerCreateInfo();
            createInfo.pNext = &debugCreateInfo;
        }

        VkResult result = vkCreateInstance(&createInfo, ctx.allocator.callbacks(), &ctx.instance);
        if (result != VK_SUCCESS)
        {
            std::printf("[VULKAN] vkCreateInstance failed: %d\n", static_cast<int>(result));
            return false;
        }

        std::printf(" - instance\n");

        if (enableValidationLayers)
        {
            if (!VK_DebuggerInit(ctx.instance, ctx.debugMessenger, ctx.allocator.callbacks()))
            {
                std::printf("[VULKAN] VK_DebuggerInit failed!\n");
                vkDestroyInstance(ctx.instance, ctx.allocator.callbacks());
                ctx.instance = VK_NULL_HANDLE;
                return false;
            }
        }

        std::printf(" - debugger\n");
        return true;
    }

    bool CreateSurface(VKContext &ctx, SDL_Window &window)
    {
        if (!SDL_Vulkan_CreateSurface(&window, ctx.instance, ctx.allocator.callbacks(), &ctx.surface))
        {
            std::printf("Failed to create Vulkan surface: %s\n", SDL_GetError());

            if (ctx.debugMessenger != VK_NULL_HANDLE)
            {
                VK_DebuggerClean(ctx.instance, ctx.debugMessenger, ctx.allocator.callbacks());
                ctx.debugMessenger = VK_NULL_HANDLE;
            }

            vkDestroyInstance(ctx.instance, ctx.allocator.callbacks());
            ctx.instance = VK_NULL_HANDLE;

            return false;
        }

        std::printf(" - surface\n");
        return true;
    }
}